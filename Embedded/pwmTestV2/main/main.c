#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"

#include "pwm.h"

// GPIO pin connected to ESC signal input
#define ESC_SIGNAL_GPIO     GPIO_NUM_4

// UART used for serial console
#define CONSOLE_UART        UART_NUM_0
#define CONSOLE_BAUD        115200
#define RX_BUF_SIZE         1024

// Write a string to the UART console
static void console_write(const char *text)
{
    uart_write_bytes(CONSOLE_UART, text, strlen(text));
}

// Write a string plus newline to the UART console
static void console_write_line(const char *text)
{
    uart_write_bytes(CONSOLE_UART, text, strlen(text));
    uart_write_bytes(CONSOLE_UART, "\r\n", 2);
}

// Initialize UART0 so we can type commands from the PC serial monitor
static void console_uart_init(void)
{
    const uart_config_t uart_cfg = {
        .baud_rate = CONSOLE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(CONSOLE_UART, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(CONSOLE_UART,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, RX_BUF_SIZE, 0, 0, NULL, 0));
}

// Read one line of text from UART until Enter is pressed
static int uart_read_line(char *buf, size_t buf_len)
{
    size_t idx = 0;

    if (buf == NULL || buf_len < 2) {
        return -1;
    }

    while (idx < (buf_len - 1)) {
        uint8_t ch = 0;

        // Wait forever for 1 byte from UART
        int len = uart_read_bytes(CONSOLE_UART, &ch, 1, portMAX_DELAY);
        if (len <= 0) {
            continue;
        }

        // If Enter is pressed, finish the string
        if (ch == '\r' || ch == '\n') {
            if (idx == 0) {
                // Ignore blank line
                continue;
            }

            console_write("\r\n");
            break;
        }

        // Handle backspace so user can edit input
        if ((ch == '\b' || ch == 127) && idx > 0) {
            idx--;
            console_write("\b \b");
            continue;
        }

        // Store typed character in buffer
        buf[idx++] = (char)ch;

        // Echo the typed character back to terminal
        uart_write_bytes(CONSOLE_UART, (const char *)&ch, 1);
    }

    // Null terminate the string
    buf[idx] = '\0';
    return (int)idx;
}

void app_main(void)
{
    char line[32];

    // Start serial console
    console_uart_init();

    // PWM setup structure
    pwm_config_t pwm_cfg = {
        .gpio_num        = ESC_SIGNAL_GPIO,
        .channel         = LEDC_CHANNEL_0,
        .timer           = LEDC_TIMER_0,
        .arm_pulse_us    = PWM_ESC_ARM_US,
        .arm_time_ms     = PWM_ESC_ARM_TIME_MS,
        .command_min_us  = PWM_ESC_MIN_US,
        .command_max_us  = PWM_ESC_MAX_US,
    };

    console_write_line("");
    console_write_line("ESC PWM demo");
    console_write_line("Sending init pulse now...");
    console_write_line("Arm pulse: 870 us for 5000 ms");
    console_write_line("");

    // Initialize PWM and hold startup pulse
    ESP_ERROR_CHECK(pwm_init(&pwm_cfg));

    console_write_line("Ready.");
    console_write_line("Type one of these, then press Enter:");
    console_write_line("  870        -> send arm/init pulse again");
    console_write_line("  1000..2000 -> normal throttle pulse width in us");
    console_write_line("");

    while (1) {
        // Prompt user
        console_write("pulse_us> ");

        // Read one line from terminal
        int n = uart_read_line(line, sizeof(line));
        if (n <= 0) {
            continue;
        }

        

        // Convert typed string to number
        char *endptr = NULL;
        long pulse = strtol(line, &endptr, 10);

        // Check for invalid text input
        if (endptr == line || *endptr != '\0') {
            console_write_line("Invalid input. Enter a number from 800 to 2000.");
            continue;
        }

        // Send pulse width to PWM driver
        esp_err_t err = pwm_set_pulse_us((uint32_t)pulse);
        if (err == ESP_OK) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Set pulse to %ld us", pulse);
            console_write_line(msg);
        } else {
            console_write_line("Out of range. Use 800..2000.");
        }
    }
}