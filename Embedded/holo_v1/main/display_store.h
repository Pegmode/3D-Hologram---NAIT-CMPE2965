#ifndef DISPLAY_STORE_H
#define DISPLAY_STORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

#include "wifi_rx.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

// One displayed slice is always a fixed-width 64-byte payload.
typedef uint8_t display_slice_t[DISPLAY_SLICE_BYTES];

// Storage for a full animation set.
//
// trigger_counts[slice] stores the PCNT threshold for each slice position.
// frame_data stores all frame/slice payloads as a contiguous block of
// display_slice_t elements:
//
//     frame_data[(frame_index * slice_count) + slice_index]
//
// The helper accessors in this module hide that indexing so the caller can
// treat it like runtime-sized frame_data[frame][slice][64].
typedef struct
{
    uint32_t frame_count;
    uint32_t slice_count;

    int32_t *trigger_counts;
    display_slice_t *frame_data;
} display_store_t;

// Allocate storage for trigger counts and frame payloads.
//
// trigger_counts is stored in normal 8-bit capable RAM.
// frame_data is stored in PSRAM so larger animation sets fit comfortably.
esp_err_t display_store_alloc(display_store_t *store, uint32_t frame_count, uint32_t slice_count);

// Free all allocated storage and clear the struct back to zeros.
void display_store_free(display_store_t *store);

// Returns true when both trigger and frame buffers have been allocated.
bool display_store_is_allocated(const display_store_t *store);

// Total number of slices stored across all animation frames.
size_t display_store_total_slices(const display_store_t *store);

// Total bytes used by frame_data only.
size_t display_store_total_bytes(const display_store_t *store);

// Returns a pointer to the trigger threshold for one slice index.
int32_t *display_store_trigger_at(display_store_t *store, uint32_t slice_index);


// Returns a pointer to one 64-byte slice payload for the given frame/slice.
display_slice_t *display_store_slice_at(display_store_t *store, uint32_t frame_index, uint32_t slice_index);
const display_slice_t *display_store_slice_at_const(const display_store_t *store, uint32_t frame_index, uint32_t slice_index);

// Initialize the dual-buffered display-store manager.
esp_err_t display_store_manager_init(void);

// Returns true once the active/staging store manager has been initialized.
bool display_store_manager_is_initialized(void);

// Get the currently active display data used by the display task.
display_store_t *display_store_get_active(void);

// Build a new staging buffer from one validated Wi-Fi payload.
//
// This allocates or reallocates the staging store as needed, copies all slice
// data into PSRAM, and computes evenly spaced encoder update triggers in normal
// RAM using ENC_COUNTS_PER_REV.
esp_err_t display_store_stage_from_payload(const wifi_rx_header_t *header,
                                           const uint8_t *payload,
                                           size_t payload_len);

// Mark the staged store as ready to replace the active store on the next Z.
esp_err_t display_store_request_swap(void);

// Returns true when a staged store is waiting to become active.
bool display_store_swap_pending(void);

// Promote the staged store to active if a swap is pending.
//
// Intended to be called from the display task at the index pulse so frame data
// and trigger counts change only at a safe revolution boundary.
esp_err_t display_store_swap_if_pending(bool *did_swap);

// Free both active and staging data sets and cancel any pending swap.
//
// This is used by control flows such as DISPLAYOFF where the currently shown
// image data should be dropped completely instead of waiting for replacement.
esp_err_t display_store_clear_all(void);

// Test helper: print the currently staged trigger-count list to the console.
esp_err_t display_store_print_staging_triggers_console(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_STORE_H
