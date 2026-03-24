#include "console_io.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"

#define CONSOLE_RX_BUF_SIZE 256
#define CONSOLE_TX_BUF_SIZE 256

static bool s_console_initialized = false;

void console_io_init(void)
{
    if (s_console_initialized) {
        return;
    }

    usb_serial_jtag_driver_config_t cfg = {
        .tx_buffer_size = CONSOLE_TX_BUF_SIZE,
        .rx_buffer_size = CONSOLE_RX_BUF_SIZE,
    };

    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));

    s_console_initialized = true;
}

void console_io_write(const char *text)
{
    if (text == NULL) {
        return;
    }

    usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
}

void console_io_write_line(const char *text)
{
    if (text != NULL && text[0] != '\0') {
        usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
    }
    usb_serial_jtag_write_bytes("\r\n", 2, portMAX_DELAY);
}

int console_io_read_line(char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len < 2) {
        return -1;
    }

    size_t idx = 0;

    while (idx < (buf_len - 1)) {
        uint8_t ch = 0;

        // Block until at least one byte arrives
        int len = usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY);
        if (len <= 0) {
            continue;
        }

        // Handle CR/LF as end-of-line
        if (ch == '\r' || ch == '\n') {
            // Ignore empty CR/LF so terminals sending CRLF do not produce blank lines
            if (idx == 0) {
                continue;
            }

            console_io_write("\r\n");
            break;
        }

        // Handle backspace/delete
        if ((ch == '\b' || ch == 127) && idx > 0) {
            idx--;
            console_io_write("\b \b");
            continue;
        }

        buf[idx++] = (char)ch;

        // Echo typed character
        usb_serial_jtag_write_bytes((const char *)&ch, 1, portMAX_DELAY);
    }

    buf[idx] = '\0';
    return (int)idx;
}