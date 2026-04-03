#include "display_task.h"

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "encoder.h"
#include "shiftreg.h"
#include "main.h"

static const char *TAG_display_task = "display_task";

// Example trigger points for one revolution.
//
// Each PCNT value marks where the display task should update to the next slice.
// Later these values should come from the frame metadata received over Wi-Fi.
static const int s_demo_trigger_counts[] = {
    128,
    256,
    384,
    512,
};

// Example 64-byte slice payloads used by the display task below.
//
// These are only bring-up patterns so you can verify that count-based slice
// updates work before the network path is added.
static const uint8_t s_demo_slice_frames[][SR_FRAME_BYTES] = {
    {
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    },
    {
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
    },
    {
        0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04
    },
    {
        0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08
    }
};

static TaskHandle_t s_display_task_handle = NULL;

// Return the number of demo slices in the example trigger/frame tables.
static size_t demo_slice_count(void)
{
    return sizeof(s_demo_trigger_counts) / sizeof(s_demo_trigger_counts[0]);
}

// Arm the next PCNT watch point that should wake the display task.
//
// Keeping this in one helper makes the slice-advance path easier to follow.
static esp_err_t arm_next_demo_watch_point(size_t slice_index)
{
    if (slice_index >= demo_slice_count()) {
        return ESP_ERR_INVALID_ARG;
    }

    return encoder_set_count_watch_point(s_demo_trigger_counts[slice_index]);
}

// Example display task driven by encoder notifications.
//
// Z notifications start a new revolution.
// Count notifications advance to the next slice output within that revolution.
static void display_task(void *arg)
{
    uint32_t notify_bits = 0;
    size_t next_slice_index = 0;
    esp_err_t err;
    int watch_point = 0;

    (void)arg;

    while (1) {
        // Sleep until the encoder tells us either the revolution restarted
        // or the A/B counter reached the next slice trigger point.
        xTaskNotifyWait(0, UINT32_MAX, &notify_bits, portMAX_DELAY);

        if ((notify_bits & ENCODER_NOTIFY_EVENT_Z) != 0U) {
            // Reset the slice sequence at the index pulse.
            next_slice_index = 0;

            // Clear PCNT from task context, not from the GPIO ISR.
            err = encoder_clear_count();
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "encoder_clear_count failed: %s", esp_err_to_name(err));
                continue;
            }

            // Arm the first slice trigger for the new revolution.
            err = arm_next_demo_watch_point(next_slice_index);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "arm_next_demo_watch_point failed: %s", esp_err_to_name(err));
            }
        }

        if ((notify_bits & ENCODER_NOTIFY_EVENT_COUNT) != 0U) {
            // Read the watch point that woke the task for debug / verification.
            err = encoder_get_last_watch_point(&watch_point);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "encoder_get_last_watch_point failed: %s", esp_err_to_name(err));
                continue;
            }

            // Push the 64-byte slice payload associated with this trigger.
            err = shiftreg_send_frame(s_demo_slice_frames[next_slice_index], SR_FRAME_BYTES);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "shiftreg_send_frame failed: %s", esp_err_to_name(err));
                continue;
            }

            ESP_LOGI(TAG_display_task, "Displayed slice %u at count %d", (unsigned)next_slice_index, watch_point);

            // Advance to the next slice in the current revolution.
            next_slice_index++;

            // Only arm the next watch point if another slice remains before Z.
            if (next_slice_index < demo_slice_count()) {
                err = arm_next_demo_watch_point(next_slice_index);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG_display_task, "arm_next_demo_watch_point failed: %s", esp_err_to_name(err));
                }
            }
        }
    }
}

esp_err_t display_task_start(void)
{
    esp_err_t err;

    // Only allow one display task instance.
    if (s_display_task_handle != NULL) {
        ESP_LOGW(TAG_display_task, "display_task_start called more than once");
        return ESP_OK;
    }

    // Create the display task on core 0 so encoder-driven display work stays on
    // the core reserved for time-sensitive tasks.
    if (xTaskCreatePinnedToCore(
            display_task,
            "display_task",
            4096,
            NULL,
            10,
            &s_display_task_handle,
            0) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    // Route both encoder event types to the display task.
    encoder_set_z_notify_task(s_display_task_handle);
    encoder_set_count_notify_task(s_display_task_handle);

    // Prime the first count threshold so the slice-update path can be tested
    // before the first full revolution completes.
    err = arm_next_demo_watch_point(0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_display_task, "Initial watch-point arm failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}
