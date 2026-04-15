#include "encoder.h"

#include <stddef.h>

#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

static const char *TAG_encoder = "encoder";

// Saved configuration
encoder_config_t encoder_config = {
	.init = -1,
    .pin_a = -1,                  // Encoder A channel
    .pin_b = -1,                 // Encoder B channel
    .pin_z = -1,                  // Encoder Z / index channel, or -1 if not used

    .ab_pull = -1,   // Pull mode for A/B pins
    .z_pull = -1,    // Pull mode for Z pin

    .glitch_filter_ns = -1  // PCNT glitch filter length in ns
};

// PCNT handles
static pcnt_unit_handle_t g_pcnt_unit = NULL;
static pcnt_channel_handle_t g_pcnt_chan = NULL;

// Optional task to notify when Z occurs
static TaskHandle_t z_notify_task = NULL;
static TaskHandle_t count_notify_task = NULL;

// Counts how many Z pulses have occurred
static volatile uint32_t z_rev_count = 0;
// Most recent watch point seen by the PCNT ISR.
//
// This simpler implementation keeps only the latest fired threshold. If
// several watch points fire before the task handles the notification, newer
// values overwrite older ones.
static volatile int last_watch_point = 0;
// Values currently loaded into the PCNT watch-point hardware.
//
// The PCNT peripheral owns the actual thresholds. This array only tracks which
// ones the firmware added so they can be removed and replaced later.
static int *loaded_watch_points = NULL;
static size_t loaded_watch_point_count = 0;
static size_t loaded_watch_point_capacity = 0;

// Tracks whether the module has been initialized
static bool encoder_initialized = false;

// Ensure there is enough RAM to remember all active watch points.
static esp_err_t encoder_reserve_watch_point_capacity(size_t required_capacity)
{
    int *resized_watch_points;
    size_t new_capacity;

    if (required_capacity <= loaded_watch_point_capacity) {
        return ESP_OK;
    }

    new_capacity = loaded_watch_point_capacity;
    if (new_capacity == 0U) {
        new_capacity = 8U;
    }

    while (new_capacity < required_capacity) {
        new_capacity *= 2U;
    }

    resized_watch_points = (int *)heap_caps_realloc(
        loaded_watch_points,
        new_capacity * sizeof(*loaded_watch_points),
        MALLOC_CAP_8BIT
    );
    if (resized_watch_points == NULL) {
        return ESP_ERR_NO_MEM;
    }

    loaded_watch_points = resized_watch_points;
    loaded_watch_point_capacity = new_capacity;

    return ESP_OK;
}

// --------------------------------------------------
// Small helper: configure an input pin
// --------------------------------------------------

esp_err_t encoder_config_input_pin(int pin, gpio_pull_mode_t pull_mode, gpio_int_type_t intr_type)
{
    if (pin < 0) {
        return ESP_OK;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = (pull_mode == GPIO_PULLUP_ONLY) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (pull_mode == GPIO_PULLDOWN_ONLY) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = intr_type
    };

    return gpio_config(&cfg);
}

// --------------------------------------------------
// Z index interrupt handler
//
// Keep this very short.
// We only:
// 1) increment revolution counter
// 2) optionally notify a task
// --------------------------------------------------

void encoder_z_isr(void *arg)
{
    (void)arg;

    z_rev_count++;

    BaseType_t higher_prio_woken = pdFALSE;

    if (z_notify_task != NULL) {
        xTaskNotifyFromISR(
            z_notify_task,
            ENCODER_NOTIFY_EVENT_Z,
            eSetBits,
            &higher_prio_woken
        );
    }

    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

static bool encoder_pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    BaseType_t higher_prio_woken = pdFALSE;

    (void)unit;
    (void)user_ctx;

    last_watch_point = edata->watch_point_value;

    if (count_notify_task != NULL) {
        xTaskNotifyFromISR(
            count_notify_task,
            ENCODER_NOTIFY_EVENT_COUNT,
            eSetBits,
            &higher_prio_woken
        );
    }

    return (higher_prio_woken == pdTRUE);
}

// --------------------------------------------------
// Initialize PCNT for quadrature-style counting
//
// Simple method:
// - A is the pulse input
// - B is the direction control input
// - count the rising edge of A
// --------------------------------------------------

esp_err_t encoder_init_pcnt(void)
{
    const pcnt_event_callbacks_t cbs = {
        .on_reach = encoder_pcnt_on_reach,
    };

    // Create PCNT unit
    pcnt_unit_config_t unit_cfg = {
        .high_limit = 32767,
        .low_limit  = -32768,
    };

    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, &g_pcnt_unit), TAG_encoder, "pcnt_new_unit failed");

    // Optional glitch filter to reject short noise spikes
    if (encoder_config.glitch_filter_ns > 0) {
        pcnt_glitch_filter_config_t filter_cfg = {
            .max_glitch_ns = encoder_config.glitch_filter_ns,
        };

        ESP_RETURN_ON_ERROR(
            pcnt_unit_set_glitch_filter(g_pcnt_unit, &filter_cfg),
            TAG_encoder,
            "pcnt_unit_set_glitch_filter failed"
        );
    }

    // A = edge input, B = level input
    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num  = encoder_config.pin_a,
        .level_gpio_num = encoder_config.pin_b,
    };

    ESP_RETURN_ON_ERROR(
        pcnt_new_channel(g_pcnt_unit, &chan_cfg, &g_pcnt_chan),
        TAG_encoder,
        "pcnt_new_channel failed"
    );

    // Count only the rising edge of A.
    //
    // If x2 counting is needed later, change the falling-edge action from
    // HOLD to INCREASE and update ENC_COUNT_MULTIPLIER to match.
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_edge_action(
            g_pcnt_chan,
            PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // rising edge of A
            PCNT_CHANNEL_EDGE_ACTION_INCREASE        // falling edge of A
        ),
        TAG_encoder,
        "pcnt_channel_set_edge_action failed"
    );

    // Use B to control direction
    //
    // If the direction is backwards on your hardware,
    // swap KEEP and INVERSE here.
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_level_action(
            g_pcnt_chan,
            PCNT_CHANNEL_LEVEL_ACTION_KEEP,      // B high
            PCNT_CHANNEL_LEVEL_ACTION_KEEP    // B low
        ),
        TAG_encoder,
        "pcnt_channel_set_level_action failed"
    );

    ESP_RETURN_ON_ERROR(
        pcnt_unit_register_event_callbacks(g_pcnt_unit, &cbs, NULL),
        TAG_encoder,
        "pcnt_unit_register_event_callbacks failed"
    );

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(g_pcnt_unit), TAG_encoder, "pcnt_unit_enable failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(g_pcnt_unit), TAG_encoder, "pcnt_unit_clear_count failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(g_pcnt_unit), TAG_encoder, "pcnt_unit_start failed");

    return ESP_OK;
}

// --------------------------------------------------
// Initialize optional Z index interrupt
// --------------------------------------------------

esp_err_t encoder_init_z(void)
{
    esp_err_t err;

    // No Z pin configured -> nothing to do
    if (encoder_config.pin_z < 0) {
        return ESP_OK;
    }

    // Configure Z input with rising-edge interrupt
    ESP_RETURN_ON_ERROR(
        encoder_config_input_pin(encoder_config.pin_z, encoder_config.z_pull, GPIO_INTR_POSEDGE),
        TAG_encoder,
        "Z pin config failed"
    );

    // Install the shared GPIO ISR service if not already installed
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_encoder, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return err;
    }

    // Attach the Z interrupt handler
    ESP_RETURN_ON_ERROR(
        gpio_isr_handler_add(encoder_config.pin_z, encoder_z_isr, NULL),
        TAG_encoder,
        "gpio_isr_handler_add failed"
    );

    return ESP_OK;
}



esp_err_t encoder_init()
{

	if (encoder_config.init <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (encoder_config.pin_a < 0 || encoder_config.pin_b < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (encoder_initialized) {
        ESP_LOGW(TAG_encoder, "encoder_init called more than once");
        return ESP_OK;
    }

    // Configure A/B pins as inputs
    ESP_RETURN_ON_ERROR(
        encoder_config_input_pin(encoder_config.pin_a, encoder_config.ab_pull, GPIO_INTR_DISABLE),
        TAG_encoder,
        "A pin config failed"
    );

    ESP_RETURN_ON_ERROR(
        encoder_config_input_pin(encoder_config.pin_b, encoder_config.ab_pull, GPIO_INTR_DISABLE),
        TAG_encoder,
        "B pin config failed"
    );

    // Set up pulse counter
    ESP_RETURN_ON_ERROR(encoder_init_pcnt(), TAG_encoder, "encoder_init_pcnt failed");

    // Set up optional Z interrupt
    ESP_RETURN_ON_ERROR(encoder_init_z(), TAG_encoder, "encoder_init_z failed");

    encoder_initialized = true;

    ESP_LOGI(TAG_encoder, "Encoder initialized");
    ESP_LOGI(TAG_encoder, "A=%d  B=%d  Z=%d", encoder_config.pin_a, encoder_config.pin_b, encoder_config.pin_z);

    return ESP_OK;
}

void encoder_set_z_notify_task(TaskHandle_t task_handle)
{
    z_notify_task = task_handle;
}

void encoder_set_count_notify_task(TaskHandle_t task_handle)
{
    count_notify_task = task_handle;
}

esp_err_t encoder_set_count_watch_point(int watch_point)
{
    ESP_RETURN_ON_ERROR(
        encoder_clear_all_count_watch_points(),
        TAG_encoder,
        "encoder_clear_all_count_watch_points failed"
    );

    return encoder_add_count_watch_point(watch_point);
}

esp_err_t encoder_add_count_watch_point(int watch_point)
{
    size_t watch_point_index;

    if (!encoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for (watch_point_index = 0; watch_point_index < loaded_watch_point_count; watch_point_index++) {
        if (loaded_watch_points[watch_point_index] == watch_point) {
            ESP_LOGW(TAG_encoder, "Watch point %d is already loaded", watch_point);
            return ESP_OK;
        }
    }

    ESP_RETURN_ON_ERROR(
        encoder_reserve_watch_point_capacity(loaded_watch_point_count + 1U),
        TAG_encoder,
        "encoder_reserve_watch_point_capacity failed"
    );

    ESP_RETURN_ON_ERROR(
        pcnt_unit_add_watch_point(g_pcnt_unit, watch_point),
        TAG_encoder,
        "pcnt_unit_add_watch_point failed"
    );

    loaded_watch_points[loaded_watch_point_count] = watch_point;
    loaded_watch_point_count++;

    return ESP_OK;
}

esp_err_t encoder_load_count_watch_points(const int32_t *watch_points, size_t watch_point_count)
{
    size_t watch_point_index;
    esp_err_t err;

    if (!encoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (watch_point_count > 0U && watch_points == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        encoder_clear_all_count_watch_points(),
        TAG_encoder,
        "encoder_clear_all_count_watch_points failed"
    );

    for (watch_point_index = 0; watch_point_index < watch_point_count; watch_point_index++) {
        err = encoder_add_count_watch_point((int)watch_points[watch_point_index]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_encoder,
                     "Failed to load watch point %ld at index %u",
                     (long)watch_points[watch_point_index],
                     (unsigned)watch_point_index);

            // Clear any partial load so the hardware state stays predictable.
            encoder_clear_all_count_watch_points();
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t encoder_clear_all_count_watch_points(void)
{
    size_t watch_point_index;
    esp_err_t err;

    if (!encoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    for (watch_point_index = 0; watch_point_index < loaded_watch_point_count; watch_point_index++) {
        err = pcnt_unit_remove_watch_point(g_pcnt_unit, loaded_watch_points[watch_point_index]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_encoder,
                     "pcnt_unit_remove_watch_point failed for %d: %s",
                     loaded_watch_points[watch_point_index],
                     esp_err_to_name(err));
            return err;
        }
    }

    loaded_watch_point_count = 0U;

    return ESP_OK;
}

esp_err_t encoder_clear_count_watch_point(void)
{
    return encoder_clear_all_count_watch_points();
}

esp_err_t encoder_get_last_watch_point(int *watch_point)
{
    if (watch_point == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!encoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *watch_point = last_watch_point;
    return ESP_OK;
}

esp_err_t encoder_get_count(int *count)
{
    if (!encoder_initialized || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_get_count(g_pcnt_unit, count);
}

esp_err_t encoder_clear_count(void)
{
    if (!encoder_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return pcnt_unit_clear_count(g_pcnt_unit);
}

uint32_t encoder_get_revolution_count(void)
{
    return z_rev_count;
}
