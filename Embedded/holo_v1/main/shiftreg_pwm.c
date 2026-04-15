#include "shiftreg_pwm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "main.h"

// ESP32-S3 uses LEDC low speed mode.
#define SHIFTREG_PWM_LEDC_MODE      LEDC_LOW_SPEED_MODE

// 10-bit resolution gives 1024 dimming steps while still allowing a fast PWM.
#define SHIFTREG_PWM_DUTY_BITS      10U
#define SHIFTREG_PWM_DUTY_RES       LEDC_TIMER_10_BIT

static const char *TAG_shiftreg_pwm = "shiftreg_pwm";

// Brightness is controlled in four fixed steps during bring-up so the display
// can be adjusted with two simple GPIO switches.
#define SHIFTREG_PWM_LEVEL_COUNT           4U
#define SHIFTREG_PWM_POLL_PERIOD_MS        20U
#define SHIFTREG_PWM_CONTROL_TASK_STACK    2048U
#define SHIFTREG_PWM_CONTROL_TASK_PRIO     4U

shiftreg_pwm_config_t shiftreg_pwm_config = {
    .init = -1,
    .gpio_num = -1,
    .channel = -1,
    .timer = -1,
    .frequency_hz = 0,
    .startup_brightness_percent = 0
};

static bool s_initialized = false;
static uint32_t s_current_brightness_percent = 0U;
static size_t s_current_level_index = 0U;
static TaskHandle_t s_control_task_handle = NULL;
static bool s_gate_enabled = true;

// Discrete brightness levels used by the two option switches.
static const uint32_t s_brightness_levels[SHIFTREG_PWM_LEVEL_COUNT] = {
    20U,
    40U,
    60U,
    100U
};

// Configure a GPIO as a simple digital output for the debug LEDs.
static esp_err_t shiftreg_pwm_config_output_pin(int pin)
{
    gpio_config_t cfg;

    if (pin < 0) {
        return ESP_OK;
    }

    cfg = (gpio_config_t) {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&cfg);
}

// Configure a GPIO as a pulled-up input for the brightness switches.
//
// This assumes each switch pulls the pin low when pressed.
static esp_err_t shiftreg_pwm_config_input_pin(int pin)
{
    gpio_config_t cfg;

    if (pin < 0) {
        return ESP_OK;
    }

    cfg = (gpio_config_t) {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&cfg);
}

static size_t shiftreg_pwm_find_level_index(uint32_t brightness_percent)
{
    size_t level_index;

    for (level_index = 0U; level_index < SHIFTREG_PWM_LEVEL_COUNT; level_index++) {
        if (s_brightness_levels[level_index] == brightness_percent) {
            return level_index;
        }
    }

    // Map any non-table brightness back to the closest supported indicator
    // level so the debug LEDs still show something sensible.
    if (brightness_percent <= s_brightness_levels[0]) {
        return 0U;
    }

    if (brightness_percent >= s_brightness_levels[SHIFTREG_PWM_LEVEL_COUNT - 1U]) {
        return SHIFTREG_PWM_LEVEL_COUNT - 1U;
    }

    for (level_index = 1U; level_index < SHIFTREG_PWM_LEVEL_COUNT; level_index++) {
        if (brightness_percent < s_brightness_levels[level_index]) {
            uint32_t lower = s_brightness_levels[level_index - 1U];
            uint32_t upper = s_brightness_levels[level_index];

            if ((brightness_percent - lower) <= (upper - brightness_percent)) {
                return level_index - 1U;
            }

            return level_index;
        }
    }

    return SHIFTREG_PWM_LEVEL_COUNT - 1U;
}

// Reflect the current brightness level on the three debug LEDs.
//
// 100% -> red
//  60% -> yellow
//  40% -> blue
//  20% -> all off
static void shiftreg_pwm_update_debug_leds(size_t level_index)
{
    if (PIN_LED_RED >= 0) {
        gpio_set_level(PIN_LED_RED, (level_index == 3U) ? 1 : 0);
    }

    if (PIN_LED_YELLOW >= 0) {
        gpio_set_level(PIN_LED_YELLOW, (level_index == 2U) ? 1 : 0);
    }

    if (PIN_LED_BLUE >= 0) {
        gpio_set_level(PIN_LED_BLUE, (level_index == 1U) ? 1 : 0);
    }
}

// Convert requested brightness into LEDC duty on an active-LOW OE pin.
//
// brightness 0%   -> OE stays high -> outputs always disabled
// brightness 100% -> OE stays low  -> outputs always enabled
static uint32_t shiftreg_pwm_brightness_to_duty(uint32_t brightness_percent)
{
    const uint32_t max_duty = (1U << SHIFTREG_PWM_DUTY_BITS) - 1U;
    uint32_t off_percent;
    uint64_t duty;

    if (brightness_percent >= 100U) {
        return 0U;
    }

    if (brightness_percent == 0U) {
        return max_duty;
    }

    off_percent = 100U - brightness_percent;
    duty = ((uint64_t)off_percent * max_duty + 50U) / 100U;

    return (uint32_t)duty;
}

// Program the LEDC channel with the exact visible brightness that should be
// applied to the OE pin right now.
static esp_err_t shiftreg_pwm_apply_effective_brightness(uint32_t brightness_percent)
{
    uint32_t duty;

    duty = shiftreg_pwm_brightness_to_duty(brightness_percent);

    ESP_RETURN_ON_ERROR(
        ledc_set_duty(SHIFTREG_PWM_LEDC_MODE, shiftreg_pwm_config.channel, duty),
        TAG_shiftreg_pwm,
        "ledc_set_duty failed"
    );

    ESP_RETURN_ON_ERROR(
        ledc_update_duty(SHIFTREG_PWM_LEDC_MODE, shiftreg_pwm_config.channel),
        TAG_shiftreg_pwm,
        "ledc_update_duty failed"
    );

    return ESP_OK;
}

// Reapply the saved base brightness after combining it with the current gate
// state. This lets the display task blank the output without losing the user's
// chosen brightness.
static esp_err_t shiftreg_pwm_apply_current_output(void)
{
    return shiftreg_pwm_apply_effective_brightness(s_gate_enabled ? s_current_brightness_percent : 0U);
}

// Update the saved base brightness and immediately refresh the effective output
// seen on the OE pin.
static esp_err_t shiftreg_pwm_set_base_brightness(uint32_t brightness_percent)
{
    s_current_brightness_percent = brightness_percent;
    s_current_level_index = shiftreg_pwm_find_level_index(brightness_percent);
    shiftreg_pwm_update_debug_leds(s_current_level_index);

    return shiftreg_pwm_apply_current_output();
}

// Apply one of the fixed switch-selected brightness steps.
static esp_err_t shiftreg_pwm_apply_level_index(size_t level_index)
{
    if (level_index >= SHIFTREG_PWM_LEVEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    return shiftreg_pwm_set_base_brightness(s_brightness_levels[level_index]);
}

// Poll the two brightness switches and step the global brightness on press.
//
// The task runs on core 1 so the display timing loop on core 0 stays out of
// the button-handling path.
static void shiftreg_pwm_control_task(void *arg)
{
    bool prev_brighter_pressed = false;
    bool prev_dimmer_pressed = false;

    (void)arg;

    while (1) {
        bool brighter_pressed = false;
        bool dimmer_pressed = false;

        if (PIN_SW_OPT1 >= 0) {
            brighter_pressed = (gpio_get_level(PIN_SW_OPT1) == 0);
        }

        if (PIN_SW_OPT2 >= 0) {
            dimmer_pressed = (gpio_get_level(PIN_SW_OPT2) == 0);
        }

        if (brighter_pressed && !prev_brighter_pressed && s_current_level_index < (SHIFTREG_PWM_LEVEL_COUNT - 1U)) {
            if (shiftreg_pwm_apply_level_index(s_current_level_index + 1U) == ESP_OK) {
                ESP_LOGI(TAG_shiftreg_pwm,
                         "Brightness increased to %lu%%",
                         (unsigned long)s_current_brightness_percent);
            }
        }

        if (dimmer_pressed && !prev_dimmer_pressed && s_current_level_index > 0U) {
            if (shiftreg_pwm_apply_level_index(s_current_level_index - 1U) == ESP_OK) {
                ESP_LOGI(TAG_shiftreg_pwm,
                         "Brightness decreased to %lu%%",
                         (unsigned long)s_current_brightness_percent);
            }
        }

        prev_brighter_pressed = brighter_pressed;
        prev_dimmer_pressed = dimmer_pressed;

        vTaskDelay(pdMS_TO_TICKS(SHIFTREG_PWM_POLL_PERIOD_MS));
    }
}

esp_err_t shiftreg_pwm_init(void)
{
    ledc_timer_config_t timer_cfg;
    ledc_channel_config_t channel_cfg;

    if (shiftreg_pwm_config.init <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        ESP_LOGW(TAG_shiftreg_pwm, "shiftreg_pwm_init called more than once");
        return ESP_OK;
    }

    if (shiftreg_pwm_config.gpio_num < 0 ||
        shiftreg_pwm_config.frequency_hz == 0U ||
        shiftreg_pwm_config.startup_brightness_percent > 100U) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        shiftreg_pwm_config_output_pin(PIN_LED_RED),
        TAG_shiftreg_pwm,
        "PIN_LED_RED config failed"
    );
    ESP_RETURN_ON_ERROR(
        shiftreg_pwm_config_output_pin(PIN_LED_YELLOW),
        TAG_shiftreg_pwm,
        "PIN_LED_YELLOW config failed"
    );
    ESP_RETURN_ON_ERROR(
        shiftreg_pwm_config_output_pin(PIN_LED_BLUE),
        TAG_shiftreg_pwm,
        "PIN_LED_BLUE config failed"
    );
    ESP_RETURN_ON_ERROR(
        shiftreg_pwm_config_input_pin(PIN_SW_OPT1),
        TAG_shiftreg_pwm,
        "PIN_SW_OPT1 config failed"
    );
    ESP_RETURN_ON_ERROR(
        shiftreg_pwm_config_input_pin(PIN_SW_OPT2),
        TAG_shiftreg_pwm,
        "PIN_SW_OPT2 config failed"
    );

    timer_cfg = (ledc_timer_config_t) {
        .speed_mode = SHIFTREG_PWM_LEDC_MODE,
        .duty_resolution = SHIFTREG_PWM_DUTY_RES,
        .timer_num = shiftreg_pwm_config.timer,
        .freq_hz = shiftreg_pwm_config.frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ESP_RETURN_ON_ERROR(
        ledc_timer_config(&timer_cfg),
        TAG_shiftreg_pwm,
        "ledc_timer_config failed"
    );

    channel_cfg = (ledc_channel_config_t) {
        .gpio_num = shiftreg_pwm_config.gpio_num,
        .speed_mode = SHIFTREG_PWM_LEDC_MODE,
        .channel = shiftreg_pwm_config.channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = shiftreg_pwm_config.timer,
        .duty = shiftreg_pwm_brightness_to_duty(shiftreg_pwm_config.startup_brightness_percent),
        .hpoint = 0
    };

    ESP_RETURN_ON_ERROR(
        ledc_channel_config(&channel_cfg),
        TAG_shiftreg_pwm,
        "ledc_channel_config failed"
    );

    s_initialized = true;
    s_gate_enabled = true;
    s_current_brightness_percent = shiftreg_pwm_config.startup_brightness_percent;
    s_current_level_index = shiftreg_pwm_find_level_index(s_current_brightness_percent);
    shiftreg_pwm_update_debug_leds(s_current_level_index);

    ESP_LOGI(TAG_shiftreg_pwm,
             "Shift-register brightness PWM initialized on GPIO %d at %lu Hz",
             shiftreg_pwm_config.gpio_num,
             (unsigned long)shiftreg_pwm_config.frequency_hz);
    ESP_LOGI(TAG_shiftreg_pwm,
             "Startup brightness set to %lu%%",
             (unsigned long)s_current_brightness_percent);

    if (s_control_task_handle == NULL) {
        if (xTaskCreatePinnedToCore(
                shiftreg_pwm_control_task,
                "shiftreg_pwm_ctrl",
                SHIFTREG_PWM_CONTROL_TASK_STACK,
                NULL,
                SHIFTREG_PWM_CONTROL_TASK_PRIO,
                &s_control_task_handle,
                1) != pdPASS) {
            s_initialized = false;
            shiftreg_pwm_update_debug_leds(0U);
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

esp_err_t shiftreg_pwm_set_brightness_percent(uint32_t brightness_percent)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness_percent > 100U) {
        return ESP_ERR_INVALID_ARG;
    }

    return shiftreg_pwm_set_base_brightness(brightness_percent);
}

uint32_t shiftreg_pwm_get_brightness_percent(void)
{
    return s_current_brightness_percent;
}

// Open or close the OE gate while preserving the stored base brightness level.
esp_err_t shiftreg_pwm_set_gate_enabled(bool enabled)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_gate_enabled = enabled;
    return shiftreg_pwm_apply_current_output();
}

// Report whether the display gate is currently open.
bool shiftreg_pwm_get_gate_enabled(void)
{
    return s_gate_enabled;
}

bool shiftreg_pwm_is_initialized(void)
{
    return s_initialized;
}

esp_err_t shiftreg_pwm_deinit(void)
{
    ledc_timer_config_t timer_cfg;

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_control_task_handle != NULL) {
        vTaskDelete(s_control_task_handle);
        s_control_task_handle = NULL;
    }

    ledc_stop(SHIFTREG_PWM_LEDC_MODE, shiftreg_pwm_config.channel, 1);

    timer_cfg = (ledc_timer_config_t) {
        .speed_mode = SHIFTREG_PWM_LEDC_MODE,
        .timer_num = shiftreg_pwm_config.timer,
        .deconfigure = true,
    };

    (void)ledc_timer_config(&timer_cfg);

    shiftreg_pwm_config.init = 0;
    s_initialized = false;
    s_gate_enabled = true;
    s_current_brightness_percent = 0U;
    s_current_level_index = 0U;
    shiftreg_pwm_update_debug_leds(0U);

    ESP_LOGI(TAG_shiftreg_pwm, "Shift-register brightness PWM deinitialized");

    return ESP_OK;
}
