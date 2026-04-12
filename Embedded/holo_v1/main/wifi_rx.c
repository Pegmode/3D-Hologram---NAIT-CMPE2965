#include "wifi_rx.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_rom_crc.h"
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

#include "display_task.h"
#include "display_store.h"
#include "main.h"

// Default runtime values used when the caller does not override them.
//
// These give the module a usable baseline during early bring-up.
#define WIFI_RX_DEFAULT_TCP_PORT       3333U
#define WIFI_RX_DEFAULT_LISTEN_BACKLOG 1U
#define WIFI_RX_DEFAULT_TASK_STACK     4096
#define WIFI_RX_DEFAULT_TASK_PRIORITY  6

// Wi-Fi connection event bits.
//
// The receive task waits on this before opening its TCP listener.
#define WIFI_RX_CONNECTED_BIT BIT0

// Fixed size of the on-wire packet header.
//
// This matches wifi_rx_wire_header_t exactly.
#define WIFI_RX_WIRE_HEADER_SIZE ((uint8_t)sizeof(wifi_rx_wire_header_t))

static const char *TAG_wifi_rx = "wifi_rx";

wifi_rx_config_t wifi_rx_config = {
    .init = -1,
    .wifi_ssid = { 0 },
    .wifi_password = { 0 },
    .tcp_port = WIFI_RX_DEFAULT_TCP_PORT,
    .listen_backlog = WIFI_RX_DEFAULT_LISTEN_BACKLOG,
    .max_connections = 1,
    .task_stack_size = WIFI_RX_DEFAULT_TASK_STACK,
    .task_priority = WIFI_RX_DEFAULT_TASK_PRIORITY,
    .print_headers_to_console = true,
};

static EventGroupHandle_t s_wifi_event_group = NULL;
static TaskHandle_t s_wifi_rx_task_handle = NULL;
static bool s_initialized = false;
static wifi_rx_header_t s_last_header = { 0 };
static esp_event_handler_instance_t s_wifi_event_instance = NULL;
static esp_netif_t *s_wifi_ap_netif = NULL;

static void wifi_rx_task(void *arg);

// Return how many frames should be used when interpreting the payload.
//
// A still image only carries one frame of slice data, so frame_count may be
// omitted on the wire by sending -1.
static esp_err_t wifi_rx_get_effective_frame_count(const wifi_rx_header_t *header, size_t *frame_count)
{
    if (header == NULL || frame_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (header->data_type) {
        case WIFI_RX_DATA_TYPE_NONE:
        case WIFI_RX_DATA_TYPE_DISPLAYOFF:
            *frame_count = 0;
            return ESP_OK;

        case WIFI_RX_DATA_TYPE_STILL_3D:
            if (header->frame_count == -1 || header->frame_count == 1) {
                *frame_count = 1;
                return ESP_OK;
            }
            return ESP_ERR_INVALID_ARG;

        case WIFI_RX_DATA_TYPE_ANIMATION_3D:
            if (header->frame_count <= 0) {
                return ESP_ERR_INVALID_ARG;
            }
            *frame_count = (size_t)header->frame_count;
            return ESP_OK;

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

// Return the expected payload size implied by the parsed header.
//
// If payload_bytes is -1, the packet intentionally carries no payload.
static esp_err_t wifi_rx_get_expected_payload_bytes(const wifi_rx_header_t *header, size_t *payload_bytes)
{
    size_t frame_count = 0;

    if (header == NULL || payload_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (header->payload_bytes == -1) {
        *payload_bytes = 0;
        return ESP_OK;
    }

    if (header->slice_count <= 0 || header->payload_bytes < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        wifi_rx_get_effective_frame_count(header, &frame_count),
        TAG_wifi_rx,
        "wifi_rx_get_effective_frame_count failed"
    );

    *payload_bytes = frame_count *
                     (size_t)header->slice_count *
                     (size_t)DISPLAY_SLICE_BYTES;

    return ESP_OK;
}

// Handle Wi-Fi and IP events needed by the receive task.
//
// This keeps the policy simple:
// - signal the TCP task when the SoftAP starts
// - log when a client joins or leaves the SoftAP
static void wifi_rx_event_handler(void *arg,
                                  esp_event_base_t event_base,
                                  int32_t event_id,
                                  void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_RX_CONNECTED_BIT);
        }

        ESP_LOGI(TAG_wifi_rx, "SoftAP started");
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG_wifi_rx, "SoftAP client connected: aid=%d", event->aid);
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG_wifi_rx, "SoftAP client disconnected: aid=%d", event->aid);
    }
}

static esp_err_t wifi_rx_init_nvs(void)
{
    // Wi-Fi uses NVS internally, so make sure flash storage is ready first.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG_wifi_rx, "nvs_flash_erase failed");
        err = nvs_flash_init();
    }

    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }

    return err;
}

static esp_err_t wifi_rx_wait_for_connection(TickType_t wait_ticks)
{
    EventBits_t bits;

    if (s_wifi_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Block until the SoftAP has started and is ready to accept TCP clients.
    bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_RX_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        wait_ticks
    );

    if ((bits & WIFI_RX_CONNECTED_BIT) == 0U) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static int wifi_rx_create_server_socket(void)
{
    int listen_sock = -1;
    int opt_value = 1;
    struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(wifi_rx_config.tcp_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    // Open a simple IPv4 TCP listening socket.
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG_wifi_rx, "socket failed: errno %d", errno);
        return -1;
    }

    // SO_REUSEADDR makes repeated bring-up tests less annoying.
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(opt_value)) != 0) {
        ESP_LOGW(TAG_wifi_rx, "setsockopt(SO_REUSEADDR) failed: errno %d", errno);
    }

    if (bind(listen_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        ESP_LOGE(TAG_wifi_rx, "bind failed: errno %d", errno);
        close(listen_sock);
        return -1;
    }

    if (listen(listen_sock, wifi_rx_config.listen_backlog) != 0) {
        ESP_LOGE(TAG_wifi_rx, "listen failed: errno %d", errno);
        close(listen_sock);
        return -1;
    }

    return listen_sock;
}

static esp_err_t wifi_rx_recv_exact(int sock, void *buf, size_t len)
{
    uint8_t *write_ptr = (uint8_t *)buf;
    size_t total_received = 0;

    // TCP is a byte stream, so one recv call is not guaranteed to return the
    // entire header or payload chunk we asked for.
    while (total_received < len) {
        int ret = recv(sock, write_ptr + total_received, len - total_received, 0);
        if (ret == 0) {
            return ESP_FAIL;
        }

        if (ret < 0) {
            ESP_LOGW(TAG_wifi_rx, "recv failed: errno %d", errno);
            return ESP_FAIL;
        }

        total_received += (size_t)ret;
    }

    return ESP_OK;
}

static esp_err_t wifi_rx_receive_payload(int sock, const wifi_rx_header_t *header, uint8_t **payload_buf)
{
    uint8_t *buf = NULL;
    size_t expected_payload_bytes = 0;

    if (header == NULL || payload_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        wifi_rx_get_expected_payload_bytes(header, &expected_payload_bytes),
        TAG_wifi_rx,
        "wifi_rx_get_expected_payload_bytes failed"
    );

    if (header->payload_bytes == -1) {
        *payload_buf = NULL;
        return ESP_OK;
    }

    if ((size_t)header->payload_bytes != expected_payload_bytes) {
        ESP_LOGE(TAG_wifi_rx,
                 "Payload size mismatch: header=%" PRId32 " expected=%u",
                 header->payload_bytes,
                 (unsigned)expected_payload_bytes);
        return ESP_ERR_INVALID_ARG;
    }

    buf = heap_caps_malloc(expected_payload_bytes, MALLOC_CAP_8BIT);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (wifi_rx_recv_exact(sock, buf, expected_payload_bytes) != ESP_OK) {
        heap_caps_free(buf);
        return ESP_FAIL;
    }

    *payload_buf = buf;
    return ESP_OK;
}

static esp_err_t wifi_rx_validate_payload_crc32(const wifi_rx_header_t *header,
                                                const uint8_t *payload,
                                                size_t payload_len)
{
    uint32_t calculated_crc32;

    if (header == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (header->payload_bytes == -1) {
        if (header->payload_crc32 != 0U) {
            ESP_LOGE(TAG_wifi_rx, "Packets without payload must use payload_crc32 = 0");
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }

    if (payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // For standard reflected CRC-32 (poly 0x04C11DB7, init 0xFFFFFFFF,
    // xorout 0xFFFFFFFF), ESP-IDF's ROM helper should be called with 0 here.
    // That matches the common CRC-32 value most PC libraries produce.
    calculated_crc32 = esp_rom_crc32_le(0, payload, payload_len);
    if (calculated_crc32 != header->payload_crc32) {
        ESP_LOGE(TAG_wifi_rx,
                 "Payload CRC mismatch: header=0x%08" PRIX32 " calculated=0x%08" PRIX32,
                 header->payload_crc32,
                 calculated_crc32);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

static esp_err_t wifi_rx_discard_payload(int sock, int32_t payload_bytes)
{
    uint8_t discard_buf[256];
    uint32_t bytes_remaining;

    if (payload_bytes <= 0) {
        return ESP_OK;
    }

    bytes_remaining = (uint32_t)payload_bytes;

    // For now the receiver only validates and reports the header.
    // Drain the payload so the next packet starts on the correct boundary.
    while (bytes_remaining > 0U) {
        size_t chunk_size = bytes_remaining;
        if (chunk_size > sizeof(discard_buf)) {
            chunk_size = sizeof(discard_buf);
        }

        ESP_RETURN_ON_ERROR(
            wifi_rx_recv_exact(sock, discard_buf, chunk_size),
            TAG_wifi_rx,
            "wifi_rx_recv_exact payload failed"
        );

        bytes_remaining -= (uint32_t)chunk_size;
    }

    return ESP_OK;
}

static esp_err_t wifi_rx_parse_header(const wifi_rx_wire_header_t *wire_header,
                                      wifi_rx_header_t *parsed_header)
{
    if (wire_header == NULL || parsed_header == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Convert every multibyte field from network byte order into host order.
    parsed_header->magic = ntohl(wire_header->magic);
    parsed_header->version = wire_header->version;
    parsed_header->header_size_bytes = wire_header->header_size_bytes;
    parsed_header->data_type = (wifi_rx_data_type_t)wire_header->data_type;
    parsed_header->frame_count = (int32_t)ntohl((uint32_t)wire_header->frame_count);
    parsed_header->slice_count = (int32_t)ntohl((uint32_t)wire_header->slice_count);
    parsed_header->payload_bytes = (int32_t)ntohl((uint32_t)wire_header->payload_bytes);
    parsed_header->motor_speed_rpm = (int16_t)ntohs((uint16_t)wire_header->motor_speed_rpm);
    parsed_header->payload_crc32 = ntohl(wire_header->payload_crc32);

    // Validate the header before any payload processing starts.
    if (parsed_header->magic != WIFI_RX_HEADER_MAGIC) {
        ESP_LOGE(TAG_wifi_rx, "Invalid packet magic: 0x%08" PRIX32, parsed_header->magic);
        return ESP_FAIL;
    }

    if (parsed_header->version != WIFI_RX_HEADER_VERSION) {
        ESP_LOGE(TAG_wifi_rx, "Unsupported header version: %u", parsed_header->version);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (parsed_header->header_size_bytes != WIFI_RX_WIRE_HEADER_SIZE) {
        ESP_LOGE(TAG_wifi_rx, "Unexpected header size: %u", parsed_header->header_size_bytes);
        return ESP_ERR_INVALID_ARG;
    }

    if (parsed_header->data_type != WIFI_RX_DATA_TYPE_NONE &&
        parsed_header->data_type != WIFI_RX_DATA_TYPE_STILL_3D &&
        parsed_header->data_type != WIFI_RX_DATA_TYPE_ANIMATION_3D &&
        parsed_header->data_type != WIFI_RX_DATA_TYPE_DISPLAYOFF) {
        ESP_LOGE(TAG_wifi_rx, "Unsupported data type: %d", (int)parsed_header->data_type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (parsed_header->motor_speed_rpm < -1) {
        ESP_LOGE(TAG_wifi_rx, "Invalid motor speed command: %" PRId16, parsed_header->motor_speed_rpm);
        return ESP_ERR_INVALID_ARG;
    }

    switch (parsed_header->data_type) {
        case WIFI_RX_DATA_TYPE_NONE:
        case WIFI_RX_DATA_TYPE_DISPLAYOFF:
            if (parsed_header->frame_count != -1 ||
                parsed_header->slice_count != -1 ||
                parsed_header->payload_bytes != -1) {
                ESP_LOGE(TAG_wifi_rx, "Control packets must use -1 for frame_count, slice_count, and payload_bytes");
                return ESP_ERR_INVALID_ARG;
            }
            if (parsed_header->payload_crc32 != 0U) {
                ESP_LOGE(TAG_wifi_rx, "Control packets must use payload_crc32 = 0");
                return ESP_ERR_INVALID_ARG;
            }
            break;

        case WIFI_RX_DATA_TYPE_STILL_3D:
            if (parsed_header->slice_count <= 0 || parsed_header->payload_bytes <= 0) {
                ESP_LOGE(TAG_wifi_rx, "Still packets require positive slice_count and payload_bytes");
                return ESP_ERR_INVALID_ARG;
            }
            if (parsed_header->frame_count != -1 && parsed_header->frame_count != 1) {
                ESP_LOGE(TAG_wifi_rx, "Still packets must use frame_count = -1 or 1");
                return ESP_ERR_INVALID_ARG;
            }
            break;

        case WIFI_RX_DATA_TYPE_ANIMATION_3D:
            if (parsed_header->frame_count <= 0 ||
                parsed_header->slice_count <= 0 ||
                parsed_header->payload_bytes <= 0) {
                ESP_LOGE(TAG_wifi_rx, "Animation packets require positive frame_count, slice_count, and payload_bytes");
                return ESP_ERR_INVALID_ARG;
            }
            break;

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t wifi_rx_receive_packet_header(int sock, wifi_rx_header_t *parsed_header)
{
    wifi_rx_wire_header_t wire_header = { 0 };

    // Always read one full fixed-size header before touching the payload.
    ESP_RETURN_ON_ERROR(
        wifi_rx_recv_exact(sock, &wire_header, sizeof(wire_header)),
        TAG_wifi_rx,
        "wifi_rx_recv_exact header failed"
    );

    return wifi_rx_parse_header(&wire_header, parsed_header);
}

static esp_err_t wifi_rx_process_client(int client_sock)
{
    // A connected TCP client may send multiple packets back-to-back.
    while (1) {
        wifi_rx_header_t header = { 0 };
        uint8_t *payload_buf = NULL;
        esp_err_t err = wifi_rx_receive_packet_header(client_sock, &header);
        if (err != ESP_OK) {
            return err;
        }

        // Save the most recent parsed header for debug access from elsewhere.
        s_last_header = header;

        ESP_LOGI(TAG_wifi_rx,
                 "Packet header received: type=%s frames=%" PRId32 " slices=%" PRId32 " bytes=%" PRId32,
                 wifi_rx_data_type_to_string(header.data_type),
                 header.frame_count,
                 header.slice_count,
                 header.payload_bytes);

        // Print to the USB console during bring-up so packet formatting can be
        // verified before the payload path is fully implemented.
        if (wifi_rx_config.print_headers_to_console) {
            err = wifi_rx_print_header_console(&header);
            if (err != ESP_OK) {
                ESP_LOGW(TAG_wifi_rx, "wifi_rx_print_header_console failed: %s", esp_err_to_name(err));
            }
        }

        // Receive the payload and prepare it for the staging display store.
        err = wifi_rx_receive_payload(client_sock, &header, &payload_buf);
        if (err != ESP_OK) {
            // If the payload shape is not one we can parse yet, discard it so
            // the stream stays aligned for the next packet.
            ESP_LOGW(TAG_wifi_rx, "wifi_rx_receive_payload failed: %s", esp_err_to_name(err));
            ESP_RETURN_ON_ERROR(
                wifi_rx_discard_payload(client_sock, header.payload_bytes),
                TAG_wifi_rx,
                "wifi_rx_discard_payload failed"
            );
            continue;
        }

        if (payload_buf == NULL) {
            if (header.data_type == WIFI_RX_DATA_TYPE_DISPLAYOFF) {
                err = display_task_request_display_off();
                if (err != ESP_OK) {
                    ESP_LOGW(TAG_wifi_rx, "display_task_request_display_off failed: %s", esp_err_to_name(err));
                } else {
                    ESP_LOGI(TAG_wifi_rx, "Display off requested");
                }
            }
            continue;
        }

        err = wifi_rx_validate_payload_crc32(&header, payload_buf, (size_t)header.payload_bytes);
        if (err != ESP_OK) {
            ESP_LOGW(TAG_wifi_rx, "wifi_rx_validate_payload_crc32 failed: %s", esp_err_to_name(err));
            heap_caps_free(payload_buf);
            continue;
        }

        err = display_store_stage_from_payload(&header, payload_buf, (size_t)header.payload_bytes);
        if (err != ESP_OK) {
            ESP_LOGW(TAG_wifi_rx, "display_store_stage_from_payload failed: %s", esp_err_to_name(err));
            heap_caps_free(payload_buf);
            continue;
        }

        err = display_store_request_swap();
        if (err != ESP_OK) {
            ESP_LOGW(TAG_wifi_rx, "display_store_request_swap failed: %s", esp_err_to_name(err));
            heap_caps_free(payload_buf);
            continue;
        }

        err = display_store_print_staging_triggers_console();
        if (err != ESP_OK) {
            ESP_LOGW(TAG_wifi_rx, "display_store_print_staging_triggers_console failed: %s", esp_err_to_name(err));
        }

        ESP_LOGI(TAG_wifi_rx, "New display payload staged; swap pending at next Z");

        heap_caps_free(payload_buf);
    }
}

static void wifi_rx_task(void *arg)
{
    (void)arg;

    // Keep the task alive forever. If Wi-Fi or a client drops, loop back around
    // and wait for the next valid connection.
    while (1) {
        int listen_sock = -1;

        if (wifi_rx_wait_for_connection(portMAX_DELAY) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        // Only open the TCP server once the SoftAP is up.
        //
        // Connection model for the finished system:
        // - ESP32 creates a SoftAP network
        // - ESP32 listens as the TCP server
        // - PC control software joins the SoftAP and connects as the TCP client
        listen_sock = wifi_rx_create_server_socket();
        if (listen_sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG_wifi_rx, "TCP listener ready on port %u", wifi_rx_config.tcp_port);

        while (1) {
            struct sockaddr_storage client_addr = { 0 };
            socklen_t client_addr_len = sizeof(client_addr);
            // Accept one client connection at a time for predictable bring-up.
            int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);

            if (client_sock < 0) {
                ESP_LOGW(TAG_wifi_rx, "accept failed: errno %d", errno);
                break;
            }

            ESP_LOGI(TAG_wifi_rx, "TCP client connected");

            // Process packets until the client disconnects or a stream error occurs.
            if (wifi_rx_process_client(client_sock) != ESP_OK) {
                ESP_LOGW(TAG_wifi_rx, "Client disconnected or packet processing failed");
            }

            shutdown(client_sock, 0);
            close(client_sock);
        }

        close(listen_sock);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

esp_err_t wifi_rx_init(void)
{
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_cfg = { 0 };
    size_t ssid_len;
    size_t password_len;

    if (wifi_rx_config.init <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        ESP_LOGW(TAG_wifi_rx, "wifi_rx_init called more than once");
        return ESP_OK;
    }

    // Validate the caller-provided configuration before touching the network stack.
    ssid_len = strnlen(wifi_rx_config.wifi_ssid, sizeof(wifi_rx_config.wifi_ssid));
    password_len = strnlen(wifi_rx_config.wifi_password, sizeof(wifi_rx_config.wifi_password));

    if (ssid_len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (wifi_rx_config.tcp_port == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (wifi_rx_config.task_stack_size <= 0 || wifi_rx_config.task_priority <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (wifi_rx_config.max_connections == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(wifi_rx_init_nvs(), TAG_wifi_rx, "wifi_rx_init_nvs failed");

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    // Bring up the shared ESP-IDF networking support.
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_wifi_rx, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // The default event loop is used by the Wi-Fi events below.
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_wifi_rx, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create the default SoftAP interface used by the Wi-Fi driver.
    s_wifi_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_wifi_ap_netif == NULL) {
        return ESP_FAIL;
    }

    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_wifi_rx, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_rx_event_handler,
                                            NULL,
                                            &s_wifi_event_instance),
        TAG_wifi_rx,
        "WIFI_EVENT handler register failed"
    );

    // Copy the application config into the ESP-IDF SoftAP config.
    memcpy(wifi_cfg.ap.ssid, wifi_rx_config.wifi_ssid, ssid_len);
    memcpy(wifi_cfg.ap.password, wifi_rx_config.wifi_password, password_len);
    wifi_cfg.ap.ssid_len = (uint8_t)ssid_len;
    wifi_cfg.ap.max_connection = wifi_rx_config.max_connections;
    wifi_cfg.ap.authmode = (password_len == 0U) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_cfg.ap.pmf_cfg.capable = true;
    wifi_cfg.ap.pmf_cfg.required = false;

    // Start the SoftAP. Once it is up, the TCP listener on core 1 can accept
    // connections from the PC software.
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG_wifi_rx, "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg), TAG_wifi_rx, "esp_wifi_set_config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG_wifi_rx, "esp_wifi_start failed");

    if (s_wifi_ap_netif != NULL) {
        esp_netif_ip_info_t ip_info = { 0 };
        if (esp_netif_get_ip_info(s_wifi_ap_netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG_wifi_rx, "SoftAP IP address: " IPSTR, IP2STR(&ip_info.ip));
        }
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t wifi_rx_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_wifi_rx_task_handle != NULL) {
        ESP_LOGW(TAG_wifi_rx, "wifi_rx_start called more than once");
        return ESP_OK;
    }

    // Pin network receive work to core 1 so it stays away from the encoder and
    // display timing path that is intended to live on core 0.
    if (xTaskCreatePinnedToCore(
            wifi_rx_task,
            "wifi_rx_task",
            (uint32_t)wifi_rx_config.task_stack_size,
            NULL,
            (UBaseType_t)wifi_rx_config.task_priority,
            &s_wifi_rx_task_handle,
            1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool wifi_rx_is_initialized(void)
{
    return s_initialized;
}

esp_err_t wifi_rx_get_last_header(wifi_rx_header_t *header)
{
    if (header == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *header = s_last_header;
    return ESP_OK;
}

const char *wifi_rx_data_type_to_string(wifi_rx_data_type_t data_type)
{
    switch (data_type) {
        case WIFI_RX_DATA_TYPE_NONE:
            return "none";
        case WIFI_RX_DATA_TYPE_STILL_3D:
            return "still_3d";
        case WIFI_RX_DATA_TYPE_ANIMATION_3D:
            return "animation_3d";
        case WIFI_RX_DATA_TYPE_DISPLAYOFF:
            return "display_off";
        default:
            return "invalid";
    }
}

esp_err_t wifi_rx_print_header_console(const wifi_rx_header_t *header)
{
    if (header == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG_wifi_rx,
             "wifi header: type=%s frames=%" PRId32 " slices=%" PRId32 " bytes=%" PRId32,
             wifi_rx_data_type_to_string(header->data_type),
             header->frame_count,
             header->slice_count,
             header->payload_bytes);

    ESP_LOGI(TAG_wifi_rx,
             "wifi header: motor_cmd=%" PRId16 " (-1=off 0=same >0=set_rpm)",
             header->motor_speed_rpm);

    ESP_LOGI(TAG_wifi_rx,
             "wifi header: magic=0x%08" PRIX32 " version=%u header_size=%u crc32=0x%08" PRIX32,
             header->magic,
             header->version,
             header->header_size_bytes,
             header->payload_crc32);

    return ESP_OK;
}

esp_err_t wifi_rx_print_payload_slices_console(const wifi_rx_header_t *header,
                                               const uint8_t *payload,
                                               size_t payload_len)
{
    size_t expected_payload_bytes = 0;
    size_t effective_frame_count = 0;
    size_t frame_index;
    size_t slice_index;
    size_t payload_offset = 0;

    if (header == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        wifi_rx_get_expected_payload_bytes(header, &expected_payload_bytes),
        TAG_wifi_rx,
        "wifi_rx_get_expected_payload_bytes failed"
    );

    ESP_RETURN_ON_ERROR(
        wifi_rx_get_effective_frame_count(header, &effective_frame_count),
        TAG_wifi_rx,
        "wifi_rx_get_effective_frame_count failed"
    );

    if (payload_len != expected_payload_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    for (frame_index = 0; frame_index < effective_frame_count; frame_index++) {
        for (slice_index = 0; slice_index < (size_t)header->slice_count; slice_index++) {
            ESP_LOGI(TAG_wifi_rx,
                     "wifi payload: frame=%u slice=%u",
                     (unsigned)frame_index,
                     (unsigned)slice_index);

            // Print each 64-byte slice as four 16-byte rows to keep the output readable.
            for (size_t row = 0; row < DISPLAY_SLICE_BYTES; row += 16U) {
                ESP_LOGI(TAG_wifi_rx,
                         "  %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                         payload[payload_offset + row + 0U],
                         payload[payload_offset + row + 1U],
                         payload[payload_offset + row + 2U],
                         payload[payload_offset + row + 3U],
                         payload[payload_offset + row + 4U],
                         payload[payload_offset + row + 5U],
                         payload[payload_offset + row + 6U],
                         payload[payload_offset + row + 7U],
                         payload[payload_offset + row + 8U],
                         payload[payload_offset + row + 9U],
                         payload[payload_offset + row + 10U],
                         payload[payload_offset + row + 11U],
                         payload[payload_offset + row + 12U],
                         payload[payload_offset + row + 13U],
                         payload[payload_offset + row + 14U],
                         payload[payload_offset + row + 15U]);
            }

            payload_offset += DISPLAY_SLICE_BYTES;
        }
    }

    return ESP_OK;
}
