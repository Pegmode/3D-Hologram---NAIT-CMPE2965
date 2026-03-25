#ifndef PWM_H
#define PWM_H

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ESC PWM settings
// Most hobby ESCs expect a 50 Hz signal, which is a 20 ms period.
#define PWM_ESC_FREQ_HZ          50U
#define PWM_ESC_PERIOD_US        20000U

// Special startup / arming signal requested
#define PWM_ESC_ARM_US           870U
#define PWM_ESC_ARM_TIME_MS      5000U

// Normal command range for throttle
#define PWM_ESC_MIN_US           870U
#define PWM_ESC_MAX_US           2000U

// Configuration structure for the PWM output
typedef struct
{
    gpio_num_t gpio_num;           // GPIO pin connected to ESC signal wire
    ledc_channel_t channel;        // LEDC channel used for output
    ledc_timer_t timer;            // LEDC timer used for PWM timing

    uint32_t arm_pulse_us;         // Startup pulse width, e.g. 870 us
    uint32_t arm_time_ms;          // How long to hold startup pulse
    uint32_t command_min_us;       // Minimum normal pulse width
    uint32_t command_max_us;       // Maximum normal pulse width
} pwm_config_t;

extern pwm_config_t pwm_config;

// Initialize PWM hardware and send the startup pulse
esp_err_t pwm_init(const pwm_config_t *config);

// Set the PWM pulse width in microseconds
esp_err_t pwm_set_pulse_us(uint32_t pulse_us);

// Get the last pulse width that was applied
uint32_t pwm_get_pulse_us(void);

// Stop PWM and clean up
esp_err_t pwm_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // PWM_H