#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us()
#include "encoder.h"
#include "shiftreg.h"
#include "pwm.h"
#include "console_io.h"

// -----------------------
// USER SETTINGS (edit these)
// -----------------------

// SPI pins (set these to whatever pins you wired to the shift-register chain)
#define PIN_SPI_MOSI    	39
#define PIN_SPI_SCLK    	40
#define PIN_SPI_MISO    	-1   // not used

// Shift-register control pins
#define PIN_SR_LE      		41   // latch enable (LE)
#define PIN_SR_NOT_OE      	42   // output enable (OE) - active LOW; set -1 if not connected

//led power
#define PIN_BUCK_4V_EN		9	//pull up to enable 4v buck converter
#define PIN_BUCK_4V_PG		10 	//goes high to signal buck converter is ready for use

//encoder pins
#define PIN_ENC_A			4
#define PIN_ENC_B			5
#define PIN_ENC_Z			6

//pwm
#define PIN_ESC_PWM			16

// Optional debug pins 
#define PIN_SW_OPT1         21    // set -1 if not used
#define PIN_SW_OPT2			14
#define PIN_LED_RED			11
#define PIN_LED_YELLOW		12
#define PIN_LED_BLUE		13

// SPI host selection (ESP32-S3 typically uses SPI2_HOST or SPI3_HOST)
#define SR_SPI_HOST     SPI2_HOST

// SPI clock rate (start lower for clean waveforms; increase later)
#define SR_SPI_HZ       (10 * 1000 * 1000)  // 10 MHz

// Our frame is 512 LEDs -> 512 bits -> 64 bytes
#define SR_FRAME_BYTES  64

// How often to send the frame in this test (later this will be encoder-triggered)
#define TEST_SEND_PERIOD_MS  1000

static const char *TAG_main = "main_test";



// -----------------------
// app_main: init and start the test task
// -----------------------
void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG_main, "Shift-register SPI test starting...");

	//config shiftreg
	shiftreg_config.spi_host 			= SR_SPI_HOST;
	shiftreg_config.pin_mosi 			= PIN_SPI_MOSI;
	shiftreg_config.pin_sclk 			= PIN_SPI_SCLK;
	shiftreg_config.pin_miso 			= PIN_SPI_MISO;
	shiftreg_config.pin_le 				= PIN_SR_LE;
	shiftreg_config.pin_oe 				= PIN_SR_NOT_OE;
	shiftreg_config.spi_clock_hz 		= SR_SPI_HZ;
	shiftreg_config.max_transfer_bytes 	= SR_FRAME_BYTES;
	esp_err_t err = shiftreg_init();
	if (err != ESP_OK) {
		ESP_LOGE(TAG_main, "shiftreg_init failed: %s", esp_err_to_name(err));
		return;
	}

	console_io_init();
	
	pwm_config.gpio_num        = PIN_ESC_PWM;
	pwm_config.channel         = LEDC_CHANNEL_0;
	pwm_config.timer           = LEDC_TIMER_0;
	pwm_config.arm_pulse_us    = PWM_ESC_ARM_US;
	pwm_config.arm_time_ms     = PWM_ESC_ARM_TIME_MS;
	pwm_config.command_min_us  = PWM_ESC_MIN_US;
	pwm_config.command_max_us  = PWM_ESC_MAX_US;
    


    // Create a simple task; later you can pin it to a specific core and raise priority.
    xTaskCreate(test_shiftreg_dummy_task, TAG_main, 4096, NULL, 5, NULL);
}