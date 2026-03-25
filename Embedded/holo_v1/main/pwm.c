#include "pwm.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP32-S3 uses LEDC low speed mode
#define PWM_LEDC_MODE       LEDC_LOW_SPEED_MODE

// Duty resolution for PWM
// 14-bit gives values from 0 to 16383
#define PWM_DUTY_BITS       14U
#define PWM_DUTY_RES        LEDC_TIMER_14_BIT

// Static module variables
// These only exist inside pwm.c
static pwm_config_t s_cfg = {0};         // Stores user config after init
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
    esp_err_t err;
    uint32_t duty = pulse_us_to_duty(pulse_us);

    // Set the new duty value for the channel
    err = ledc_set_duty(PWM_LEDC_MODE, s_cfg.channel, duty);
    if (err != ESP_OK) {
        return err;
    }

    // Push the updated duty value to hardware
    err = ledc_update_duty(PWM_LEDC_MODE, s_cfg.channel);
    if (err != ESP_OK) {
        return err;
    }

    // Save current pulse for later reading
    s_current_pulse_us = pulse_us;
    return ESP_OK;
}

// Initialize LEDC timer + channel, then send startup pulse
esp_err_t pwm_init(const pwm_config_t *config)
{
    esp_err_t err;

    // Check input pointer
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Prevent double initialization
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Make sure min <= max
    if (config->command_min_us > config->command_max_us) {
        return ESP_ERR_INVALID_ARG;
    }

    // Save config into local module storage
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg = *config;

    // Configure LEDC timer
    // This sets PWM frequency and resolution
    ledc_timer_config_t timer_cfg = {
        .speed_mode       = PWM_LEDC_MODE,
        .duty_resolution  = PWM_DUTY_RES,
        .timer_num        = s_cfg.timer,
        .freq_hz          = PWM_ESC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };

    err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        return err;
    }

    // Configure LEDC channel
    // This connects the timer to a specific GPIO pin
    ledc_channel_config_t channel_cfg = {
        .gpio_num   = s_cfg.gpio_num,
        .speed_mode = PWM_LEDC_MODE,
        .channel    = s_cfg.channel,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = s_cfg.timer,
        .duty       = pulse_us_to_duty(s_cfg.arm_pulse_us), // initial pulse
        .hpoint     = 0,
    };

    err = ledc_channel_config(&channel_cfg);
    if (err != ESP_OK) {
        return err;
    }

    s_initialized = true;
    s_current_pulse_us = s_cfg.arm_pulse_us;

    // Hold the ESC startup pulse for the requested time
    vTaskDelay(pdMS_TO_TICKS(s_cfg.arm_time_ms));

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
    if ((pulse_us != s_cfg.arm_pulse_us) &&
        (pulse_us < s_cfg.command_min_us || pulse_us > s_cfg.command_max_us)) {
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
    ledc_stop(PWM_LEDC_MODE, s_cfg.channel, 0);

    // Deconfigure timer
    ledc_timer_config_t timer_cfg = {
        .speed_mode  = PWM_LEDC_MODE,
        .timer_num   = s_cfg.timer,
        .deconfigure = true,
    };

    (void)ledc_timer_config(&timer_cfg);

    // Clear saved data
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_initialized = false;
    s_current_pulse_us = 0;

    return ESP_OK;
}