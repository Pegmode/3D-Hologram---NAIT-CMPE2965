#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"

// Configuration for the encoder module
typedef struct
{
    int pin_a;                  // Encoder A channel
    int pin_b;                  // Encoder B channel
    int pin_z;                  // Encoder Z / index channel, or -1 if not used

    gpio_pull_mode_t ab_pull;   // Pull mode for A/B pins
    gpio_pull_mode_t z_pull;    // Pull mode for Z pin

    uint32_t glitch_filter_ns;  // PCNT glitch filter length in ns
} encoder_config_t;

// Initialize the encoder module
esp_err_t encoder_init(const encoder_config_t *config);

// Register a task to be notified when the Z pulse occurs
// Pass NULL to disable task notification.
void encoder_set_z_notify_task(TaskHandle_t task_handle);

// Read the current encoder count
esp_err_t encoder_get_count(int *count);

// Clear the current encoder count back to 0
esp_err_t encoder_clear_count(void);

// Get the number of Z pulses seen so far
uint32_t encoder_get_revolution_count(void);

// Returns true if a Z pin is configured
bool encoder_has_z(void);

#endif // ENCODER_H