#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <inttypes.h>
#include <stdlib.h>
#include "esp_log.h"
#include "url.h"
#include "engine.h"

// MARK: - Constants
static const char* TAG = "livekit_engine";

// MARK: - Event bits
#define ENGINE_EV_CONNECT_CMD             (1 << 0)
#define ENGINE_EV_CLOSE_CMD               (1 << 1)
#define ENGINE_EV_COMPONENT_STATE_CHANGED (1 << 2)

// MARK: - Type definitions

typedef struct {
    engine_state_t state;
    engine_options_t options;

    // signal_handle_t signal_handle;
    // peer_handle_t pub_peer_handle;
    // peer_handle_t sub_peer_handle;

    SemaphoreHandle_t state_mutex;
    char* signal_url;

    TaskHandle_t task_handle;
    EventGroupHandle_t event_group;
    bool is_running;
    int retry_count;
} engine_t;

// MARK: - State machine

static void handle_state_disconnected(engine_t *eng);
static void handle_state_connecting(engine_t *eng);
static void handle_state_connected(engine_t *eng);
static void handle_state_reconnecting(engine_t *eng);
static void handle_state_disconnecting(engine_t *eng);

static void engine_task(void *arg)
{
    engine_t *eng = (engine_t *)arg;
    while (eng->is_running) {
        engine_state_t state = eng->state;
        switch (state) {
            case ENGINE_STATE_DISCONNECTED:  handle_state_disconnected(eng);  break;
            case ENGINE_STATE_CONNECTING:    handle_state_connecting(eng);    break;
            case ENGINE_STATE_CONNECTED:     handle_state_connected(eng);     break;
            case ENGINE_STATE_RECONNECTING:  handle_state_reconnecting(eng);  break;
            case ENGINE_STATE_DISCONNECTING: handle_state_disconnecting(eng); break;
            default: break;
        }
        if (eng->state != state) {
            ESP_LOGI(TAG, "State changed: %d -> %d", state, eng->state);
            // TODO: Dispatch change event
        }
    }
    vTaskDelete(NULL);
}

static void handle_state_disconnected(engine_t *eng)
{
    EventBits_t bits = xEventGroupWaitBits(
        eng->event_group,
        ENGINE_EV_CONNECT_CMD | ENGINE_EV_CLOSE_CMD,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY
    );
    if (bits & ENGINE_EV_CONNECT_CMD) {
        eng->state = ENGINE_STATE_CONNECTING;
    }
}

static void handle_state_connecting(engine_t *eng)
{
    // TODO: Setup connections
    // - Wait for required connections to be established
    // - Handle new connection or close

    eng->state = ENGINE_STATE_CONNECTED;
}

static void handle_state_connected(engine_t *eng)
{
    EventBits_t bits = xEventGroupWaitBits(
        eng->event_group,
        ENGINE_EV_CONNECT_CMD | ENGINE_EV_CLOSE_CMD | ENGINE_EV_COMPONENT_STATE_CHANGED,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY
    );
    if (bits & ENGINE_EV_CLOSE_CMD) {
        eng->state = ENGINE_STATE_DISCONNECTING;
        return;
    }
    if (bits & ENGINE_EV_CONNECT_CMD) {
        // TODO: Support new connection while already connected
        return;
    }
    if (bits & ENGINE_EV_COMPONENT_STATE_CHANGED) {
        // TODO: Ensure components are still properly connected
    }
}

static void handle_state_reconnecting(engine_t *eng)
{
    if (eng->retry_count >= CONFIG_LK_MAX_RETRIES) {
        ESP_LOGW(TAG, "Max retries reached");
        eng->state = ENGINE_STATE_DISCONNECTED;
        return;
    }

     // TODO: Exponential backoff
    uint32_t backoff_ms = 1000;

    ESP_LOGI(TAG, "Attempting reconnect %d/%d in %" PRIu32 "ms",
        eng->retry_count + 1, CONFIG_LK_MAX_RETRIES, backoff_ms);

    vTaskDelay(pdMS_TO_TICKS(backoff_ms));
    eng->retry_count++;

    // TODO: Try connection, transition to connected if successful
    // - Handle full vs partial reconnect
}

static void handle_state_disconnecting(engine_t *eng)
{
    // TODO: Graceful shutdown
    // - Send leave, wait for ack
    // - Disconnect components, wait for close with timeout
}

// MARK: - Public API

engine_err_t engine_create(engine_handle_t *handle, engine_options_t *options)
{
    engine_t *eng = (engine_t *)calloc(1, sizeof(engine_t));
    if (eng == NULL) {
        return ENGINE_ERR_NO_MEM;
    }
    eng->state_mutex = xSemaphoreCreateMutex();
    if (eng->state_mutex == NULL) {
        free(eng);
        return ENGINE_ERR_NO_MEM;
    }
    eng->event_group = xEventGroupCreate();
    if (eng->event_group == NULL) {
        vSemaphoreDelete(eng->state_mutex);
        free(eng);
        return ENGINE_ERR_NO_MEM;
    }

    eng->options = *options;
    eng->state = ENGINE_STATE_DISCONNECTED;
    eng->is_running = true;

    if (xTaskCreate(engine_task, "engine_task", 4096, eng, 5, &eng->task_handle) != pdPASS) {
        vEventGroupDelete(eng->event_group);
        vSemaphoreDelete(eng->state_mutex);
        free(eng);
        return ENGINE_ERR_NO_MEM;
    }

    *handle = (engine_handle_t)eng;
    return ENGINE_ERR_NONE;
}

engine_err_t engine_destroy(engine_handle_t handle)
{
    if (handle == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;

    eng->is_running = false;
    if (eng->task_handle != NULL) {
        // TODO: Wait for disconnected state or timeout
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    xSemaphoreTake(eng->state_mutex, portMAX_DELAY);
    if (eng->signal_url != NULL) {
        free(eng->signal_url);
    }
    vSemaphoreDelete(eng->state_mutex);

    // TODO: Free other resources
    free(eng);
    return ENGINE_ERR_NONE;
}

engine_err_t engine_connect(engine_handle_t handle, const char* server_url, const char* token)
{
    if (handle == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;

    char* new_signal_url = NULL;
    if (!url_build(server_url, token, &new_signal_url)) {
        return ENGINE_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(eng->state_mutex, pdMS_TO_TICKS(100)) != pdPASS) {
        free(new_signal_url);
        return ENGINE_ERR_OTHER;
    }
    if (eng->signal_url != NULL) {
        free(eng->signal_url);
    }
    eng->signal_url = new_signal_url;
    xSemaphoreGive(eng->state_mutex);

    xEventGroupSetBits(eng->event_group, ENGINE_EV_CONNECT_CMD);
    return ENGINE_ERR_NONE;
}

engine_err_t engine_close(engine_handle_t handle)
{
    if (handle == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;

    xEventGroupSetBits(eng->event_group, ENGINE_EV_CLOSE_CMD);
    return ENGINE_ERR_NONE;
}

engine_err_t engine_send_data_packet(engine_handle_t handle, const livekit_pb_data_packet_t* packet, livekit_pb_data_packet_kind_t kind)
{
    if (handle == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;

    // TODO: Send data packet

    return ENGINE_ERR_NONE;
}