#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Create and start the display task on core 0.
//
// The task waits for encoder notifications, swaps in newly staged frame data at
// each Z pulse, and advances through a full-revolution trigger table using the
// current encoder count.
//
// In the current display mode, the active slice set is shown twice per
// revolution:
// - once normally in the first 180 degrees
// - once mirrored in the second 180 degrees
esp_err_t display_task_start(void);

// Request that the display task blank the LEDs and drop any loaded image data.
//
// This is intended for control packets such as WIFI_RX_DATA_TYPE_DISPLAYOFF so
// the actual display shutdown still happens on core 0 where the display logic
// already lives.
esp_err_t display_task_request_display_off(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_TASK_H
