#include "pwm.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"

// ESP32-S3 uses LEDC low speed mode
#define PWM_LEDC_MODE       LEDC_LOW_SPEED_MODE

// Duty resolution for PWM
// 14-bit gives values from 0 to 16383
#define PWM_DUTY_BITS       14U
#define PWM_DUTY_RES        LEDC_TIMER_14_BIT

static const char *TAG_pwm = "pwm";

// Static module variables
// These only exist inside pwm.c

pwm_config_t pwm_config = {
	.init = -1,					   // bool, marks if config has be initialized.
    .gpio_num = -1,           // GPIO pin connected to ESC signal wire
    .channel = -1,        // LEDC channel used for output
    .timer = -1,            // LEDC timer used for PWM timing
    .arm_pulse_us = -1,         // Startup pulse width, e.g. 870 us
    .arm_time_ms = -1,          // How long to hold startup pulse
    .command_min_us = -1,       // Minimum normal pulse width
    .command_max_us = -1       // Maximum normal pulse width
};

static bool s_initialized = false;       // Tracks whether module is initialized
static uint32_t s_current_pulse_us = 0;  // Last pulse width sent

// Convert pulse width in microseconds into LEDC duty value
// Example:
// 1000 us out of 20000 us period = 5% duty cycle
// duty = pulse_us / period * max_duty
static uint32_t pulse_us_to_duty(uint32_t pulse_us)
{
    const uint32_t scale = (1U << PWM_DUTY_BITS);

    // Use uint64_t so multiplication does not overflow
    // Add half the denominator for simple rounding
    uint64_t duty = ((uint64_t)pulse_us * scale + (PWM_ESC_PERIOD_US / 2U)) / PWM_ESC_PERIOD_US;
    return (uint32_t)duty;
}

// Apply a pulse width directly to the LEDC hardware
static esp_err_t pwm_apply_pulse_us(uint32_t pulse_us)
{
    uint32_t duty = pulse_us_to_duty(pulse_us);

    ESP_RETURN_ON_ERROR(
        ledc_set_duty(PWM_LEDC_MODE, pwm_config.channel, duty),
        TAG_pwm,
        "ledc_set_duty failed"
    );

    ESP_RETURN_ON_ERROR(
        ledc_update_duty(PWM_LEDC_MODE, pwm_config.channel),
        TAG_pwm,
        "ledc_update_duty failed"
    );

    // Save current pulse for later reading
    s_current_pulse_us = pulse_us;
    return ESP_OK;
}

// Initialize LEDC timer + channel, then send startup pulse
esp_err_t pwm_init()
{
    // Check config
    if (pwm_config.init <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Prevent double initialization
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Make sure min <= max
    if (pwm_config.command_min_us > pwm_config.command_max_us) {
        return ESP_ERR_INVALID_ARG;
    }

    // Save config into local module storage
    // memset(&s_cfg, 0, sizeof(s_cfg));
    // s_cfg = *config;

    // Configure LEDC timer
    // This sets PWM frequency and resolution
    ledc_timer_config_t timer_cfg = {
        .speed_mode       = PWM_LEDC_MODE,
        .duty_resolution  = PWM_DUTY_RES,
        .timer_num        = pwm_config.timer,
        .freq_hz          = PWM_ESC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };

    ESP_RETURN_ON_ERROR(
        ledc_timer_config(&timer_cfg),
        TAG_pwm,
        "ledc_timer_config failed"
    );


    // Configure LEDC channel
    // This connects the timer to a specific GPIO pin
    ledc_channel_config_t channel_cfg = {
        .gpio_num   = pwm_config.gpio_num,
        .speed_mode = PWM_LEDC_MODE,
        .channel    = pwm_config.channel,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = pwm_config.timer,
        .duty       = pulse_us_to_duty(pwm_config.arm_pulse_us), // initial pulse
        .hpoint     = 0,
    };

    ESP_RETURN_ON_ERROR(
        ledc_channel_config(&channel_cfg),
        TAG_pwm,
        "ledc_channel_config failed"
    );

    s_initialized = true;
    s_current_pulse_us = pwm_config.arm_pulse_us;

    // Hold the ESC startup pulse for the requested time
    vTaskDelay(pdMS_TO_TICKS(pwm_config.arm_time_ms));

    return ESP_OK;
}

// Public function to set pulse width
esp_err_t pwm_set_pulse_us(uint32_t pulse_us)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Allow either:
    // - the special arm pulse
    // - a normal pulse inside the command range
    if ((pulse_us != pwm_config.arm_pulse_us) &&
        (pulse_us < pwm_config.command_min_us || pulse_us > pwm_config.command_max_us)) {
        return ESP_ERR_INVALID_ARG;
    }

    return pwm_apply_pulse_us(pulse_us);
}

// Return the most recent pulse width
uint32_t pwm_get_pulse_us(void)
{
    return s_current_pulse_us;
}

// Stop PWM output and deconfigure timer
esp_err_t pwm_deinit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Stop channel output
    ledc_stop(PWM_LEDC_MODE, pwm_config.channel, 0);

    // Deconfigure timer
    ledc_timer_config_t timer_cfg = {
        .speed_mode  = PWM_LEDC_MODE,
        .timer_num   = pwm_config.timer,
        .deconfigure = true,
    };

    (void)ledc_timer_config(&timer_cfg);

    // Clear saved data
    pwm_config.init = 0;
    s_initialized = false;
    s_current_pulse_us = 0;

    return ESP_OK;
}
