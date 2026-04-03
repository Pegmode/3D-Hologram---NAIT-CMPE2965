#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"

// Task notification bits used by the encoder module.
//
// A task can wait on these bits with xTaskNotifyWait() to tell whether it woke
// up because of the Z pulse or because the A/B count hit a watch point.
#define ENCODER_NOTIFY_EVENT_Z      (1UL << 0)
#define ENCODER_NOTIFY_EVENT_COUNT  (1UL << 1)

// Configuration for the encoder module
typedef struct
{
	int init;					// determine if config has been initialized in main
    int pin_a;                  // Encoder A channel
    int pin_b;                  // Encoder B channel
    int pin_z;                  // Encoder Z / index channel, or -1 if not used

    gpio_pull_mode_t ab_pull;   // Pull mode for A/B pins
    gpio_pull_mode_t z_pull;    // Pull mode for Z pin

    uint32_t glitch_filter_ns;  // PCNT glitch filter length in ns
} encoder_config_t;

extern encoder_config_t encoder_config;

esp_err_t encoder_config_input_pin(int pin, gpio_pull_mode_t pull_mode, gpio_int_type_t intr_type);

void encoder_z_isr(void *arg);

esp_err_t encoder_init_pcnt(void);

esp_err_t encoder_init_z(void);

// Initialize the encoder module
esp_err_t encoder_init();

// Register a task to be notified when the Z pulse occurs
// Pass NULL to disable task notification.
void encoder_set_z_notify_task(TaskHandle_t task_handle);

// Register a task to be notified when the A/B counter reaches the next watch point.
// Pass NULL to disable task notification.
void encoder_set_count_notify_task(TaskHandle_t task_handle);

// Configure the next PCNT watch point that should wake the display task.
// Only one active watch point is tracked at a time.
esp_err_t encoder_set_count_watch_point(int watch_point);

// Read the most recent watch point value that triggered the PCNT callback.
esp_err_t encoder_get_last_watch_point(int *watch_point);

// Read the current encoder count
esp_err_t encoder_get_count(int *count);

// Clear the current encoder count back to 0
esp_err_t encoder_clear_count(void);

// Get the number of Z pulses seen so far
uint32_t encoder_get_revolution_count(void);

#endif // ENCODER_H
