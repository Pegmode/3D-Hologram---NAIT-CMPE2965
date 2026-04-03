#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us()
#include "encoder.h"
#include "shiftreg.h"
#include "pwm.h"
#include "console_io.h"
#include "display_task.h"
#include "main.h"


static const char *TAG_main = "main_test";
// -----------------------
// app_main: init and start the test task
// -----------------------
void app_main(void)
{
    esp_err_t err;

	vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG_main, "Shift-register SPI test starting...");

	//config shiftreg
	shiftreg_config.init				= 1;
	shiftreg_config.spi_host 			= SR_SPI_HOST;
	shiftreg_config.pin_mosi 			= PIN_SPI_MOSI;
	shiftreg_config.pin_sclk 			= PIN_SPI_SCLK;
	shiftreg_config.pin_miso 			= PIN_SPI_MISO;
	shiftreg_config.pin_le 				= PIN_SR_LE;
	shiftreg_config.pin_oe 				= PIN_SR_NOT_OE;
	shiftreg_config.spi_clock_hz 		= SR_SPI_HZ;
	shiftreg_config.max_transfer_bytes 	= SR_FRAME_BYTES;
	
	//config pwm
	pwm_config.init			   	= 1;
	pwm_config.gpio_num        	= PIN_ESC_PWM;
	pwm_config.channel         	= LEDC_CHANNEL_0;
	pwm_config.timer           	= LEDC_TIMER_0;
	pwm_config.arm_pulse_us    	= PWM_ESC_ARM_US;
	pwm_config.arm_time_ms     	= PWM_ESC_ARM_TIME_MS;
	pwm_config.command_min_us  	= PWM_ESC_MIN_US;
	pwm_config.command_max_us  	= PWM_ESC_MAX_US;
    
	//config encoder
	encoder_config.init 		= 1;
	encoder_config.pin_a  		= PIN_ENC_A;
	encoder_config.pin_b		= PIN_ENC_B;
	encoder_config.pin_z		= PIN_ENC_Z;
	encoder_config.ab_pull 		= GPIO_FLOATING;
	encoder_config.z_pull		= GPIO_FLOATING;


    err = shiftreg_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "shiftreg_init failed: %s", esp_err_to_name(err));
        return;
    }

    err = pwm_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "pwm_init failed: %s", esp_err_to_name(err));
        return;
    }

    err = console_io_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "console_io_init failed: %s", esp_err_to_name(err));
        return;
    }

    err = encoder_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "encoder_init failed: %s", esp_err_to_name(err));
        return;
    }

    // Start the example display task after all hardware drivers are ready.
    err = display_task_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "display_task_start failed: %s", esp_err_to_name(err));
    }
}
