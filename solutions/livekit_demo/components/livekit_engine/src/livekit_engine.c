#include "esp_log.h"
#include "webrtc_utils_time.h"
#include "livekit_protocol.h"
#include "livekit_engine.h"
#include "livekit_signaling.h"
#include "livekit_peer.h"

static const char *TAG = "livekit_engine";

#define SAFE_FREE(ptr) if (ptr != NULL) {   \
    free(ptr);                      \
    ptr = NULL;                     \
}

typedef struct {
    livekit_eng_options_t options;
    livekit_sig_handle_t  sig;

    livekit_peer_handle_t pub_peer;
    livekit_peer_handle_t sub_peer;

    esp_peer_ice_server_cfg_t *ice_servers;
    int ice_server_count;
} livekit_eng_t;

/// @brief Performs one-time system initialization.
static void sys_init(void)
{
    static bool is_initialized = false;
    if (is_initialized) {
        ESP_LOGI(TAG, "System already initialized");
        return;
    }
    is_initialized = webrtc_utils_time_sync_init() == ESP_OK;
    if (!is_initialized) {
        ESP_LOGE(TAG, "System initialization failed");
        return;
    }
}

static void free_ice_servers(livekit_eng_t *eng)
{
    if (eng == NULL || eng->ice_servers == NULL) {
        return;
    }
    esp_peer_ice_server_cfg_t *ice_servers = eng->ice_servers;
    for (int i = 0; i < eng->ice_server_count; i++) {
        SAFE_FREE(ice_servers[i].stun_url);
        SAFE_FREE(ice_servers[i].user);
        SAFE_FREE(ice_servers[i].psw);
    }
    SAFE_FREE(eng->ice_servers);
    eng->ice_server_count = 0;
}

static livekit_eng_err_t set_ice_servers(livekit_eng_t* eng, livekit_pb_ice_server_t *servers, int count)
{
    if (eng == NULL || servers == NULL || count <= 0) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    // A single livekit_ice_server_t can contain multiple URLs, which
    // will map to multiple esp_peer_ice_server_cfg_t entries.
    size_t cfg_count = 0;
    for (int i = 0; i < count; i++) {
        if (servers[i].urls_count <= 0) {
            return LIVEKIT_PEER_ERR_INVALID_ARG;
        }
        for (int j = 0; j < servers[i].urls_count; j++) {
            if (servers[i].urls[j] == NULL) {
                return LIVEKIT_PEER_ERR_INVALID_ARG;
            }
            cfg_count++;
        }
    }

    esp_peer_ice_server_cfg_t *cfgs = calloc(cfg_count, sizeof(esp_peer_ice_server_cfg_t));
    if (cfgs == NULL) {
        return LIVEKIT_PEER_ERR_NO_MEM;
    }

    int cfg_idx = 0;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < servers[i].urls_count; j++) {
            bool has_auth = false;
            cfgs[cfg_idx].stun_url = strdup(servers[i].urls[j]);
            if (servers[i].username != NULL) {
                cfgs[cfg_idx].user = strdup(servers[i].username);
                has_auth = true;
            }
            if (servers[i].credential != NULL) {
                cfgs[cfg_idx].psw = strdup(servers[i].credential);
                has_auth = true;
            }
            ESP_LOGI(TAG, "Adding ICE server: has_auth=%d, url=%s", has_auth, servers[i].urls[j]);
            cfg_idx++;
        }
    }

    // Set ICE servers for both peers (owned by engine)
    livekit_peer_set_ice_servers(eng->pub_peer, cfgs, cfg_count);
    livekit_peer_set_ice_servers(eng->sub_peer, cfgs, cfg_count);

    free_ice_servers(eng);
    eng->ice_servers = cfgs;
    eng->ice_server_count = cfg_count;

    return LIVEKIT_PEER_ERR_NONE;
}

static void on_peer_pub_offer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Pub peer generated offer: %s", sdp);
    livekit_sig_send_offer(eng->sig, sdp);
}

static void on_peer_sub_answer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Sub peer generated answer: %s", sdp);
    livekit_sig_send_answer(eng->sig, sdp);
}

static void on_peer_ice_candidate(const char *candidate, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Peer generated ice candidate: %s", candidate);
}

static void on_sig_connect(void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Signaling connected");
    // TODO: Implement
}

static void on_sig_disconnect(void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Signaling disconnected");
    // TODO: Implement
}

static void on_sig_error(void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Signaling error");
    // TODO: Implement
}

static void on_sig_join(livekit_pb_join_response_t *join_res, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    set_ice_servers(eng, join_res->ice_servers, join_res->ice_servers_count);

    if (join_res->subscriber_primary) {
        ESP_LOGE(TAG, "Subscriber primary is not supported yet");
        return;
    }
    livekit_peer_connect_options_t connect_options = {
        .force_relay = join_res->has_client_configuration &&
                       join_res->client_configuration.force_relay == LIVEKIT_PB_CLIENT_CONFIG_SETTING_ENABLED,
        .media = &eng->options.media,
    };
    livekit_peer_connect(eng->pub_peer, connect_options);
    // livekit_peer_connect(connection_options, eng->sub_peer);
}

static void on_sig_answer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Received answer: \n%s", sdp);
    livekit_peer_handle_sdp(eng->pub_peer, sdp);
}

static void on_sig_offer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Received offer: \n%s", sdp);
    livekit_peer_handle_sdp(eng->sub_peer, sdp);
}

static void on_sig_trickle(const char *ice_candidate, livekit_pb_signal_target_t target, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    livekit_peer_handle_t target_peer = target == LIVEKIT_PB_SIGNAL_TARGET_SUBSCRIBER ?
        eng->sub_peer : eng->pub_peer;
    livekit_peer_handle_ice_candidate(target_peer, ice_candidate);
}

livekit_eng_err_t livekit_eng_create(livekit_eng_handle_t *handle, livekit_eng_options_t *options)
{
    if (handle == NULL || options == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)calloc(1, sizeof(livekit_eng_t));
    if (eng == NULL) {
        return LIVEKIT_ENG_ERR_NO_MEM;
    }
    eng->options = *options;
    livekit_sig_options_t sig_options = {
        .ctx = eng,
        .on_connect = on_sig_connect,
        .on_disconnect = on_sig_disconnect,
        .on_error = on_sig_error,
        .on_join = on_sig_join,
        .on_answer = on_sig_answer,
        .on_offer = on_sig_offer,
        .on_trickle = on_sig_trickle,
    };
    int ret = LIVEKIT_ENG_ERR_OTHER;
    do {
        if (livekit_sig_create(&eng->sig, &sig_options) != LIVEKIT_SIG_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create signaling client");
            break;
        }

        livekit_peer_options_t pub_options = {
            .target = LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER,
            .on_sdp = on_peer_pub_offer,
            .on_ice_candidate = on_peer_ice_candidate,
            .ctx = eng
        };
        if (livekit_peer_create(&eng->pub_peer, pub_options) != LIVEKIT_PEER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create publisher peer");
            break;
        }

        livekit_peer_options_t sub_options = {
            .target = LIVEKIT_PB_SIGNAL_TARGET_SUBSCRIBER,
            .on_sdp = on_peer_sub_answer,
            .on_ice_candidate = on_peer_ice_candidate,
            .ctx = eng
        };
        if (livekit_peer_create(&eng->sub_peer, sub_options) != LIVEKIT_PEER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to create subscriber peer");
            break;
        }
        *handle = eng;
        return LIVEKIT_ENG_ERR_NONE;
    } while (0);

    if (eng->sub_peer != NULL) {
        livekit_peer_destroy(eng->sub_peer);
    }
    if (eng->pub_peer != NULL) {
        livekit_peer_destroy(eng->pub_peer);
    }
    if (eng->sig != NULL) {
        livekit_sig_destroy(eng->sig);
    }
    free(eng);
    return ret;
}

livekit_eng_err_t livekit_eng_destroy(livekit_eng_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    if (eng->sub_peer != NULL) {
        livekit_peer_destroy(eng->sub_peer);
        eng->sub_peer = NULL;
    }
    if (eng->pub_peer != NULL) {
        livekit_peer_destroy(eng->pub_peer);
        eng->pub_peer = NULL;
    }
    if (eng->sig != NULL) {
        livekit_sig_destroy(eng->sig);
        eng->sig = NULL;
    }
    free_ice_servers(eng);
    free(eng);
    return LIVEKIT_ENG_ERR_NONE;
}

livekit_eng_err_t livekit_eng_connect(livekit_eng_handle_t handle, const char* server_url, const char* token)
{
    if (handle == NULL || server_url == NULL || token == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    sys_init();

    if (livekit_sig_connect(eng->sig, server_url, token) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect signaling client");
        return LIVEKIT_ENG_ERR_SIGNALING;
    }
    return LIVEKIT_ENG_ERR_NONE;
}

livekit_eng_err_t livekit_eng_close(livekit_eng_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    if (eng->sub_peer != NULL) {
        livekit_peer_disconnect(eng->sub_peer);
    }
    if (eng->pub_peer != NULL) {
        livekit_peer_disconnect(eng->pub_peer);
    }
    if (eng->sig != NULL) {
        // TODO: Ensure the WebSocket stays open long enough for the leave message to be sent
        livekit_sig_send_leave(eng->sig);
        livekit_sig_close(eng->sig);
    }
    return LIVEKIT_ENG_ERR_NONE;
}

livekit_eng_err_t livekit_eng_send_data_packet(livekit_eng_handle_t handle, livekit_pb_data_packet_t* packet, livekit_pb_data_packet_kind_t kind)
{
    if (handle == NULL || packet == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    // TODO: Once subscriber primary is supported, dynamically switch between pub and sub peers
    if (livekit_peer_send_data_packet(eng->pub_peer, packet, kind) != LIVEKIT_PEER_ERR_NONE) {
        return LIVEKIT_ENG_ERR_RTC;
    }
    return LIVEKIT_ENG_ERR_NONE;
}