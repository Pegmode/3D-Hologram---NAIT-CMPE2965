#include "display_store.h"

#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG_display_store = "display_store";

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

esp_err_t display_store_alloc(display_store_t *store, uint32_t frame_count, uint32_t slice_count)
{
    if (store == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_count == 0 || slice_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (display_store_is_allocated(store)) {
        ESP_LOGW(TAG_display_store, "display_store_alloc called on an allocated store");
        return ESP_ERR_INVALID_STATE;
    }

    // One trigger value exists per slice position, regardless of frame count.
    store->trigger_counts = (int32_t *)heap_caps_calloc(
        slice_count,
        sizeof(*store->trigger_counts),
        MALLOC_CAP_8BIT
    );
    if (store->trigger_counts == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Frame data is stored as one contiguous block of display_slice_t items:
    // [frame0 slice0], [frame0 slice1], ..., [frameN sliceM]
    store->frame_data = (display_slice_t *)heap_caps_calloc(
        (size_t)frame_count * (size_t)slice_count,
        sizeof(*store->frame_data),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (store->frame_data == NULL) {
        heap_caps_free(store->trigger_counts);
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

    // Reset the struct so it is safe to reuse with a later alloc call.
    memset(store, 0, sizeof(*store));
}

bool display_store_is_allocated(const display_store_t *store)
{
    if (store == NULL) {
        return false;
    }

    return (store->trigger_counts != NULL) && (store->frame_data != NULL);
}

size_t display_store_total_slices(const display_store_t *store)
{
    if (store == NULL) {
        return 0;
    }

    // frame_count x slice_count gives the total number of stored 64-byte slices.
    return (size_t)store->frame_count * (size_t)store->slice_count;
}

size_t display_store_total_bytes(const display_store_t *store)
{
    return display_store_total_slices(store) * DISPLAY_SLICE_BYTES;
}

int32_t *display_store_trigger_at(display_store_t *store, uint32_t slice_index)
{
    if (store == NULL || !display_store_is_allocated(store) || slice_index >= store->slice_count) {
        return NULL;
    }

    return &store->trigger_counts[slice_index];
}

const int32_t *display_store_trigger_at_const(const display_store_t *store, uint32_t slice_index)
{
    if (store == NULL || !display_store_is_allocated(store) || slice_index >= store->slice_count) {
        return NULL;
    }

    return &store->trigger_counts[slice_index];
}

display_slice_t *display_store_slice_at(display_store_t *store, uint32_t frame_index, uint32_t slice_index)
{
    size_t linear_index;

    if (!display_store_indices_valid(store, frame_index, slice_index)) {
        return NULL;
    }

    // Flatten runtime-sized [frame][slice] addressing into one linear index.
    linear_index = ((size_t)frame_index * (size_t)store->slice_count) + (size_t)slice_index;
    return &store->frame_data[linear_index];
}

const display_slice_t *display_store_slice_at_const(const display_store_t *store, uint32_t frame_index, uint32_t slice_index)
{
    size_t linear_index;

    if (!display_store_indices_valid(store, frame_index, slice_index)) {
        return NULL;
    }

    // Same indexing as display_store_slice_at(), but for read-only callers.
    linear_index = ((size_t)frame_index * (size_t)store->slice_count) + (size_t)slice_index;
    return &store->frame_data[linear_index];
}
