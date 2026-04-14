#include "display_store.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG_display_store = "display_store";

// One 64-byte slice is arranged as 32 rows of 16 columns, with each row
// occupying two bytes in the payload.
#define DISPLAY_SLICE_ROWS               32U
#define DISPLAY_SLICE_BYTES_PER_ROW      2U

// The current display mode plays each slice set twice per revolution:
// - original image in the first 180 degrees
// - mirrored image in the second 180 degrees
#define DISPLAY_PLAYBACKS_PER_REV        2U

// Protect active/staging pointer swaps across the Wi-Fi task on core 1 and the
// display task on core 0.
static portMUX_TYPE s_display_store_lock = portMUX_INITIALIZER_UNLOCKED;

// One buffer is active for the display task while the other is reused as the
// staging area for the next received payload.
static display_store_t s_store_a = { 0 };
static display_store_t s_store_b = { 0 };
static display_store_t *s_active_store = &s_store_a;
static display_store_t *s_staging_store = &s_store_b;
static bool s_swap_pending = false;
static bool s_manager_initialized = false;

// Validate frame/slice indices against an allocated store before access.
static bool display_store_indices_valid(const display_store_t *store, uint32_t frame_index, uint32_t slice_index)
{
    if (store == NULL) {
        return false;
    }

    if (!display_store_is_allocated(store)) {
        return false;
    }

    if (frame_index >= store->frame_count) {
        return false;
    }

    if (slice_index >= store->slice_count) {
        return false;
    }

    return true;
}

// Return the frame count implied by the Wi-Fi packet type.
static esp_err_t display_store_get_effective_frame_count(const wifi_rx_header_t *header, uint32_t *frame_count)
{
    if (header == NULL || frame_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (header->data_type) {
        case WIFI_RX_DATA_TYPE_STILL_3D:
            *frame_count = 1;
            return ESP_OK;

        case WIFI_RX_DATA_TYPE_ANIMATION_3D:
            if (header->frame_count <= 0) {
                return ESP_ERR_INVALID_ARG;
            }
            *frame_count = (uint32_t)header->frame_count;
            return ESP_OK;

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

// Compute evenly spaced encoder update triggers for one full revolution.
//
// The display now uses two playback windows per revolution:
// - the first half-revolution shows the original slices
// - the second half-revolution shows mirrored slices
static esp_err_t display_store_fill_update_triggers(display_store_t *store)
{
    uint32_t slice_index;
    uint32_t half_counts_per_rev;

    if (store == NULL || !display_store_is_allocated(store)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (store->slice_count == 0U || store->slice_count > ENC_COUNTS_PER_REV) {
        return ESP_ERR_INVALID_ARG;
    }

    half_counts_per_rev = ENC_COUNTS_PER_REV / 2U;

    for (slice_index = 0; slice_index < store->slice_count; slice_index++) {
        uint32_t first_half_index = slice_index;
        uint32_t second_half_index = store->slice_count + slice_index;

        // Use proportional integer math within each 180-degree window so
        // rounding error is spread evenly across both playbacks.
        store->trigger_counts[first_half_index] =
            (int32_t)(((uint64_t)slice_index * (uint64_t)half_counts_per_rev) /
                      (uint64_t)store->slice_count);

        store->trigger_counts[second_half_index] =
            (int32_t)(half_counts_per_rev +
                      (((uint64_t)slice_index * (uint64_t)half_counts_per_rev) /
                       (uint64_t)store->slice_count));
    }

    return ESP_OK;
}

// Reverse the 16 bits of one display row so the 16 visible columns are
// mirrored left-to-right for the second half of the revolution.
static uint16_t display_store_reverse_16_bits(uint16_t value)
{
    value = (uint16_t)(((value & 0x5555U) << 1) | ((value >> 1) & 0x5555U));
    value = (uint16_t)(((value & 0x3333U) << 2) | ((value >> 2) & 0x3333U));
    value = (uint16_t)(((value & 0x0F0FU) << 4) | ((value >> 4) & 0x0F0FU));
    value = (uint16_t)((value << 8) | (value >> 8));

    return value;
}

// Build the mirrored slice buffer from the original payload data.
//
// The current slice format is assumed to be:
// - 32 rows
// - 2 bytes per row
// - row bytes stored in big-endian order
static void display_store_fill_mirrored_slices(display_store_t *store)
{
    size_t total_slices;
    size_t slice_linear_index;

    total_slices = display_store_total_slices(store);

    for (slice_linear_index = 0; slice_linear_index < total_slices; slice_linear_index++) {
        const uint8_t *src = (const uint8_t *)store->frame_data[slice_linear_index];
        uint8_t *dst = (uint8_t *)store->mirrored_frame_data[slice_linear_index];
        uint32_t row_index;

        for (row_index = 0U; row_index < DISPLAY_SLICE_ROWS; row_index++) {
            size_t row_offset = (size_t)row_index * DISPLAY_SLICE_BYTES_PER_ROW;
            uint16_t row_value = (uint16_t)(((uint16_t)src[row_offset] << 8) |
                                            (uint16_t)src[row_offset + 1U]);
            uint16_t mirrored_row_value = display_store_reverse_16_bits(row_value);

            dst[row_offset] = (uint8_t)(mirrored_row_value >> 8);
            dst[row_offset + 1U] = (uint8_t)(mirrored_row_value & 0xFFU);
        }
    }
}

esp_err_t display_store_alloc(display_store_t *store, uint32_t frame_count, uint32_t slice_count)
{
    if (store == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_count == 0U || slice_count == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (display_store_is_allocated(store)) {
        ESP_LOGW(TAG_display_store, "display_store_alloc called on an allocated store");
        return ESP_ERR_INVALID_STATE;
    }

    // One trigger value exists per display step in the full revolution.
    store->trigger_counts = (int32_t *)heap_caps_calloc(
        slice_count * DISPLAY_PLAYBACKS_PER_REV,
        sizeof(*store->trigger_counts),
        MALLOC_CAP_8BIT
    );
    if (store->trigger_counts == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Frame data is stored as one contiguous block of display_slice_t items:
    // [frame0 slice0], [frame0 slice1], ..., [frameN sliceM]
    //
    // Prefer PSRAM for larger animation sets, but fall back to normal RAM so
    // bring-up still works on targets/builds where PSRAM is unavailable.
    store->frame_data = (display_slice_t *)heap_caps_calloc(
        (size_t)frame_count * (size_t)slice_count,
        sizeof(*store->frame_data),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (store->frame_data == NULL) {
        ESP_LOGW(TAG_display_store, "PSRAM alloc failed for frame_data, falling back to internal RAM");
        store->frame_data = (display_slice_t *)heap_caps_calloc(
            (size_t)frame_count * (size_t)slice_count,
            sizeof(*store->frame_data),
            MALLOC_CAP_8BIT
        );
    }
    if (store->frame_data == NULL) {
        heap_caps_free(store->trigger_counts);
        store->trigger_counts = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Precompute a mirrored copy of every slice so the display task can pick
    // the correct half-revolution view without mirroring bits in real time.
    store->mirrored_frame_data = (display_slice_t *)heap_caps_calloc(
        (size_t)frame_count * (size_t)slice_count,
        sizeof(*store->mirrored_frame_data),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (store->mirrored_frame_data == NULL) {
        ESP_LOGW(TAG_display_store, "PSRAM alloc failed for mirrored_frame_data, falling back to internal RAM");
        store->mirrored_frame_data = (display_slice_t *)heap_caps_calloc(
            (size_t)frame_count * (size_t)slice_count,
            sizeof(*store->mirrored_frame_data),
            MALLOC_CAP_8BIT
        );
    }
    if (store->mirrored_frame_data == NULL) {
        heap_caps_free(store->frame_data);
        heap_caps_free(store->trigger_counts);
        store->frame_data = NULL;
        store->trigger_counts = NULL;
        return ESP_ERR_NO_MEM;
    }

    store->frame_count = frame_count;
    store->slice_count = slice_count;

    return ESP_OK;
}

void display_store_free(display_store_t *store)
{
    if (store == NULL) {
        return;
    }

    if (store->trigger_counts != NULL) {
        heap_caps_free(store->trigger_counts);
    }

    if (store->frame_data != NULL) {
        heap_caps_free(store->frame_data);
    }

    if (store->mirrored_frame_data != NULL) {
        heap_caps_free(store->mirrored_frame_data);
    }

    memset(store, 0, sizeof(*store));
}

bool display_store_is_allocated(const display_store_t *store)
{
    if (store == NULL) {
        return false;
    }

    return (store->trigger_counts != NULL) &&
           (store->frame_data != NULL) &&
           (store->mirrored_frame_data != NULL);
}

size_t display_store_total_slices(const display_store_t *store)
{
    if (store == NULL) {
        return 0;
    }

    return (size_t)store->frame_count * (size_t)store->slice_count;
}

size_t display_store_total_bytes(const display_store_t *store)
{
    return display_store_total_slices(store) * DISPLAY_SLICE_BYTES * 2U;
}

int32_t *display_store_trigger_at(display_store_t *store, uint32_t slice_index)
{
    if (store == NULL || !display_store_is_allocated(store) || slice_index >= display_store_step_count(store)) {
        return NULL;
    }

    return &store->trigger_counts[slice_index];
}

uint32_t display_store_step_count(const display_store_t *store)
{
    if (store == NULL || !display_store_is_allocated(store)) {
        return 0U;
    }

    return store->slice_count * DISPLAY_PLAYBACKS_PER_REV;
}



display_slice_t *display_store_slice_at(display_store_t *store, uint32_t frame_index, uint32_t slice_index)
{
    size_t linear_index;

    if (!display_store_indices_valid(store, frame_index, slice_index)) {
        return NULL;
    }

    linear_index = ((size_t)frame_index * (size_t)store->slice_count) + (size_t)slice_index;
    return &store->frame_data[linear_index];
}

const display_slice_t *display_store_slice_at_const(const display_store_t *store, uint32_t frame_index, uint32_t slice_index)
{
    size_t linear_index;

    if (!display_store_indices_valid(store, frame_index, slice_index)) {
        return NULL;
    }

    linear_index = ((size_t)frame_index * (size_t)store->slice_count) + (size_t)slice_index;
    return &store->frame_data[linear_index];
}

const display_slice_t *display_store_slice_for_step_at_const(const display_store_t *store,
                                                             uint32_t frame_index,
                                                             uint32_t step_index)
{
    size_t linear_index;
    uint32_t slice_index;
    bool mirrored;

    if (store == NULL || !display_store_is_allocated(store)) {
        return NULL;
    }

    if (frame_index >= store->frame_count || step_index >= display_store_step_count(store)) {
        return NULL;
    }

    mirrored = (step_index >= store->slice_count);
    slice_index = step_index % store->slice_count;
    linear_index = ((size_t)frame_index * (size_t)store->slice_count) + (size_t)slice_index;

    if (mirrored) {
        return &store->mirrored_frame_data[linear_index];
    }

    return &store->frame_data[linear_index];
}

esp_err_t display_store_manager_init(void)
{
    if (s_manager_initialized) {
        ESP_LOGW(TAG_display_store, "display_store_manager_init called more than once");
        return ESP_OK;
    }

    s_active_store = &s_store_a;
    s_staging_store = &s_store_b;
    s_swap_pending = false;
    s_manager_initialized = true;

    return ESP_OK;
}

bool display_store_manager_is_initialized(void)
{
    return s_manager_initialized;
}

display_store_t *display_store_get_active(void)
{
    const display_store_t *active_store;

    if (!s_manager_initialized) {
        return NULL;
    }

    portENTER_CRITICAL(&s_display_store_lock);
    active_store = s_active_store;
    portEXIT_CRITICAL(&s_display_store_lock);

    return active_store;
}

esp_err_t display_store_stage_from_payload(const wifi_rx_header_t *header,
                                           const uint8_t *payload,
                                           size_t payload_len)
{
    uint32_t frame_count = 0;
    uint32_t slice_count;
    size_t expected_payload_bytes;
    esp_err_t err;

    if (!s_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (header == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (header->slice_count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    slice_count = (uint32_t)header->slice_count;
    if (slice_count > ENC_COUNTS_PER_REV) {
        ESP_LOGE(TAG_display_store, "slice_count exceeds encoder counts per revolution");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        display_store_get_effective_frame_count(header, &frame_count),
        TAG_display_store,
        "display_store_get_effective_frame_count failed"
    );

    expected_payload_bytes = (size_t)frame_count * (size_t)slice_count * DISPLAY_SLICE_BYTES;
    if (payload_len != expected_payload_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    // Reallocate the staging buffer if the payload shape changed.
    if (!display_store_is_allocated(s_staging_store) ||
        s_staging_store->frame_count != frame_count ||
        s_staging_store->slice_count != slice_count) {
        display_store_free(s_staging_store);
        ESP_RETURN_ON_ERROR(
            display_store_alloc(s_staging_store, frame_count, slice_count),
            TAG_display_store,
            "display_store_alloc failed"
        );
    }

    memcpy(s_staging_store->frame_data, payload, payload_len);
    display_store_fill_mirrored_slices(s_staging_store);

    err = display_store_fill_update_triggers(s_staging_store);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t display_store_request_swap(void)
{
    if (!s_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!display_store_is_allocated(s_staging_store)) {
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_display_store_lock);
    s_swap_pending = true;
    portEXIT_CRITICAL(&s_display_store_lock);

    return ESP_OK;
}

bool display_store_swap_pending(void)
{
    bool swap_pending;

    if (!s_manager_initialized) {
        return false;
    }

    portENTER_CRITICAL(&s_display_store_lock);
    swap_pending = s_swap_pending;
    portEXIT_CRITICAL(&s_display_store_lock);

    return swap_pending;
}

esp_err_t display_store_swap_if_pending(bool *did_swap)
{
    if (!s_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (did_swap != NULL) {
        *did_swap = false;
    }

    portENTER_CRITICAL(&s_display_store_lock);
    if (s_swap_pending) {
        display_store_t *old_active = s_active_store;
        s_active_store = s_staging_store;
        s_staging_store = old_active;
        s_swap_pending = false;
        if (did_swap != NULL) {
            *did_swap = true;
        }
    }
    portEXIT_CRITICAL(&s_display_store_lock);

    return ESP_OK;
}

esp_err_t display_store_clear_all(void)
{
    if (!s_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_display_store_lock);
    display_store_free(&s_store_a);
    display_store_free(&s_store_b);
    s_active_store = &s_store_a;
    s_staging_store = &s_store_b;
    s_swap_pending = false;
    portEXIT_CRITICAL(&s_display_store_lock);

    return ESP_OK;
}

esp_err_t display_store_print_staging_triggers_console(void)
{
    uint32_t step_index;

    if (!s_manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!display_store_is_allocated(s_staging_store)) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG_display_store,
             "staged trigger list: slices=%u steps=%u counts_per_rev=%u",
             (unsigned)s_staging_store->slice_count,
             (unsigned)display_store_step_count(s_staging_store),
             (unsigned)ENC_COUNTS_PER_REV);

    for (step_index = 0; step_index < display_store_step_count(s_staging_store); step_index++) {
        ESP_LOGI(TAG_display_store,
                 "trigger[%u] = %ld",
                 (unsigned)step_index,
                 (long)s_staging_store->trigger_counts[step_index]);
    }

    return ESP_OK;
}
