#include "display_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "console_io.h"
#include "display_store.h"
#include "encoder.h"
#include "shiftreg.h"

static const char *TAG_display_task = "display_task";

static TaskHandle_t s_display_task_handle = NULL;

// Runtime state owned by the display task.
//
// Slice 0 is displayed immediately after each accepted Z pulse. The remaining
// slices are advanced one at a time using one active PCNT watch point.
typedef struct
{
    size_t current_frame_index;
    size_t current_slice_index;
    size_t next_slice_index;
    bool frame_sequence_started;
    bool pending_immediate_update;
} display_task_state_t;

// Print one display-task event to the USB console during bring-up.
static void display_task_print_event_console(const char *event_name,
                                             size_t frame_index,
                                             size_t slice_index,
                                             int encoder_count,
                                             int trigger_count)
{
    char line[128];
    int err;

    err = snprintf(line,
                   sizeof(line),
                   "display event: %s frame=%u slice=%u enc_count=%d trigger_count=%d",
                   event_name,
                   (unsigned)frame_index,
                   (unsigned)slice_index,
                   encoder_count,
                   trigger_count);
    if (err < 0 || (size_t)err >= sizeof(line)) {
        ESP_LOGW(TAG_display_task, "display_task_print_event_console snprintf failed");
        return;
    }

    if (console_io_write_line(line) != ESP_OK) {
        ESP_LOGW(TAG_display_task, "display_task_print_event_console console_io_write_line failed");
    }
}

// Push one slice from the active display store to the shift-register chain.
static esp_err_t display_task_show_slice(const display_store_t *store,
                                         size_t frame_index,
                                         size_t slice_index)
{
    const display_slice_t *slice_data;

    if (store == NULL || !display_store_is_allocated(store)) {
        return ESP_ERR_INVALID_STATE;
    }

    slice_data = display_store_slice_at_const(store, (uint32_t)frame_index, (uint32_t)slice_index);
    if (slice_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return shiftreg_send_frame(*slice_data, DISPLAY_SLICE_BYTES);
}

// Arm the next slice trigger for the current revolution.
//
// Only one hardware watch point is used at a time. Once slice 0 has been shown
// at Z, the task arms the trigger for slice 1. Each later count event re-arms
// the following trigger in the same way.
static esp_err_t display_task_arm_next_watch_point(const display_store_t *store,
                                                   const display_task_state_t *state)
{
    int32_t *trigger_count;

    if (store == NULL || state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!display_store_is_allocated(store)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (state->next_slice_index >= store->slice_count) {
        return encoder_clear_count_watch_point();
    }

    trigger_count = display_store_trigger_at(store, (uint32_t)state->next_slice_index);
    if (trigger_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (*trigger_count <= 0) {
        ESP_LOGE(TAG_display_task,
                 "Invalid trigger count %ld for slice %u",
                 (long)*trigger_count,
                 (unsigned)state->next_slice_index);
        return ESP_ERR_INVALID_STATE;
    }

    return encoder_set_count_watch_point(*trigger_count);
}


// Handle one hardware-triggered slice update.
//
// Because only one watch point is armed at a time, next_slice_index is the
// slice that should be shown when the current count event arrives.
static esp_err_t display_task_handle_count_event(display_task_state_t *state,
                                                 int trigger_count, display_store_t *active_store)
{


    int encoder_count = 0;
    esp_err_t err;

    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    active_store = display_store_get_active();
    if (active_store == NULL || !display_store_is_allocated(active_store)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (state->next_slice_index >= active_store->slice_count) {
        return ESP_ERR_INVALID_STATE;
    }

    state->current_slice_index = state->next_slice_index;


    err = encoder_get_count(&encoder_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_display_task, "encoder_get_count failed during count update: %s", esp_err_to_name(err));
        encoder_count = trigger_count;
    }

    err = display_task_show_slice(active_store, state->current_frame_index, state->current_slice_index);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG_display_task,
             "Displayed frame %u slice %u at count %d",
             (unsigned)state->current_frame_index,
             (unsigned)state->current_slice_index,
             trigger_count);

    display_task_print_event_console("update",
                                     state->current_frame_index,
                                     state->current_slice_index,
                                     encoder_count,
                                     trigger_count);

    state->next_slice_index = state->current_slice_index + 1U;

    return ESP_OK;
}

// Handle the start of one new revolution.
//
// The Z pulse is responsible for:
// - promoting staged data to active at a safe boundary
// - selecting the frame for this revolution
// - clearing the encoder count back to zero
// - clearing any old watch point from the previous revolution
// - requesting the immediate slice-0 update
static esp_err_t display_task_handle_z_event(display_task_state_t *state)
{
    const display_store_t *active_store;
    bool did_swap = false;
    int encoder_count = 0;
    esp_err_t err;

    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    err = encoder_get_count(&encoder_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_display_task, "encoder_get_count failed before Z reset: %s", esp_err_to_name(err));
        encoder_count = 0;
    }

    err = display_store_swap_if_pending(&did_swap);
    if (err != ESP_OK) {
        return err;
    }

    active_store = display_store_get_active();
    
    err = encoder_clear_count();
    if (err != ESP_OK) {
        return err;
    }

    if (active_store == NULL || !display_store_is_allocated(active_store)) {
        state->current_frame_index = 0U;
        state->current_slice_index = 0U;
        state->next_slice_index = 0U;
        state->frame_sequence_started = false;
        state->pending_immediate_update = false;
        return ESP_OK;
    }

    if (did_swap || !state->frame_sequence_started) {
        state->current_frame_index = 0U;
    } else if (active_store->frame_count > 1U) {
        state->current_frame_index = (state->current_frame_index + 1U) % active_store->frame_count;
    } else {
        state->current_frame_index = 0U;
    }

    state->current_slice_index = 0U;
    state->next_slice_index = 0U;
    state->frame_sequence_started = true;
    state->pending_immediate_update = true;


    if (did_swap) {
        display_task_print_event_console("swap_at_z", 0U, 0U, encoder_count, 0);
    }

    display_task_print_event_console("z", 0U, 0U, encoder_count, 0);

    return ESP_OK;
}

static void display_task(void *arg)
{
    int encoder_count = 0;
    int32_t trigger_count = 0;
    int32_t *trigger_ptr = NULL;
    uint32_t notify_bits = 0;
    display_store_t *active_store = NULL;
    display_task_state_t state = {
        .current_frame_index = 0U,
        .current_slice_index = 0U,
        .next_slice_index = 0U,
        .frame_sequence_started = false,
        .pending_immediate_update = false,
    };
    BaseType_t notified = pdFALSE;

    (void)arg;

    //wait for app_main() to finish up
    vTaskDelay(10); //1sec

    while (1) {
        esp_err_t err;

        notified = xTaskNotifyWait(0, UINT32_MAX, &notify_bits, 0);
        if (notified != pdTRUE) {
            notify_bits = 0;
        }

        if ((notify_bits & ENCODER_NOTIFY_EVENT_Z) != 0U) {
            err = display_task_handle_z_event(&state);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "display_task_handle_z_event failed: %s", esp_err_to_name(err));
                continue;
            }

            active_store = display_store_get_active();

            // A Z pulse starts a new revolution. If a count bit arrived in the
            // same notification, it belongs to the previous revolution and
            // should not be processed after the counter reset.
            notify_bits &= ~ENCODER_NOTIFY_EVENT_COUNT;
        }

        // Z triggered slice 0 update.
        if (state.pending_immediate_update) {
            state.pending_immediate_update = false;

            err = display_task_handle_count_event(&state, 0, active_store);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task,
                         "display_task_handle_count_event failed for watch point %d: %s",
                         0,
                         esp_err_to_name(err));
            }
            continue;
        }
        
        err = encoder_get_count(&encoder_count);
            if (err != ESP_OK) {
                ESP_LOGW(TAG_display_task, "encoder_get_count failed during count update: %s", esp_err_to_name(err));
            
        }
        
        if(active_store == NULL)
            continue;
        trigger_ptr = display_store_trigger_at(active_store, (uint32_t)state.next_slice_index);
        if (trigger_ptr == NULL) {
            continue;
        }
        trigger_count = *trigger_ptr;

        if (state.next_slice_index >= active_store->slice_count) {
            taskYIELD();
            continue;
        }

        if (encoder_count >= trigger_count) {

            err = display_task_handle_count_event(&state, trigger_count, active_store);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task,
                         "display_task_handle_count_event failed for trigger %d: %s",
                         trigger_count,
                         esp_err_to_name(err));
            }
        }

        taskYIELD();
    }
}

esp_err_t display_task_start(void)
{
    if (s_display_task_handle != NULL) {
        ESP_LOGW(TAG_display_task, "display_task_start called more than once");
        return ESP_OK;
    }

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

    encoder_set_z_notify_task(s_display_task_handle);
    //encoder_set_count_notify_task(s_display_task_handle);

    return ESP_OK;
}
