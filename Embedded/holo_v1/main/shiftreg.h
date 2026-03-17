#ifndef SHIFTREG_H
#define SHIFTREG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver/spi_master.h"
#include "esp_err.h"

// Configuration for the shift register driver
typedef struct
{
    spi_host_device_t spi_host;   // SPI2_HOST or SPI3_HOST on ESP32-S3

    int pin_mosi;                 // SPI MOSI pin
    int pin_sclk;                 // SPI clock pin
    int pin_miso;                 // SPI MISO pin, or -1 if unused

    int pin_le;                   // Latch Enable pin
    int pin_oe;                   // Output Enable pin (active LOW), or -1 if not used

    int spi_clock_hz;             // SPI clock frequency
    size_t max_transfer_bytes;    // Maximum frame size supported by this module
} shiftreg_config_t;

// Public config variable
extern shiftreg_config_t shiftreg_config;

// Initialize the shift register module
esp_err_t shiftreg_init();

// Send one frame of data to the shift register chain
//
// frame points to the data to be shifted out.
// frame_len_bytes is how many bytes to send.
//
// The function does:
// 1) shift data out over SPI
// 2) blank outputs briefly if OE is available
// 3) pulse LE to latch the new outputs
// 4) re-enable outputs
esp_err_t shiftreg_send_frame(const uint8_t *frame, size_t frame_len_bytes);

// Enable or disable the outputs manually
//
// enable = true  -> outputs ON
// enable = false -> outputs OFF
//
// If OE pin is not configured, this function does nothing and returns ESP_OK.
esp_err_t shiftreg_set_output_enabled(bool enable);

// Pulse the latch pin manually
esp_err_t shiftreg_latch(void);

// Returns true if the module has been initialized
bool shiftreg_is_initialized(void);

#endif // SHIFTREG_H