#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"

#include "console_io.h"
#include "pwm.h"

#define ESC_SIGNAL_GPIO GPIO_NUM_4

void app_main(void)
{
    char line[32];

    console_io_init();

    pwm_config_t pwm_cfg = {
        .gpio_num        = ESC_SIGNAL_GPIO,
        .channel         = LEDC_CHANNEL_0,
        .timer           = LEDC_TIMER_0,
        .arm_pulse_us    = PWM_ESC_ARM_US,
        .arm_time_ms     = PWM_ESC_ARM_TIME_MS,
        .command_min_us  = PWM_ESC_MIN_US,
        .command_max_us  = PWM_ESC_MAX_US,
    };

    console_io_write_line("");
    console_io_write_line("ESC PWM demo");
    console_io_write_line("Sending init pulse now...");
    console_io_write_line("Arm pulse: 870 us for 5000 ms");
    console_io_write_line("");

    ESP_ERROR_CHECK(pwm_init(&pwm_cfg));

    console_io_write_line("Ready.");
    console_io_write_line("Type one of these, then press Enter:");
    console_io_write_line("  870        -> send arm/init pulse again");
    console_io_write_line("  1000..2000 -> normal throttle pulse width in us");
    console_io_write_line("  arm        -> same as 870");
    console_io_write_line("");

    while (1) {
        console_io_write("pulse_us> ");

        int n = console_io_read_line(line, sizeof(line));
        if (n <= 0) {
            continue;
        }

        if (strcmp(line, "arm") == 0 || strcmp(line, "ARM") == 0) {
            esp_err_t err = pwm_set_pulse_us(PWM_ESC_ARM_US);
            if (err == ESP_OK) {
                console_io_write_line("Set pulse to 870 us");
            } else {
                console_io_write_line("Failed to set arm pulse");
            }
            continue;
        }

        char *endptr = NULL;
        long pulse = strtol(line, &endptr, 10);

        if (endptr == line || *endptr != '\0') {
            console_io_write_line("Invalid input. Enter 870 or a number from 1000 to 2000.");
            continue;
        }

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