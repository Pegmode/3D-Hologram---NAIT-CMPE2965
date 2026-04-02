#include "encoder.h"

#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_check.h"

// --------------------------------------------------
// Private module state
// --------------------------------------------------

static const char *TAG_encoder = "encoder";

// Saved configuration
encoder_config_t encoder_config = {
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
static TaskHandle_t g_z_notify_task = NULL;

// Counts how many Z pulses have occurred
static volatile uint32_t g_rev_count = 0;

// Tracks whether the module has been initialized
static bool encoder_initialized = false;

// --------------------------------------------------
// Small helper: configure an input pin
// --------------------------------------------------

static esp_err_t encoder_config_input_pin(int pin, gpio_pull_mode_t pull_mode, gpio_int_type_t intr_type)
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

static void encoder_z_isr(void *arg)
{
    (void)arg;

    g_rev_count++;

    BaseType_t higher_prio_woken = pdFALSE;

    if (g_z_notify_task != NULL) {
        vTaskNotifyGiveFromISR(g_z_notify_task, &higher_prio_woken);
    }

    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

// --------------------------------------------------
// Initialize PCNT for quadrature-style counting
//
// Simple method:
// - A is the pulse input
// - B is the direction control input
// - count both rising and falling edges of A => x2 counting
// --------------------------------------------------

static esp_err_t encoder_init_pcnt(void)
{
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

    // Count on BOTH edges of A
    // This gives x2 counting.
    ESP_RETURN_ON_ERROR(
        pcnt_channel_set_edge_action(
            g_pcnt_chan,
            PCNT_CHANNEL_EDGE_ACTION_INCREASE,   // rising edge of A
            PCNT_CHANNEL_EDGE_ACTION_INCREASE    // falling edge of A
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
            PCNT_CHANNEL_LEVEL_ACTION_INVERSE    // B low
        ),
        TAG_encoder,
        "pcnt_channel_set_level_action failed"
    );

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(g_pcnt_unit), TAG_encoder, "pcnt_unit_enable failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(g_pcnt_unit), TAG_encoder, "pcnt_unit_clear_count failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(g_pcnt_unit), TAG_encoder, "pcnt_unit_start failed");

    return ESP_OK;
}

// --------------------------------------------------
// Initialize optional Z index interrupt
// --------------------------------------------------

static esp_err_t encoder_init_z(void)
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

// --------------------------------------------------
// Public API
// --------------------------------------------------

esp_err_t encoder_init()
{

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
    g_z_notify_task = task_handle;
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
    return g_rev_count;
}