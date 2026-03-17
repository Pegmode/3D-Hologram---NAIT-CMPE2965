#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us()


// -----------------------
// USER SETTINGS (edit these)
// -----------------------

// SPI pins (set these to whatever pins you wired to the shift-register chain)
#define PIN_SPI_MOSI    11
#define PIN_SPI_SCLK    12
#define PIN_SPI_MISO    -1   // not used

// Shift-register control pins
#define PIN_SR_LE       10   // latch enable (LE)
#define PIN_SR_OE        9   // output enable (OE) - active LOW; set -1 if not connected

// Optional debug pin (toggle around SPI transmit so you can measure timing)
#define PIN_DBG         8    // set -1 if not used

// SPI host selection (ESP32-S3 typically uses SPI2_HOST or SPI3_HOST)
#define SR_SPI_HOST     SPI2_HOST

// SPI clock rate (start lower for clean waveforms; increase later)
#define SR_SPI_HZ       (10 * 1000 * 1000)  // 10 MHz

// Our frame is 512 LEDs -> 512 bits -> 64 bytes
#define SR_FRAME_BYTES  64

// How often to send the frame in this test (later this will be encoder-triggered)
#define SEND_PERIOD_MS  1000

static const char *TAG = "sr_test";

// SPI device handle
static spi_device_handle_t g_spi = NULL;

// DMA-capable TX buffer
static uint8_t *g_tx_dma = NULL;

// -----------------------
// Small helper: safe GPIO set when pin may be disabled (-1)
// -----------------------
// static inline void gpio_set_level_if_valid(int pin, int level)
// {
//     if (pin >= 0) {
//         gpio_set_level(pin, level);
//     }
// }

// -----------------------
// Initialize GPIO pins for LE/OE/DBG
// -----------------------
static void shiftreg_gpio_init(void)
{
    // Configure LE pin as output
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_SR_LE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    // Optional OE pin as output (if connected)
    if (PIN_SR_OE >= 0) {
        gpio_config_t oe = io;
        oe.pin_bit_mask = (1ULL << PIN_SR_OE);
        ESP_ERROR_CHECK(gpio_config(&oe));
    }

    // Optional debug pin as output (if used)
    if (PIN_DBG >= 0) {
        gpio_config_t dbg = io;
        dbg.pin_bit_mask = (1ULL << PIN_DBG);
        ESP_ERROR_CHECK(gpio_config(&dbg));
        gpio_set_level(PIN_DBG, 0);
    }

    // Initial states:
    // LE low means "do not latch"
    gpio_set_level(PIN_SR_LE, 0);

    // OE is active LOW:
    // Start "disabled" (high) so you don't flash random data on boot.
    gpio_set_level_if_valid(PIN_SR_OE, 1);
}

// -----------------------
// Initialize SPI (master) with no chip-select
// -----------------------
static void shiftreg_spi_init(void)
{
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SR_FRAME_BYTES  // small transfers in this step
    };

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(SR_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Configure the SPI device interface
    // - spics_io_num = -1 means we do NOT use a hardware CS pin.
    // - mode = 0 is typical for shift registers (CPOL=0, CPHA=0).
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SR_SPI_HZ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
        .flags = 0
    };

    // Attach the device to the SPI bus
    ESP_ERROR_CHECK(spi_bus_add_device(SR_SPI_HOST, &devcfg, &g_spi));

    // Allocate a DMA-capable TX buffer (important for reliable SPI transfers)
    g_tx_dma = (uint8_t *)heap_caps_malloc(SR_FRAME_BYTES, MALLOC_CAP_DMA);
    if (!g_tx_dma) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        abort();
    }
    memset(g_tx_dma, 0, SR_FRAME_BYTES);
}

// -----------------------
// Latch the shifted data into outputs
// -----------------------
static void shiftreg_latch_pulse(void)
{
    // Pulse LE high briefly then low.
    // Use a short delay to ensure the latch sees the pulse.
    gpio_set_level(PIN_SR_LE, 1);
    esp_rom_delay_us(1);
    gpio_set_level(PIN_SR_LE, 0);
}

// -----------------------
// Send one 512-bit frame:
// 1) Shift bits out over SPI
// 2) (Optional) blank outputs (OE high)
// 3) Pulse LE to latch
// 4) (Optional) enable outputs (OE low)
// -----------------------
static void shiftreg_send_frame(const uint8_t *frame64)
{
    // Copy into DMA buffer (keeps later design flexible when data lives in PSRAM)
    memcpy(g_tx_dma, frame64, SR_FRAME_BYTES);

    // Prepare SPI transaction
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = SR_FRAME_BYTES * 8; // in bits
    t.tx_buffer = g_tx_dma;

    // Optional debug pin: high during SPI transfer
    gpio_set_level_if_valid(PIN_DBG, 1);

    // Shift out the data
    ESP_ERROR_CHECK(spi_device_polling_transmit(g_spi, &t));

    gpio_set_level_if_valid(PIN_DBG, 0);

    // Blank outputs *briefly* while latching to prevent any visible glitch
    gpio_set_level_if_valid(PIN_SR_OE, 1);

    // Latch new data into outputs
    shiftreg_latch_pulse();

    // Enable outputs (active LOW)
    gpio_set_level_if_valid(PIN_SR_OE, 0);
	
	//ESP_LOGI(TAG, "send frame done");

}

// -----------------------
// Simple test patterns for bring-up
// -----------------------
static void pattern_all_off(uint8_t *out64)
{
    memset(out64, 0x00, SR_FRAME_BYTES);
}

static void pattern_all_on(uint8_t *out64)
{
    memset(out64, 0xFF, SR_FRAME_BYTES);
}

// One "walking 1" bit across the 512-bit stream.
// This is extremely useful later to confirm bit order / chain direction.
static void pattern_walking_one(uint8_t *out64, int bit_index_0_to_511)
{
    memset(out64, 0x00, SR_FRAME_BYTES);

    // bit_index 0 means "first bit clocked out"
    // byte_index 0 holds the first 8 bits clocked out.
    int byte_index = bit_index_0_to_511 / 8;
    int bit_in_byte = bit_index_0_to_511 % 8;

    // Choose MSB-first within each byte so the first bit is bit 7.
    // If your observed behavior is reversed later, you’ll flip this.
    out64[byte_index] = (uint8_t)(1U << (7 - bit_in_byte));
}

// -----------------------
// Task: repeatedly send a known frame at a slow rate
// -----------------------
static void shiftreg_test_task(void *arg)
{
    (void)arg;

    uint8_t frame[SR_FRAME_BYTES];

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
    gpio_set_level_if_valid(PIN_SR_OE, 0);

    int walk = 0;

    while (1) {
        // Cycle through a few patterns so you can see changes in the waveforms

		
		shiftreg_send_frame(test_frame);
		ESP_LOGI(TAG, "Sent: test_rame");
		vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));
		


        // pattern_all_off(frame);
        // shiftreg_send_frame(frame);
        // ESP_LOGI(TAG, "Sent: ALL OFF");
        // vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));

        // pattern_all_on(frame);
        // shiftreg_send_frame(frame);
        // ESP_LOGI(TAG, "Sent: ALL ON");
        // vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));

        // pattern_walking_one(frame, walk);
        // shiftreg_send_frame(frame);
        // ESP_LOGI(TAG, "Sent: WALKING 1 (bit %d)", walk);
        // vTaskDelay(pdMS_TO_TICKS(SEND_PERIOD_MS));

        // walk = (walk + 1) % 512;
    }
}

// -----------------------
// app_main: init and start the test task
// -----------------------
void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Shift-register SPI test starting...");

    shiftreg_gpio_init();
    shiftreg_spi_init();

    // Create a simple task; later you can pin it to a specific core and raise priority.
    xTaskCreate(shiftreg_test_task, "sr_test", 4096, NULL, 5, NULL);
}