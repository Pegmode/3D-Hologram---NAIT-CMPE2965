#include "display_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "display_store.h"
#include "encoder.h"
#include "main.h"
#include "pwm.h"
#include "shiftreg.h"
#include "shiftreg_pwm.h"
#include "speed_telemetry.h"

static const char *TAG_display_task = "display_task";

// Private task-notify bit used for control requests that originate outside the
// encoder module.
#define DISPLAY_TASK_NOTIFY_EVENT_DISPLAY_OFF (1UL << 2)

static TaskHandle_t s_display_task_handle = NULL;
static esp_timer_handle_t s_strobe_off_timer = NULL;

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

    bool strobe_enabled_for_rev;
    bool strobe_gate_active;
    int64_t strobe_deadline_us;
    uint32_t strobe_on_time_us;
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

// Clear all per-revolution strobe state so the display returns to continuous
// mode until a new valid strobe timing is computed.
static esp_err_t display_task_set_gate_enabled(bool enabled);

// Timer callback used to close the OE gate at a precise microsecond deadline
// after each slice becomes visible.
static void display_task_strobe_off_timer_cb(void *arg)
{
    (void)arg;
    (void)display_task_set_gate_enabled(false);
}

// Stop the pending one-shot blanking timer, if one is currently armed.
static void display_task_stop_strobe_timer(void)
{
    esp_err_t err;

    if (s_strobe_off_timer == NULL) {
        return;
    }

    err = esp_timer_stop(s_strobe_off_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG_display_task, "esp_timer_stop failed: %s", esp_err_to_name(err));
    }
}

static void display_task_reset_strobe_state(display_task_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->strobe_enabled_for_rev = false;
    state->strobe_on_time_us = 0U;
    state->strobe_gate_active = false;
    state->strobe_deadline_us = 0;
    display_task_stop_strobe_timer();
}

// Compute the hybrid slice visibility width from the current revolution period
// and step count, then clamp it to the configured bounds.
static uint32_t display_task_compute_strobe_on_time_us(uint32_t revolution_period_us,
                                                       uint32_t step_count)
{
    uint64_t slice_interval_us;
    uint64_t on_time_us;

    if (revolution_period_us == 0U || step_count == 0U) {
        return 0U;
    }

    slice_interval_us = (uint64_t)revolution_period_us / (uint64_t)step_count;
    if (slice_interval_us == 0U) {
        return 0U;
    }

    on_time_us = (slice_interval_us * (uint64_t)DISPLAY_STROBE_DUTY_PERCENT) / 100U;

    if (on_time_us < DISPLAY_STROBE_MIN_ON_US) {
        on_time_us = DISPLAY_STROBE_MIN_ON_US;
    }

    if (on_time_us > DISPLAY_STROBE_MAX_ON_US) {
        on_time_us = DISPLAY_STROBE_MAX_ON_US;
    }

    // Leave at least a tiny off-window before the next slice whenever the
    // slice interval is long enough.
    if (on_time_us >= slice_interval_us) {
        on_time_us = (slice_interval_us > 1U) ? (slice_interval_us - 1U) : 1U;
    }

    return (uint32_t)on_time_us;
}

// Route display blanking requests through the OE PWM gate when that hardware is
// available. If OE PWM is disabled, treat the request as a no-op.
static esp_err_t display_task_set_gate_enabled(bool enabled)
{
    if (!shiftreg_pwm_is_initialized()) {
        return ESP_OK;
    }

    return shiftreg_pwm_set_gate_enabled(enabled);
}

// Recompute the per-slice strobe width once per revolution and decide whether
// this revolution should use pulsed or continuous visibility.
static esp_err_t display_task_refresh_strobe_for_revolution(display_task_state_t *state,
                                                            const display_store_t *active_store)
{
    speed_telemetry_snapshot_t telemetry = { 0 };
    uint32_t step_count;
    esp_err_t err;

    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    display_task_reset_strobe_state(state);

    if (!DISPLAY_STROBE_ENABLED) {
        return display_task_set_gate_enabled(true);
    }

    if (pwm_get_pulse_us() == PWM_ESC_MIN_US) {
        return display_task_set_gate_enabled(true);
    }

    if (active_store == NULL || !display_store_is_allocated(active_store)) {
        return display_task_set_gate_enabled(true);
    }

    step_count = display_store_step_count(active_store);
    if (step_count == 0U) {
        return display_task_set_gate_enabled(true);
    }

    err = speed_telemetry_get_snapshot(&telemetry);
    if (err != ESP_OK || telemetry.period <= 0) {
        return display_task_set_gate_enabled(true);
    }

    state->strobe_on_time_us = display_task_compute_strobe_on_time_us((uint32_t)telemetry.period, step_count);
    if (state->strobe_on_time_us == 0U) {
        return display_task_set_gate_enabled(true);
    }

    state->strobe_enabled_for_rev = true;

    // Start the revolution dark; each slice update opens a short visibility
    // window after the new data has been latched.
    return display_task_set_gate_enabled(false);
}

// Open the visibility window for the freshly latched slice. In continuous mode
// this simply leaves the gate on until the next update.
static esp_err_t display_task_start_visibility_window(display_task_state_t *state)
{
    esp_err_t err;

    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!state->strobe_enabled_for_rev) {
        state->strobe_gate_active = false;
        state->strobe_deadline_us = 0;
        return display_task_set_gate_enabled(true);
    }

    display_task_stop_strobe_timer();

    err = display_task_set_gate_enabled(true);
    if (err != ESP_OK) {
        return err;
    }

    if (s_strobe_off_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    err = esp_timer_start_once(s_strobe_off_timer, state->strobe_on_time_us);
    if (err != ESP_OK) {
        (void)display_task_set_gate_enabled(false);
        return err;
    }

    state->strobe_gate_active = false;
    state->strobe_deadline_us = 0;

    return ESP_OK;
}

// Poll-based blanking is no longer used once the one-shot strobe timer exists,
// but this helper is kept as a no-op to preserve the task structure.
static void display_task_service_strobe_gate(display_task_state_t *state)
{
    (void)state;
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
                                                 int trigger_count,
                                                 display_store_t *active_store)
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

    err = display_task_start_visibility_window(state);
    if (err != ESP_OK) {
        return err;
    }

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
    uint32_t revcount;
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
    revcount = encoder_get_revolution_count();
    speed_telemetry_record_revolution(revcount);

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
        display_task_reset_strobe_state(state);
        (void)display_task_set_gate_enabled(true);
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

    err = display_task_refresh_strobe_for_revolution(state, active_store);
    if (err != ESP_OK) {
        return err;
    }

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

    display_task_stop_strobe_timer();

    err = display_task_set_gate_enabled(false);
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
    display_task_reset_strobe_state(state);

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
        .strobe_enabled_for_rev = false,
        .strobe_gate_active = false,
        .strobe_deadline_us = 0,
        .strobe_on_time_us = 0U,
    };
    BaseType_t notified = pdFALSE;

    (void)arg;

    // Wait for app_main() to finish setup.
    vTaskDelay(10);

    while (1) {
        esp_err_t err;

        display_task_service_strobe_gate(&state);

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

        if (active_store == NULL) {
            continue;
        }

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
    esp_timer_create_args_t strobe_timer_args;

    if (s_display_task_handle != NULL) {
        ESP_LOGW(TAG_display_task, "display_task_start called more than once");
        return ESP_OK;
    }

    if (s_strobe_off_timer == NULL) {
        strobe_timer_args = (esp_timer_create_args_t) {
            .callback = &display_task_strobe_off_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "disp_strobe_off",
            .skip_unhandled_events = true,
        };

        if (esp_timer_create(&strobe_timer_args, &s_strobe_off_timer) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    if (xTaskCreatePinnedToCore(
            display_task,
            "display_task",
            4096,
            NULL,
            10,
            &s_display_task_handle,
            0) != pdPASS) {
        if (s_strobe_off_timer != NULL) {
            esp_timer_delete(s_strobe_off_timer);
            s_strobe_off_timer = NULL;
        }
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
