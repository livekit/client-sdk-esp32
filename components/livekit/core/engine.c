#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <inttypes.h>
#include <stdlib.h>
#include "esp_log.h"
#include "url.h"
#include "signaling.h"
#include "peer.h"
#include "engine.h"

// MARK: - Constants
static const char* TAG = "livekit_engine";

// MARK: - Event bits
#define ENGINE_EV_CONNECT_CMD             (1 << 0)
#define ENGINE_EV_CLOSE_CMD               (1 << 1)
#define ENGINE_EV_COMPONENT_STATE_CHANGED (1 << 2)
#define ENGINE_EV_JOIN_RECEIVED           (1 << 3)
#define ENGINE_EV_LEAVE_RECEIVED          (1 << 4)

// MARK: - Type definitions

typedef struct {
    engine_state_t state;
    engine_options_t options;

    signal_handle_t signal_handle;
    peer_handle_t pub_peer_handle;
    // peer_handle_t sub_peer_handle;

    connection_state_t signal_state;
    connection_state_t pub_peer_state;

    // Session state
    livekit_pb_disconnect_reason_t disconnect_reason;
    livekit_pb_leave_request_action_t leave_action;
    bool is_subscriber_primary;
    bool force_relay;

    SemaphoreHandle_t state_mutex;
    char* server_url;
    char* token;

    TaskHandle_t task_handle;
    EventGroupHandle_t event_group;
    bool is_running;
    int retry_count;
} engine_t;

// MARK: - Signal event handlers

static void on_signal_state_changed(connection_state_t state, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    eng->signal_state = state;
    xEventGroupSetBits(eng->event_group, ENGINE_EV_COMPONENT_STATE_CHANGED);
}

static void on_signal_join(livekit_pb_join_response_t *join_res, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    eng->is_subscriber_primary = join_res->subscriber_primary;
    eng->force_relay = join_res->has_client_configuration &&
                       join_res->client_configuration.force_relay == LIVEKIT_PB_CLIENT_CONFIG_SETTING_ENABLED;
    // TODO: Retain other fields
    xEventGroupSetBits(eng->event_group, ENGINE_EV_JOIN_RECEIVED);
}

static void on_signal_leave(livekit_pb_disconnect_reason_t reason, livekit_pb_leave_request_action_t action, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    eng->disconnect_reason = reason;
    eng->leave_action = action;
    xEventGroupSetBits(eng->event_group, ENGINE_EV_LEAVE_RECEIVED);
}

// MARK: - Peer event handlers

static void on_peer_ice_candidate(const char *candidate, void *ctx)
{
    ESP_LOGI(TAG, "ICE candidate");
    // TODO: Handle ICE candidate
}

static void on_peer_packet_received(livekit_pb_data_packet_t* packet, void *ctx)
{
    ESP_LOGI(TAG, "Packet received");
    // TODO: Handle packet
}

static void on_peer_pub_state_changed(connection_state_t state, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    eng->pub_peer_state = state;
    xEventGroupSetBits(eng->event_group, ENGINE_EV_COMPONENT_STATE_CHANGED);
}

static void on_peer_pub_offer(const char *sdp, void *ctx)
{
    ESP_LOGI(TAG, "Publisher peer offer");
    // TODO: Handle offer
}

// MARK: - Peer lifecycle

static bool disconnect_peer(peer_handle_t *peer)
{
    if (*peer == NULL) return false;
    if (peer_disconnect(*peer) != PEER_ERR_NONE) return false;
    if (peer_destroy(*peer) != PEER_ERR_NONE) return false;
    *peer = NULL;
    return true;
}

static bool connect_peer(engine_t *eng, peer_options_t *options, peer_handle_t *peer)
{
    disconnect_peer(peer);
    if (peer_create(peer, options) != PEER_ERR_NONE) return false;
    if (peer_connect(*peer) != PEER_ERR_NONE) return false;
    return true;
}

static bool connect_peers(engine_t *eng)
{
    peer_options_t options = {
        // Options common to both peers
        .force_relay = eng->force_relay,
        .media = &eng->options.media,
        // .server_list = eng->ice_servers,
        //.server_count = eng->ice_server_count,
        .on_ice_candidate = on_peer_ice_candidate,
        .on_packet_received = on_peer_packet_received,
        .ctx = eng
    };

    // 1. Publisher peer
    options.is_primary = !eng->is_subscriber_primary;
    options.target = LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER;
    options.on_state_changed = on_peer_pub_state_changed;
    options.on_sdp = on_peer_pub_offer;

    if (!connect_peer(eng, &options, &eng->pub_peer_handle)) {
        ESP_LOGE(TAG, "Failed to connect publisher peer");
        return false;
    }
    return true;
}

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
            continue;
        }
        ESP_LOGI(TAG, "Re-entering state %d", eng->state);
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
    if (xSemaphoreTake(eng->state_mutex, pdMS_TO_TICKS(100)) != pdPASS) {
        eng->state = ENGINE_STATE_DISCONNECTED;
        return;
    }
    if (signal_connect(eng->signal_handle, eng->server_url, eng->token) != SIGNAL_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect signal client");
        eng->state = ENGINE_STATE_DISCONNECTED;
        xSemaphoreGive(eng->state_mutex);
        return;
    }
    xSemaphoreGive(eng->state_mutex);

    // 1. Wait for signal connected
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            eng->event_group,
            ENGINE_EV_CLOSE_CMD | ENGINE_EV_COMPONENT_STATE_CHANGED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );

        if (bits & ENGINE_EV_CLOSE_CMD) {
            eng->state = ENGINE_STATE_DISCONNECTING;
            return;
        }
        if (bits & ENGINE_EV_COMPONENT_STATE_CHANGED) {
            if (eng->signal_state == CONNECTION_STATE_CONNECTED) {
                break;
            }
            if (eng->signal_state == CONNECTION_STATE_FAILED ||
                eng->signal_state == CONNECTION_STATE_DISCONNECTED) {
                // TODO: Check error code (4xx is user error and should go to disconnecting)
                eng->state = ENGINE_STATE_RECONNECTING;
                return;
            }
        }
    }

    // 2. Wait for join response
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            eng->event_group,
            ENGINE_EV_CLOSE_CMD | ENGINE_EV_COMPONENT_STATE_CHANGED | ENGINE_EV_JOIN_RECEIVED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        if (bits & ENGINE_EV_CLOSE_CMD) {
            eng->state = ENGINE_STATE_DISCONNECTING;
            return;
        }
        if (bits & ENGINE_EV_COMPONENT_STATE_CHANGED) {
            if (eng->signal_state != CONNECTION_STATE_CONNECTED) {
                eng->state = ENGINE_STATE_RECONNECTING;
                return;
            }
        }
        if (bits & ENGINE_EV_JOIN_RECEIVED) {
            break;
        }
    }

    // 3. Start peer connections, wait for primary connected
    if (!connect_peers(eng)) {
        eng->state = ENGINE_STATE_DISCONNECTING;
        return;
    }
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            eng->event_group,
            ENGINE_EV_CLOSE_CMD | ENGINE_EV_COMPONENT_STATE_CHANGED | ENGINE_EV_LEAVE_RECEIVED,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        if (bits & ENGINE_EV_CLOSE_CMD) {
            eng->state = ENGINE_STATE_DISCONNECTING;
            return;
        }
        if (bits & ENGINE_EV_COMPONENT_STATE_CHANGED) {
            if (eng->signal_state != CONNECTION_STATE_CONNECTED ||
                eng->pub_peer_state == CONNECTION_STATE_FAILED) {
                eng->state = ENGINE_STATE_RECONNECTING;
                return;
            }
            if (eng->pub_peer_state == CONNECTION_STATE_CONNECTED) {
                break;
            }
        }
        if (bits & ENGINE_EV_LEAVE_RECEIVED) {
            // TODO: Factor out this common code
            switch (eng->leave_action) {
                case LIVEKIT_PB_LEAVE_REQUEST_ACTION_RECONNECT:
                case LIVEKIT_PB_LEAVE_REQUEST_ACTION_RESUME:
                    eng->state = ENGINE_STATE_RECONNECTING;
                    break;
                default:
                    eng->state = ENGINE_STATE_DISCONNECTING;
            }
            return;
        }
    }
    eng->state = ENGINE_STATE_CONNECTED;
}

static void handle_state_connected(engine_t *eng)
{
    // TODO: Track pub/sub

    EventBits_t bits = xEventGroupWaitBits(
        eng->event_group,
        ENGINE_EV_CLOSE_CMD | ENGINE_EV_COMPONENT_STATE_CHANGED | ENGINE_EV_LEAVE_RECEIVED,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY
    );
    if (bits & ENGINE_EV_CLOSE_CMD) {
        eng->state = ENGINE_STATE_DISCONNECTING;
        return;
    }
    if (bits & ENGINE_EV_COMPONENT_STATE_CHANGED) {
        if (eng->signal_state != CONNECTION_STATE_CONNECTED) {
            eng->state = ENGINE_STATE_RECONNECTING;
        }
        return;
    }
    if (bits & ENGINE_EV_LEAVE_RECEIVED) {
        switch (eng->leave_action) {
            case LIVEKIT_PB_LEAVE_REQUEST_ACTION_RECONNECT:
            case LIVEKIT_PB_LEAVE_REQUEST_ACTION_RESUME:
                eng->state = ENGINE_STATE_RECONNECTING;
                break;
            default:
                eng->state = ENGINE_STATE_DISCONNECTING;
        }
        return;
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
    // TODO: Send leave if user initiated

    disconnect_peer(&eng->pub_peer_handle);

    if (eng->signal_state != CONNECTION_STATE_DISCONNECTED) {
        signal_close(eng->signal_handle);
        // TODO: Wait until disconnected
    }
    eng->state = ENGINE_STATE_DISCONNECTED;
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

    signal_options_t signal_options = {
        .ctx = eng,
        .on_state_changed = on_signal_state_changed,
        .on_join = on_signal_join,
        .on_leave = on_signal_leave,
        // TODO: Add other handlers
    };
    if (signal_create(&eng->signal_handle, &signal_options) != SIGNAL_ERR_NONE) {
        vEventGroupDelete(eng->event_group);
        vSemaphoreDelete(eng->state_mutex);
        free(eng);
        return ENGINE_ERR_SIGNALING;
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

    signal_destroy(eng->signal_handle);
    peer_destroy(eng->pub_peer_handle);

    xSemaphoreTake(eng->state_mutex, portMAX_DELAY);
    if (eng->server_url != NULL) {
        free(eng->server_url);
    }
    if (eng->token != NULL) {
        free(eng->token);
    }
    vSemaphoreDelete(eng->state_mutex);
    vEventGroupDelete(eng->event_group);
    // TODO: Free other resources
    free(eng);
    return ENGINE_ERR_NONE;
}

engine_err_t engine_connect(engine_handle_t handle, const char* server_url, const char* token)
{
    if (handle == NULL || server_url == NULL || token == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;

    if (xSemaphoreTake(eng->state_mutex, pdMS_TO_TICKS(100)) != pdPASS) {
        return ENGINE_ERR_OTHER;
    }
    if (eng->server_url != NULL) {
        free(eng->server_url);
    }
    if (eng->token != NULL) {
        free(eng->token);
    }
    eng->server_url = strdup(server_url);
    eng->token = strdup(token);
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
    // engine_t *eng = (engine_t *)handle;
    // TODO: Send data packet

    return ENGINE_ERR_NONE;
}