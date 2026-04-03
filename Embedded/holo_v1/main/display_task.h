#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Create and start the example display task on core 0.
//
// The task waits for encoder notifications and advances through a small set of
// demo slices based on PCNT watch points.
esp_err_t display_task_start(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_TASK_H
