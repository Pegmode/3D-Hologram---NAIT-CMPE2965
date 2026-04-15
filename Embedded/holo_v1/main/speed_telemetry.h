#ifndef SPEED_TELEMETRY_H
#define SPEED_TELEMETRY_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPEED_TELEMETRY_SEND_INTERVAL_MS   250U
#define SPEED_TELEMETRY_FILTER_WINDOW_SIZE 4U

typedef struct
{
    int32_t rpm;
    int32_t rpmF;
    int32_t period;
    int32_t revcount;
} speed_telemetry_snapshot_t;

// Initialize the shared speed-telemetry state.
esp_err_t speed_telemetry_init(void);

// Record one new revolution boundary from the Z pulse path.
//
// This updates the latest revolution period, instantaneous RPM, and filtered
// RPM using the time delta between consecutive Z pulses.
void speed_telemetry_record_revolution(uint32_t revcount);

// Copy the latest telemetry values into caller storage.
//
// If the rotor has stopped or only one Z pulse has been seen so far, rpm,
// rpmF, and period are reported as 0.
esp_err_t speed_telemetry_get_snapshot(speed_telemetry_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif // SPEED_TELEMETRY_H
