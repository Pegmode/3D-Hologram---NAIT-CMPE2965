#include "console_io.h"

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/usb_serial_jtag.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

// Internal buffer sizes for the USB Serial/JTAG driver.
// These do not need to be huge for simple console input/output.
#define CONSOLE_RX_BUF_SIZE 256
#define CONSOLE_TX_BUF_SIZE 256

static const char *TAG_console_io = "console_io";

// Tracks whether the console driver has already been installed.
static bool s_console_initialized = false;

esp_err_t console_io_init(void)
{
    // Prevent installing the driver more than once.
    if (s_console_initialized) {
        ESP_LOGW(TAG_console_io, "console_io_init called more than once");
        return ESP_OK;
    }

    // Configure the USB Serial/JTAG driver.
    usb_serial_jtag_driver_config_t cfg = {
        .tx_buffer_size = CONSOLE_TX_BUF_SIZE,
        .rx_buffer_size = CONSOLE_RX_BUF_SIZE,
    };

    ESP_RETURN_ON_ERROR(
        usb_serial_jtag_driver_install(&cfg),
        TAG_console_io,
        "usb_serial_jtag_driver_install failed"
    );

    s_console_initialized = true;
    return ESP_OK;
}

esp_err_t console_io_write(const char *text)
{
    if (!s_console_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (text[0] == '\0') {
        return ESP_OK;
    }

    int written = usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
    if (written < 0) {
        ESP_LOGE(TAG_console_io, "usb_serial_jtag_write_bytes failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t console_io_write_line(const char *text)
{
    if (!s_console_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (text != NULL && text[0] != '\0') {
        ESP_RETURN_ON_ERROR(console_io_write(text), TAG_console_io, "console_io_write failed");
    } else if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int written = usb_serial_jtag_write_bytes("\r\n", 2, portMAX_DELAY);
    if (written < 0) {
        ESP_LOGE(TAG_console_io, "usb_serial_jtag_write_bytes failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t console_io_read_line(char *buf, size_t buf_len, size_t *chars_read)
{
    if (!s_console_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (buf == NULL || buf_len < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t idx = 0;

    while (idx < (buf_len - 1)) {
        uint8_t ch = 0;
        int len = usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY);
        if (len <= 0) {
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            if (idx == 0) {
                continue;
            }

            ESP_RETURN_ON_ERROR(console_io_write("\r\n"), TAG_console_io, "console_io_write failed");
            break;
        }

        if ((ch == '\b' || ch == 127) && idx > 0) {
            idx--;
            ESP_RETURN_ON_ERROR(console_io_write("\b \b"), TAG_console_io, "console_io_write failed");
            continue;
        }

        buf[idx++] = (char)ch;
        int written = usb_serial_jtag_write_bytes((const char *)&ch, 1, portMAX_DELAY);
        if (written < 0) {
            ESP_LOGE(TAG_console_io, "usb_serial_jtag_write_bytes failed");
            return ESP_FAIL;
        }
    }

    buf[idx] = '\0';

    if (chars_read != NULL) {
        *chars_read = idx;
    }

    return ESP_OK;
}
