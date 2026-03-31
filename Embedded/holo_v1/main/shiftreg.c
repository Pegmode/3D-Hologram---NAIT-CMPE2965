// shiftreg.c

#include "shiftreg.h"
#include "main.c"
#include <string.h>

#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"


// public config
shiftreg_config_t shiftreg_config = {
    .spi_host = -1,
    .pin_mosi = -1,
    .pin_sclk = -1,
    .pin_miso = -1,
    .pin_le = -1,
    .pin_oe = -1,
    .spi_clock_hz = 0,
    .max_transfer_bytes = 0
};

// --------------------------------------------------
// Private module state
// --------------------------------------------------

static const char *TAG_shiftreg = "shiftreg";

// SPI device handle
static spi_device_handle_t g_spi = NULL;

// DMA-capable transmit buffer
static uint8_t *g_tx_dma = NULL;

// Tracks whether init has already been done
static bool shiftreg_initialized = false;

// --------------------------------------------------
// Small helpers
// --------------------------------------------------

// Set a GPIO only if the pin number is valid
// safe GPIO set when pin may be disabled (-1)
static inline void shiftreg_gpio_set_if_valid(int pin, int level)
{
    if (pin >= 0) {
        gpio_set_level(pin, level);
    }
}

// Configure one GPIO as output
static esp_err_t shiftreg_config_output_pin(int pin)
{
    if (pin < 0) {
        return ESP_OK;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&cfg);
}


// --------------------------------------------------
// Public API
// --------------------------------------------------

esp_err_t shiftreg_init()
{

    if (shiftreg_config.pin_mosi < 0 || shiftreg_config.pin_sclk < 0 || shiftreg_config.pin_le < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (shiftreg_config.max_transfer_bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (shiftreg_initialized) {
        ESP_LOGW(TAG_shiftreg, "shiftreg_init called more than once");
        return ESP_OK;
    }

    // ------------------------------
    // Configure LE / OE / DBG pins
    // ------------------------------
    ESP_RETURN_ON_ERROR(shiftreg_config_output_pin(shiftreg_config.pin_le), TAG_shiftreg, "LE pin config failed");
    ESP_RETURN_ON_ERROR(shiftreg_config_output_pin(shiftreg_config.pin_oe), TAG_shiftreg, "OE pin config failed");

    // Safe startup states:
    // LE low = do not latch
    // OE high = outputs disabled (OE is active LOW)
  
    gpio_set_level(shiftreg_config.pin_le, 0);
    shiftreg_gpio_set_if_valid(shiftreg_config.pin_oe, 1);

    // ------------------------------
    // Configure SPI bus
    // ------------------------------
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = shiftreg_config.pin_mosi,
        .miso_io_num = shiftreg_config.pin_miso,
        .sclk_io_num = shiftreg_config.pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)shiftreg_config.max_transfer_bytes
    };

    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(shiftreg_config.spi_host, &bus_cfg, SPI_DMA_CH_AUTO),
        TAG_shiftreg,
        "spi_bus_initialize failed"
    );

    // ------------------------------
    // Configure SPI device
    // No hardware chip-select is used.
    // The shift register chain only needs MOSI + SCLK.
    // ------------------------------
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = shiftreg_config.spi_clock_hz,
        .mode = 0,            // CPOL=0, CPHA=0
        .spics_io_num = -1,   // no CS pin
        .queue_size = 1,
        .flags = 0
    };

    ESP_RETURN_ON_ERROR(
        spi_bus_add_device(shiftreg_config.spi_host, &dev_cfg, &g_spi),
        TAG_shiftreg,
        "spi_bus_add_device failed"
    );

    // ------------------------------
    // Allocate DMA-capable TX buffer
    //
    // We copy user frame data into this buffer before transmit.
    // This keeps the module simple and avoids problems if the
    // original frame lives in PSRAM later.
    // ------------------------------
    g_tx_dma = (uint8_t *)heap_caps_malloc(shiftreg_config.max_transfer_bytes, MALLOC_CAP_DMA);
    if (g_tx_dma == NULL) {
        ESP_LOGE(TAG_shiftreg, "Failed to allocate DMA buffer");
        return ESP_ERR_NO_MEM;
    }

    memset(g_tx_dma, 0, shiftreg_config.max_transfer_bytes);

    shiftreg_initialized = true;

    ESP_LOGI(TAG_shiftreg, "Shift register initialized");
    ESP_LOGI(TAG_shiftreg, "SPI host=%d, MOSI=%d, SCLK=%d, LE=%d, OE=%d, DBG=%d",
             shiftreg_config.spi_host,
             shiftreg_config.pin_mosi,
             shiftreg_config.pin_sclk,
             shiftreg_config.pin_le,
             shiftreg_config.pin_oe);

    return ESP_OK;
}

esp_err_t shiftreg_set_output_enabled(bool enable)
{
    if (!shiftreg_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // If OE is not connected, there is nothing to do
    if (shiftreg_config.pin_oe < 0) {
        return ESP_OK;
    }

    // OE is active LOW:
    // enable=true  -> drive LOW
    // enable=false -> drive HIGH
    gpio_set_level(shiftreg_config.pin_oe, enable ? 0 : 1);

    return ESP_OK;
}

esp_err_t shiftreg_latch(void)
{
    if (!shiftreg_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Pulse LE high briefly
    gpio_set_level(shiftreg_config.pin_le, 1);
    esp_rom_delay_us(1);
    gpio_set_level(shiftreg_config.pin_le, 0);

    return ESP_OK;
}

esp_err_t shiftreg_send_frame(const uint8_t *frame, size_t frame_len_bytes)
{
    if (!shiftreg_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_len_bytes == 0 || frame_len_bytes > shiftreg_config.max_transfer_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    // Copy user data into the DMA buffer
    memcpy(g_tx_dma, frame, frame_len_bytes);

    // Build the SPI transaction
    spi_transaction_t trans = {
        .length = frame_len_bytes * 8,   // SPI length is in bits
        .tx_buffer = g_tx_dma
    };

    // Optional debug pin high during transmit + latch
    // shiftreg_dbg_high();

    // 1) Shift out the bits
    ESP_RETURN_ON_ERROR(
        spi_device_polling_transmit(g_spi, &trans),
        TAG_shiftreg,
        "spi_device_polling_transmit failed"
    );

    // 2) Blank outputs briefly while latching, if OE exists
    if (shiftreg_config.pin_oe >= 0) {
        gpio_set_level(shiftreg_config.pin_oe, 1);
    }

    // 3) Latch the new output data
    ESP_RETURN_ON_ERROR(shiftreg_latch(), TAG_shiftreg, "shiftreg_latch failed");

    // 4) Re-enable outputs
    if (shiftreg_config.pin_oe >= 0) {
        gpio_set_level(shiftreg_config.pin_oe, 0);
    }

    // shiftreg_dbg_low();

    return ESP_OK;
}

bool shiftreg_is_initialized(void)
{
    return shiftreg_initialized;
}

// One "walking 1" bit across the 512-bit stream.
// This is extremely useful later to confirm bit order / chain direction.
void test_pattern_walking_one()
{
    uint8_t test_frame[SR_FRAME_BYTES] = {0};
	test_frame[0] = 1;
	int bit_index_0_to_511 = 0;
    int byte_index;
	int bit_in_byte;

	while(1){
		// bit_index 0 means "first bit clocked out"
    	// byte_index 0 holds the first 8 bits clocked out.
    	byte_index = bit_index_0_to_511 / 8;
    	bit_in_byte = bit_index_0_to_511 % 8;

    	// Choose MSB-first within each byte so the first bit is bit 7.
    	// If your observed behavior is reversed later, you’ll flip this.
    	test_frame[byte_index] = (uint8_t)(1U << (7 - bit_in_byte));

		shiftreg_send_frame(test_frame, SR_FRAME_BYTES);
		ESP_LOGI(TAG_shiftreg, "Sent: test_rame");
		vTaskDelay(pdMS_TO_TICKS(TEST_SEND_PERIOD_MS));
		bit_index_0_to_511++;
	}
}

// -----------------------
// Task: repeatedly send a known frame at a slow rate
// -----------------------
void test_shiftreg_dummy_task(void *)
{
    

    //uint8_t frame[SR_FRAME_BYTES];

	// 512 bits = 64 bytes. Byte 0 is the FIRST byte shifted out on MOSI.
	static const uint8_t test_frame[64] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
	};

    // Start with outputs enabled (if OE exists)
    gpio_set_level(PIN_SR_NOT_OE, 0);
	

    int walk = 0;

    while (1) {
        // Cycle through a few patterns so you can see changes in the waveforms

		
		shiftreg_send_frame(test_frame, SR_FRAME_BYTES);
		ESP_LOGI(TAG_shiftreg, "Sent: test_rame");
		vTaskDelay(pdMS_TO_TICKS(TEST_SEND_PERIOD_MS));
		


        // pattern_all_off(frame);
        // shiftreg_send_frame(frame);
        // ESP_LOGI(TAG_shiftreg, "Sent: ALL OFF");
        // vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));

        // pattern_all_on(frame);
        // shiftreg_send_frame(frame);
        // ESP_LOGI(TAG_shiftreg, "Sent: ALL ON");
        // vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));

        // pattern_walking_one(frame, walk);
        // shiftreg_send_frame(frame);
        // ESP_LOGI(TAG_shiftreg, "Sent: WALKING 1 (bit %d)", walk);
        // vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));

        // walk = (walk + 1) % 512;
    }
}