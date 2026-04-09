#include "buck_4v.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "main.h"

static const char *TAG_buck_4v = "buck_4v";

// Enable the LED buck regulator and wait until its power-good output asserts.
esp_err_t buck_4v_enable_and_wait(void)
{
    gpio_config_t buck_en_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUCK_4V_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config_t buck_pg_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUCK_4V_PG),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_LOGI(TAG_buck_4v, "Configuring buck enable pin");
    esp_err_t err = gpio_config(&buck_en_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_buck_4v, "buck enable pin config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG_buck_4v, "Configuring buck power-good pin");
    err = gpio_config(&buck_pg_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_buck_4v, "buck power-good pin config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG_buck_4v, "Driving buck 4 V enable high");
    gpio_set_level(PIN_BUCK_4V_EN, 1);

    // Wait until the regulator reports that the 4 V rail is ready.
    ESP_LOGI(TAG_buck_4v, "Waiting for buck power-good to go high");
    while (gpio_get_level(PIN_BUCK_4V_PG) == 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG_buck_4v, "Buck 4 V rail is ready");
    return ESP_OK;
}

// Disable the LED buck regulator by deasserting its enable pin.
esp_err_t buck_4v_disable(void)
{
    gpio_config_t buck_en_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUCK_4V_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_LOGI(TAG_buck_4v, "Configuring buck enable pin for disable");
    esp_err_t err = gpio_config(&buck_en_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_buck_4v, "buck enable pin config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG_buck_4v, "Driving buck enable low");
    gpio_set_level(PIN_BUCK_4V_EN, 0);

    ESP_LOGI(TAG_buck_4v, "Buck 4 V rail disable requested");
    return ESP_OK;
}
