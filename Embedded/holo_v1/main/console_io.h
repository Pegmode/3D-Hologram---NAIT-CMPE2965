#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the console I/O module.
// Must be called once before using the other console functions.
esp_err_t console_io_init(void);

// Write text to the console exactly as provided.
// No newline is added automatically.
esp_err_t console_io_write(const char *text);

// Write a line of text followed by CRLF ("\r\n").
esp_err_t console_io_write_line(const char *text);

// Read one line of user input from the console.
// The line ending is removed before returning.
// chars_read may be NULL if the caller does not need the final length.
esp_err_t console_io_read_line(char *buf, size_t buf_len, size_t *chars_read);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_IO_H
