#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the console I/O module.
// Must be called once before using the other console functions.
void console_io_init(void);

// Write text to the console exactly as provided.
// No newline is added automatically.
void console_io_write(const char *text);

// Write a line of text followed by CRLF ("\r\n").
void console_io_write_line(const char *text);

// Read one line of user input from the console.
// The line ending is removed before returning.
// Returns:
//   >0 : number of characters read
//    0 : empty line
//   -1 : error / invalid arguments
int console_io_read_line(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_IO_H