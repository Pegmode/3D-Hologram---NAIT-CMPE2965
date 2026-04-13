#ifndef SHIFTREG_PWM_H
#define SHIFTREG_PWM_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

// Configuration for the global shift-register brightness PWM.
//
// This module drives the shift-register OE pin with hardware LEDC PWM.
// Because OE is active LOW, the brightness math is inverted internally:
// 0% brightness keeps OE high (outputs off)
// 100% brightness keeps OE low (outputs fully enabled)
typedef struct
{
    int init;                           // bool, marks if config has been initialized
    gpio_num_t gpio_num;                // GPIO connected to shift-register OE
    ledc_channel_t channel;             // LEDC channel used for brightness PWM
    ledc_timer_t timer;                 // LEDC timer used for brightness PWM
    uint32_t frequency_hz;              // PWM frequency for global dimming
    uint32_t startup_brightness_percent;// Initial brightness after init, 0..100
} shiftreg_pwm_config_t;

extern shiftreg_pwm_config_t shiftreg_pwm_config;

// Initialize global brightness PWM on the OE pin.
esp_err_t shiftreg_pwm_init(void);

// Set global brightness in percent, from 0 to 100.
esp_err_t shiftreg_pwm_set_brightness_percent(uint32_t brightness_percent);

// Get the last brightness percent that was applied.
uint32_t shiftreg_pwm_get_brightness_percent(void);

// Returns true if the PWM module has been initialized.
bool shiftreg_pwm_is_initialized(void);

// Stop PWM output and leave OE high so all outputs stay disabled.
esp_err_t shiftreg_pwm_deinit(void);

#endif // SHIFTREG_PWM_H
