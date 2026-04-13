#ifndef WIFI_RX_H
#define WIFI_RX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif

// Protocol identifier expected at the front of every TCP packet header.
//
// This lets the receiver quickly reject bad data or a misaligned TCP stream.
#define WIFI_RX_HEADER_MAGIC 0x484F4C4FU //HOLO

// Current packet-header version.
//
// Bump this if the wire format changes in an incompatible way.
#define WIFI_RX_HEADER_VERSION 3U

// Types of display payloads that may arrive over TCP.
//
// This is the first routing decision the receiver will use after validating the
// header: still image, multi-frame animation, or a control packet such as
// display-off.
typedef enum
{
    WIFI_RX_DATA_TYPE_NONE = -1,
    WIFI_RX_DATA_TYPE_INVALID = 0,
    WIFI_RX_DATA_TYPE_STILL_3D = 1,
    WIFI_RX_DATA_TYPE_ANIMATION_3D = 2,
    WIFI_RX_DATA_TYPE_DISPLAYOFF = 3
} wifi_rx_data_type_t;

typedef enum
{
    WIFI_RX_MOTOR_OFF = -1,
    WIFI_RX_MOTOR_SAME = 0,
} wifi_rx_motor_t;

// On-wire packet header. All multibyte fields are sent in network byte order.
//
// This struct is only used at the network boundary. After reception it is
// converted into wifi_rx_header_t so the rest of the code can work in native
// host byte order.
typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint8_t version;
    uint8_t header_size_bytes;
    int8_t data_type;
    int32_t frame_count;
    int32_t slice_count;
    int32_t payload_bytes;
    int16_t motor_speed_rpm;
    uint32_t payload_crc32;
} wifi_rx_wire_header_t;

// Parsed packet header stored in host byte order.
//
// The on-wire header is received in network byte order, then converted into
// this struct before the rest of the firmware uses it.
typedef struct
{
    uint32_t magic;               // Should always match WIFI_RX_HEADER_MAGIC.
    uint8_t version;             // Protocol version of the sender.
    uint8_t header_size_bytes;   // Size of the fixed wire header.

    wifi_rx_data_type_t data_type; // Kind of display payload that follows.

    int32_t frame_count;          // Number of full frames in the payload, or -1 if unused.
    int32_t slice_count;          // Number of angular slices per frame, or -1 if unused.
    int32_t payload_bytes;        // Total payload size immediately after header, or -1 for no payload.

    // Motor control command carried in the packet header.
    //
    // Current bring-up behavior:
    // -1 = off / minimum throttle pulse
    //  0 = keep current output
    // >0 = raw ESC pulse width in microseconds
    //
    // The field name is kept for protocol compatibility even though the
    // implementation is temporarily pulse-width based rather than true RPM.
    int16_t motor_speed_rpm;
    uint32_t payload_crc32;       // CRC32 of the payload bytes, or 0 when no payload is sent.
} wifi_rx_header_t;

// Configuration for the Wi-Fi/TCP receive module.
//
// Intended network model:
// - ESP32-S3 creates its own Wi-Fi network in SoftAP mode
// - PC control software joins that Wi-Fi network
// - ESP32-S3 listens as a TCP server on the configured port
// - PC control software connects to the ESP32-S3 as a TCP client
//
// This keeps the ESP32 as the stable endpoint so the PC can change without the
// firmware needing to know the PC's IP address ahead of time.
typedef struct
{
    int init;                   // Marks whether the config has been initialized in main.

    char wifi_ssid[33];         // SoftAP SSID broadcast by the ESP32.
    char wifi_password[65];     // SoftAP password. Leave empty for an open network.

    uint16_t tcp_port;          // TCP port used by the receiver task.
    uint16_t listen_backlog;    // Socket listen backlog.
    uint8_t max_connections;    // Maximum number of Wi-Fi clients allowed on the SoftAP.

    int task_stack_size;        // Receiver task stack size.
    int task_priority;          // Receiver task priority.

    bool print_headers_to_console; // Print each parsed header during bring-up.
} wifi_rx_config_t;

extern wifi_rx_config_t wifi_rx_config;

// Initialize NVS, network stack, SoftAP mode, and start broadcasting.
//
// This prepares networking resources but does not yet create the TCP task.
// The ESP32 acts as a SoftAP here so the PC can join it directly.
esp_err_t wifi_rx_init(void);

// Start the TCP receive task pinned to core 1.
//
// Core 1 keeps the network work away from the time-sensitive display path.
// This task creates the TCP listener that the PC software connects to.
esp_err_t wifi_rx_start(void);

// Returns true once the module has been initialized.
bool wifi_rx_is_initialized(void);

// Copy the most recently received header into caller storage.
//
// Useful for quick debug checks from other modules.
esp_err_t wifi_rx_get_last_header(wifi_rx_header_t *header);

// Convert a data type enum into a readable string.
const char *wifi_rx_data_type_to_string(wifi_rx_data_type_t data_type);

// Test helper: print one parsed header to the USB console.
//
// This is intended for bring-up before the payload is fully integrated into the
// display pipeline.
esp_err_t wifi_rx_print_header_console(const wifi_rx_header_t *header);

// Test helper: print one received payload as frame/slice data to the console.
//
// The payload size depends on data_type:
// - none: no payload
// - display_off: no payload
// - still_3d: 1 * slice_count * DISPLAY_SLICE_BYTES
// - animation_3d: frame_count * slice_count * DISPLAY_SLICE_BYTES
//
// Slice data is printed in hexadecimal with frame and slice labels.
esp_err_t wifi_rx_print_payload_slices_console(const wifi_rx_header_t *header,
                                               const uint8_t *payload,
                                               size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_RX_H
