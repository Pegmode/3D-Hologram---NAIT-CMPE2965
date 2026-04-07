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
#include "display_store.h"
#include "display_task.h"
#include "main.h"
#include "wifi_rx.h"


static const char *TAG_main = "main_test";
// -----------------------
// app_main: init and start the test task
// -----------------------
void app_main(void)
{
    esp_err_t err;

	vTaskDelay(pdMS_TO_TICKS(1000));

    //ESP_LOGI(TAG_main, "Shift-register SPI test starting...");

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
	encoder_config.glitch_filter_ns = 1000;

	// Configure Wi-Fi receiver in SoftAP mode for packet-header bring-up.
	//
	// Test model:
	// - ESP32 creates its own Wi-Fi network
	// - PC joins that network
	// - PC opens a TCP connection to the ESP32
	// - ESP32 prints each received packet header to the console
	wifi_rx_config.init					= 1;
	strcpy(wifi_rx_config.wifi_ssid, "holo_v1_test");
	strcpy(wifi_rx_config.wifi_password, "holo1234");
	wifi_rx_config.tcp_port			= 3333;
	wifi_rx_config.listen_backlog	= 1;
	wifi_rx_config.max_connections	= 1;
	wifi_rx_config.task_stack_size	= 4096;
	wifi_rx_config.task_priority	= 6;
	wifi_rx_config.print_headers_to_console = true;


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

    err = display_store_manager_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "display_store_manager_init failed: %s", esp_err_to_name(err));
        return;
    }

    // Start the Wi-Fi receive path after console I/O is ready so received
    // headers can be printed immediately during bring-up testing.
    err = wifi_rx_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "wifi_rx_init failed: %s", esp_err_to_name(err));
        return;
    }

    err = wifi_rx_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "wifi_rx_start failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG_main, "Wi-Fi test AP ready");
    ESP_LOGI(TAG_main, "SSID: %s", wifi_rx_config.wifi_ssid);
    ESP_LOGI(TAG_main, "Password: %s", wifi_rx_config.wifi_password);
    ESP_LOGI(TAG_main, "TCP port: %u", (unsigned)wifi_rx_config.tcp_port);

    // Start the example display task after all hardware drivers are ready.
    err = display_task_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_main, "display_task_start failed: %s", esp_err_to_name(err));
    }
}
