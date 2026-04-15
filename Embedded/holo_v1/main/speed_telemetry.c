#include "speed_telemetry.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "esp_timer.h"

// Treat the rotor as stopped when no new revolution arrives within this
// multiple of the last valid revolution period.
#define SPEED_TELEMETRY_STOP_TIMEOUT_MULTIPLIER 3LL

static portMUX_TYPE s_speed_telemetry_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized = false;
static int64_t s_last_z_time_us = 0;
static int32_t s_last_period_us = 0;
static int32_t s_latest_rpm = 0;
static int32_t s_latest_rpm_filtered = 0;
static int32_t s_latest_revcount = 0;
static int32_t s_recent_rpms[SPEED_TELEMETRY_FILTER_WINDOW_SIZE] = { 0 };
static size_t s_recent_rpm_count = 0U;
static size_t s_recent_rpm_index = 0U;

// Clear the rolling RPM filter state after startup or after a long pause in
// rotation so the next valid revolutions rebuild the average cleanly.
static void speed_telemetry_reset_filter_locked(void)
{
    memset(s_recent_rpms, 0, sizeof(s_recent_rpms));
    s_recent_rpm_count = 0U;
    s_recent_rpm_index = 0U;
    s_latest_rpm = 0;
    s_latest_rpm_filtered = 0;
    s_last_period_us = 0;
}

// Return the rounded mean of the RPM samples currently stored in the rolling
// filter window.
static int32_t speed_telemetry_average_filter_locked(void)
{
    int64_t rpm_sum = 0;
    size_t sample_index;

    if (s_recent_rpm_count == 0U) {
        return 0;
    }

    for (sample_index = 0U; sample_index < s_recent_rpm_count; sample_index++) {
        rpm_sum += s_recent_rpms[sample_index];
    }

    return (int32_t)((rpm_sum + ((int64_t)s_recent_rpm_count / 2LL)) / (int64_t)s_recent_rpm_count);
}

// Reset the telemetry module to a known idle state before the display and
// Wi-Fi tasks begin using it.
esp_err_t speed_telemetry_init(void)
{
    portENTER_CRITICAL(&s_speed_telemetry_lock);
    s_initialized = true;
    s_last_z_time_us = 0;
    s_latest_revcount = 0;
    speed_telemetry_reset_filter_locked();
    portEXIT_CRITICAL(&s_speed_telemetry_lock);

    return ESP_OK;
}

// Capture one revolution timestamp, derive the new period and RPM, and update
// the filtered average from the most recent few valid revolutions.
void speed_telemetry_record_revolution(uint32_t revcount)
{
    int64_t now_us = esp_timer_get_time();

    portENTER_CRITICAL(&s_speed_telemetry_lock);

    if (!s_initialized) {
        portEXIT_CRITICAL(&s_speed_telemetry_lock);
        return;
    }

    s_latest_revcount = (revcount > (uint32_t)INT32_MAX) ? INT32_MAX : (int32_t)revcount;

    if (s_last_z_time_us == 0) {
        s_last_z_time_us = now_us;
        speed_telemetry_reset_filter_locked();
        portEXIT_CRITICAL(&s_speed_telemetry_lock);
        return;
    }

    if (s_last_period_us > 0) {
        int64_t stale_timeout_us = (int64_t)s_last_period_us * SPEED_TELEMETRY_STOP_TIMEOUT_MULTIPLIER;

        if ((now_us - s_last_z_time_us) > stale_timeout_us) {
            s_last_z_time_us = now_us;
            speed_telemetry_reset_filter_locked();
            portEXIT_CRITICAL(&s_speed_telemetry_lock);
            return;
        }
    }

    {
        int64_t period_us = now_us - s_last_z_time_us;
        int64_t rpm;

        s_last_z_time_us = now_us;

        if (period_us <= 0 || period_us > (int64_t)INT32_MAX) {
            speed_telemetry_reset_filter_locked();
            portEXIT_CRITICAL(&s_speed_telemetry_lock);
            return;
        }

        rpm = (60000000LL + (period_us / 2LL)) / period_us;

        s_last_period_us = (int32_t)period_us;
        s_latest_rpm = (rpm > (int64_t)INT32_MAX) ? INT32_MAX : (int32_t)rpm;

        s_recent_rpms[s_recent_rpm_index] = s_latest_rpm;
        s_recent_rpm_index = (s_recent_rpm_index + 1U) % SPEED_TELEMETRY_FILTER_WINDOW_SIZE;
        if (s_recent_rpm_count < SPEED_TELEMETRY_FILTER_WINDOW_SIZE) {
            s_recent_rpm_count++;
        }

        s_latest_rpm_filtered = speed_telemetry_average_filter_locked();
    }

    portEXIT_CRITICAL(&s_speed_telemetry_lock);
}

// Return a stable snapshot of the latest speed values for transmission to the
// PC. If the rotor appears stopped, the dynamic fields are forced to zero.
esp_err_t speed_telemetry_get_snapshot(speed_telemetry_snapshot_t *snapshot)
{
    int64_t now_us;

    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    now_us = esp_timer_get_time();

    portENTER_CRITICAL(&s_speed_telemetry_lock);

    if (!s_initialized) {
        memset(snapshot, 0, sizeof(*snapshot));
        portEXIT_CRITICAL(&s_speed_telemetry_lock);
        return ESP_ERR_INVALID_STATE;
    }

    snapshot->rpm = s_latest_rpm;
    snapshot->rpmF = s_latest_rpm_filtered;
    snapshot->period = s_last_period_us;
    snapshot->revcount = s_latest_revcount;

    if (s_last_z_time_us == 0 || s_last_period_us <= 0) {
        snapshot->rpm = 0;
        snapshot->rpmF = 0;
        snapshot->period = 0;
    } else {
        int64_t stale_timeout_us = (int64_t)s_last_period_us * SPEED_TELEMETRY_STOP_TIMEOUT_MULTIPLIER;

        if ((now_us - s_last_z_time_us) > stale_timeout_us) {
            snapshot->rpm = 0;
            snapshot->rpmF = 0;
            snapshot->period = 0;
        }
    }

    portEXIT_CRITICAL(&s_speed_telemetry_lock);

    return ESP_OK;
}
