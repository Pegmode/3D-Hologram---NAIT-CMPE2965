#include "display_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "display_store.h"
#include "encoder.h"
#include "shiftreg.h"

static const char *TAG_display_task = "display_task";

// Private task-notify bit used for control requests that originate outside the
// encoder module.
#define DISPLAY_TASK_NOTIFY_EVENT_DISPLAY_OFF (1UL << 2)

static TaskHandle_t s_display_task_handle = NULL;

// Runtime state owned by the display task.
//
// The active frame is shown twice per revolution:
// - first half-revolution: original slices
// - second half-revolution: mirrored slices
typedef struct
{
    size_t current_frame_index;
    size_t current_slice_index;
    size_t current_step_index;
    size_t next_step_index;
    bool frame_sequence_started;
    bool pending_immediate_update;
} display_task_state_t;

// Print one display-task event through ESP-IDF logging during bring-up.
static void display_task_print_event_console(const char *event_name,
                                             size_t frame_index,
                                             size_t slice_index,
                                             int encoder_count,
                                             int trigger_count)
{
    ESP_LOGI(TAG_display_task,
             "display event: %s frame=%u slice=%u enc_count=%d trigger_count=%d",
             event_name,
             (unsigned)frame_index,
             (unsigned)slice_index,
             encoder_count,
             trigger_count);
}

// Push one step from the active display store to the shift-register chain.
//
// A step maps to either the original or mirrored slice set depending on which
// half of the revolution is currently being shown.
static esp_err_t display_task_show_step(const display_store_t *store,
                                        size_t frame_index,
                                        size_t step_index)
{
    const display_slice_t *slice_data;

    if (store == NULL || !display_store_is_allocated(store)) {
        return ESP_ERR_INVALID_STATE;
    }

    slice_data = display_store_slice_for_step_at_const(store, (uint32_t)frame_index, (uint32_t)step_index);
    if (slice_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return shiftreg_send_frame(*slice_data, DISPLAY_SLICE_BYTES);
}


// Handle one display-step update.
//
// The step index determines:
// - which slice number is shown
// - whether the original or mirrored slice data is used
static esp_err_t display_task_handle_count_event(display_task_state_t *state,
                                                 int trigger_count, display_store_t *active_store)
{
    int encoder_count = 0;
    uint32_t step_count;
    esp_err_t err;

    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    active_store = display_store_get_active();
    if (active_store == NULL || !display_store_is_allocated(active_store)) {
        return ESP_ERR_INVALID_STATE;
    }

    step_count = display_store_step_count(active_store);
    if (state->next_step_index >= step_count) {
        return ESP_ERR_INVALID_STATE;
    }

    state->current_step_index = state->next_step_index;
    state->current_slice_index = state->current_step_index % active_store->slice_count;

    err = encoder_get_count(&encoder_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_display_task, "encoder_get_count failed during count update: %s", esp_err_to_name(err));
        encoder_count = trigger_count;
    }

    err = display_task_show_step(active_store, state->current_frame_index, state->current_step_index);
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

    state->next_step_index = state->current_step_index + 1U;

    return ESP_OK;
}

// Handle the start of one new revolution.
//
// The Z pulse is responsible for:
// - promoting staged data to active at a safe boundary
// - selecting the frame for this revolution
// - clearing the encoder count back to zero
// - rewinding the display-step state to the start of the first half
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
        state->current_step_index = 0U;
        state->next_step_index = 0U;
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
    state->current_step_index = 0U;
    state->next_step_index = 0U;
    state->frame_sequence_started = true;
    state->pending_immediate_update = true;


    if (did_swap) {
        display_task_print_event_console("swap_at_z", 0U, 0U, encoder_count, 0);
    }

    display_task_print_event_console("z", 0U, 0U, encoder_count, 0);

    return ESP_OK;
}

// Blank the display immediately and drop all active/staged image data.
//
// This is handled on core 0 so the display output path and the store cleanup
// stay in one place.
static esp_err_t display_task_handle_display_off(display_task_state_t *state)
{
    esp_err_t err;

    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    err = shiftreg_clear();
    if (err != ESP_OK) {
        return err;
    }

    err = encoder_clear_count_watch_point();
    if (err != ESP_OK) {
        return err;
    }

    err = display_store_clear_all();
    if (err != ESP_OK) {
        return err;
    }

    state->current_frame_index = 0U;
    state->current_slice_index = 0U;
    state->current_step_index = 0U;
    state->next_step_index = 0U;
    state->frame_sequence_started = false;
    state->pending_immediate_update = false;

    display_task_print_event_console("display_off", 0U, 0U, 0, 0);

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
        .current_step_index = 0U,
        .next_step_index = 0U,
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

        if ((notify_bits & DISPLAY_TASK_NOTIFY_EVENT_DISPLAY_OFF) != 0U) {
            err = display_task_handle_display_off(&state);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "display_task_handle_display_off failed: %s", esp_err_to_name(err));
            }

            active_store = display_store_get_active();
            notify_bits &= ~(ENCODER_NOTIFY_EVENT_Z | ENCODER_NOTIFY_EVENT_COUNT);
            taskYIELD();
            continue;
        }

        if ((notify_bits & ENCODER_NOTIFY_EVENT_Z) != 0U) {
            err = display_task_handle_z_event(&state);
            if (err != ESP_OK) {
                ESP_LOGE(TAG_display_task, "display_task_handle_z_event failed: %s", esp_err_to_name(err));
                continue;
            }

            active_store = display_store_get_active();

            // A Z pulse starts a new revolution. If a stale count bit arrived
            // in the same notification word, it belongs to the previous
            // revolution and should not be processed after the counter reset.
            notify_bits &= ~ENCODER_NOTIFY_EVENT_COUNT;
        }

        // The first step of the revolution is displayed immediately after Z.
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
        if (state.next_step_index >= display_store_step_count(active_store)) {
            taskYIELD();
            continue;
        }

        trigger_ptr = display_store_trigger_at(active_store, (uint32_t)state.next_step_index);
        if (trigger_ptr == NULL) {
            continue;
        }
        trigger_count = *trigger_ptr;

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

esp_err_t display_task_request_display_off(void)
{
    if (s_display_task_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xTaskNotify(s_display_task_handle, DISPLAY_TASK_NOTIFY_EVENT_DISPLAY_OFF, eSetBits) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}
