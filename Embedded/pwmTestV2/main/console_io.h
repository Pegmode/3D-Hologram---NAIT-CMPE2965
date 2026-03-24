#ifndef CONSOLE_IO_H
#define CONSOLE_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void console_io_init(void);
void console_io_write(const char *text);
void console_io_write_line(const char *text);
int console_io_read_line(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_IO_H