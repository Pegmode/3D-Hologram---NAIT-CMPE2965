#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"

#include "console_io.h"
#include "pwm.h"

// GPIO pin connected to the ESC signal input.
// Change this if you wired the ESC signal to a different GPIO.
#define ESC_SIGNAL_GPIO GPIO_NUM_4

void app_main(void)
{
    // Buffer used to store one line typed by the user in the monitor terminal.
    char line[32];

    // Initialize the console module.
    // On ESP32-S3, this uses the USB Serial/JTAG peripheral for input/output.
    console_io_init();

    // Configuration structure passed into the PWM module.
    // This defines which GPIO/timer/channel to use and the allowed pulse widths.
    pwm_config_t pwm_cfg = {
        .gpio_num        = ESC_SIGNAL_GPIO,
        .channel         = LEDC_CHANNEL_0,
        .timer           = LEDC_TIMER_0,
        .arm_pulse_us    = PWM_ESC_ARM_US,
        .arm_time_ms     = PWM_ESC_ARM_TIME_MS,
        .command_min_us  = PWM_ESC_MIN_US,
        .command_max_us  = PWM_ESC_MAX_US,
    };

    // Print startup information to the console.
    console_io_write_line("");
    console_io_write_line("ESC PWM demo");
    console_io_write_line("Sending init pulse now...");
    console_io_write_line("Arm pulse: 870 us for 5000 ms");
    console_io_write_line("");

    // Initialize the PWM output.
    // The PWM module will also hold the arm/startup pulse for the configured time.
    ESP_ERROR_CHECK(pwm_init(&pwm_cfg));

    // Show the user how to use the program.
    console_io_write_line("Ready.");
    console_io_write_line("Type one of these, then press Enter:");
    console_io_write_line("  870        -> send arm/init pulse again");
    console_io_write_line("  1000..2000 -> normal throttle pulse width in us");
    console_io_write_line("  arm        -> same as 870");
    console_io_write_line("");

    // Main application loop.
    // Repeatedly prompt the user, read one line, and apply the requested pulse width.
    while (1) {
        // Print the command prompt.
        console_io_write("pulse_us> ");

        // Read one line from the console.
        // The function blocks until the user presses Enter.
        int n = console_io_read_line(line, sizeof(line));

        // Ignore failed reads or empty lines.
        if (n <= 0) {
            continue;
        }

        // Optional text command: "arm"
        // This re-sends the special startup pulse of 870 us.
        if (strcmp(line, "arm") == 0 || strcmp(line, "ARM") == 0) {
            esp_err_t err = pwm_set_pulse_us(PWM_ESC_ARM_US);

            if (err == ESP_OK) {
                console_io_write_line("Set pulse to 870 us");
            } else {
                console_io_write_line("Failed to set arm pulse");
            }

            continue;
        }

        // Convert the typed text into a number.
        // Example: "1500" becomes integer 1500.
        char *endptr = NULL;
        long pulse = strtol(line, &endptr, 10);

        // Reject input that is not a clean integer.
        // For example:
        //   "abc"   -> invalid
        //   "1500x" -> invalid
        if (endptr == line || *endptr != '\0') {
            console_io_write_line("Invalid input. Enter 870 or a number from 1000 to 2000.");
            continue;
        }

        // Send the pulse width to the PWM module.
        // Allowed values are:
        //   - 870 us
        //   - 1000 to 2000 us
        esp_err_t err = pwm_set_pulse_us((uint32_t)pulse);

        if (err == ESP_OK) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Set pulse to %ld us", pulse);
            console_io_write_line(msg);
        } else {
            console_io_write_line("Out of range. Use 870 or 1000..2000.");
        }
    }
}