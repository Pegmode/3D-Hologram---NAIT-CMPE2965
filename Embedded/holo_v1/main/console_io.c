#include "console_io.h"

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"

// Internal buffer sizes for the USB Serial/JTAG driver.
// These do not need to be huge for simple console input/output.
#define CONSOLE_RX_BUF_SIZE 256
#define CONSOLE_TX_BUF_SIZE 256

// Tracks whether the console driver has already been installed.
static bool s_console_initialized = false;

void console_io_init(void)
{
    // Prevent installing the driver more than once.
    if (s_console_initialized) {
        return;
    }

    // Configure the USB Serial/JTAG driver.
    usb_serial_jtag_driver_config_t cfg = {
        .tx_buffer_size = CONSOLE_TX_BUF_SIZE,
        .rx_buffer_size = CONSOLE_RX_BUF_SIZE,
    };

    // Install the driver.
    // If this fails, ESP_ERROR_CHECK will stop the program.
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));

    s_console_initialized = true;
}

void console_io_write(const char *text)
{
    // Ignore null pointers and empty strings.
    if (text == NULL || text[0] == '\0') {
        return;
    }

    // Write the text to the USB Serial/JTAG console.
    // portMAX_DELAY means block until the write can complete.
    usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
}

void console_io_write_line(const char *text)
{
    // Write the main text only if it is non-empty.
    // This avoids passing a zero-length buffer to the driver.
    if (text != NULL && text[0] != '\0') {
        usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
    }

    // Always finish the line with carriage return + newline.
    // This displays properly in most serial terminals.
    usb_serial_jtag_write_bytes("\r\n", 2, portMAX_DELAY);
}

int console_io_read_line(char *buf, size_t buf_len)
{
    // Validate the destination buffer.
    // Need at least room for one character plus null terminator.
    if (buf == NULL || buf_len < 2) {
        return -1;
    }

    size_t idx = 0;

    // Read characters one at a time until Enter is pressed
    // or the buffer becomes full.
    while (idx < (buf_len - 1)) {
        uint8_t ch = 0;

        // Block until one byte is received from the console.
        int len = usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY);
        if (len <= 0) {
            continue;
        }

        // End the line when CR or LF is received.
        if (ch == '\r' || ch == '\n') {
            // Ignore an immediate blank CR/LF.
            // This helps with terminals that send CRLF pairs.
            if (idx == 0) {
                continue;
            }

            // Echo a newline so the terminal cursor moves to the next line.
            console_io_write("\r\n");
            break;
        }

        // Handle backspace/delete so the user can edit their input.
        if ((ch == '\b' || ch == 127) && idx > 0) {
            idx--;

            // Erase one character visually in the terminal:
            // backspace, overwrite with space, backspace again.
            console_io_write("\b \b");
            continue;
        }

        // Store the typed character in the buffer.
        buf[idx++] = (char)ch;

        // Echo the typed character back to the terminal.
        usb_serial_jtag_write_bytes((const char *)&ch, 1, portMAX_DELAY);
    }

    // Null-terminate the completed string.
    buf[idx] = '\0';

    // Return the number of characters entered, not counting the terminator.
    return (int)idx;
}