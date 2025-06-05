#include <stdlib.h>
#include "esp_log.h"
#include "esp_peer.h"
#include "esp_peer_default.h"
#include "esp_peer_signaling.h"
#include "esp_webrtc_defaults.h"
#include "media_lib_os.h"
#include "esp_codec_dev.h"

#include "livekit_peer.h"

static const char *TAG = "livekit_peer";

#define PC_EXIT_BIT      (1 << 0)
#define PC_PAUSED_BIT    (1 << 1)
#define PC_RESUME_BIT    (1 << 2)
#define PC_SEND_QUIT_BIT (1 << 3)

#define SAFE_FREE(ptr) if (ptr != NULL) {   \
    free(ptr);                      \
    ptr = NULL;                     \
}

typedef struct {
    livekit_peer_kind_t kind;
    esp_peer_role_t ice_role;
    esp_peer_handle_t connection;
    esp_peer_state_t state;

    esp_peer_ice_server_cfg_t *ice_servers;
    int ice_server_count;

    bool running;
    bool pause;
    media_lib_event_grp_handle_t wait_event;

    esp_codec_dev_handle_t        play_handle;
} livekit_peer_t;

static void peer_task(void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer task started");
    while (peer->running) {
        if (peer->pause) {
            media_lib_event_group_set_bits(peer->wait_event, PC_PAUSED_BIT);
            media_lib_event_group_wait_bits(peer->wait_event, PC_RESUME_BIT, MEDIA_LIB_MAX_LOCK_TIME);
            media_lib_event_group_clr_bits(peer->wait_event, PC_RESUME_BIT);
            continue;
        }
        esp_peer_main_loop(peer->connection);
        media_lib_thread_sleep(10);
    }
    media_lib_event_group_set_bits(peer->wait_event, PC_EXIT_BIT);
    media_lib_thread_destroy(NULL);
}

static void free_ice_servers(livekit_peer_t *peer)
{
    if (peer->ice_servers == NULL) return;
    esp_peer_ice_server_cfg_t *ice_servers = peer->ice_servers;
    for (int i = 0; i < peer->ice_server_count; i++) {
        SAFE_FREE(ice_servers[i].stun_url);
        SAFE_FREE(ice_servers[i].user);
        SAFE_FREE(ice_servers[i].psw);
    }
    SAFE_FREE(peer->ice_servers);
    peer->ice_server_count = 0;
}

static int on_state(esp_peer_state_t state, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer state changed to %d", state);
    return 0;
}

static int on_msg(esp_peer_msg_t *info, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer msg received: type=%d", info->type);
    return 0;
}

static int on_video_info(esp_peer_video_stream_info_t *info, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer video info received: %d", info->codec);
    return 0;
}

static int on_audio_info(esp_peer_audio_stream_info_t *info, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer audio info received: %d", info->codec);
    return 0;
}

static int on_video_data(esp_peer_video_frame_t *info, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer video data received: size=%d", info->size);
    return 0;
}

static int on_audio_data(esp_peer_audio_frame_t *info, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer audio data received: size=%d", info->size);
    return 0;
}

static int on_channel_open(esp_peer_data_channel_info_t *ch, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer channel open: label=%s, stream_id=%d", ch->label, ch->stream_id);
    return 0;
}

static int on_channel_close(esp_peer_data_channel_info_t *ch, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer channel close: label=%s, stream_id=%d", ch->label, ch->stream_id);
    return 0;
}

static int on_data(esp_peer_data_frame_t *frame, void *ctx)
{
    livekit_peer_t *peer = (livekit_peer_t *)ctx;
    ESP_LOGI(TAG, "Peer data received: size=%d", frame->size);
    return 0;
}

livekit_peer_err_t livekit_peer_create(livekit_peer_kind_t kind, livekit_peer_handle_t *handle)
{
    if (handle == NULL || kind == LIVEKIT_PEER_KIND_NONE) {
        return LIVEKIT_PEER_ERR_INVALID_ARG;
    }
    livekit_peer_t *peer = (livekit_peer_t *)calloc(1, sizeof(livekit_peer_t));
    if (peer == NULL) {
        return LIVEKIT_PEER_ERR_NO_MEM;
    }
    peer->kind = kind;
    peer->ice_role = kind == LIVEKIT_PEER_KIND_SUBSCRIBER ?
            ESP_PEER_ROLE_CONTROLLED : ESP_PEER_ROLE_CONTROLLING;

    *handle = (livekit_peer_handle_t)peer;
    return LIVEKIT_PEER_ERR_NONE;
}

livekit_peer_err_t livekit_peer_destroy(livekit_peer_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_PEER_ERR_INVALID_ARG;
    }
    livekit_peer_t *peer = (livekit_peer_t *)handle;
    esp_peer_close(peer->connection);
    free_ice_servers(peer);
    free(peer);
    return LIVEKIT_PEER_ERR_NONE;
}

livekit_peer_err_t livekit_peer_connect(livekit_peer_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_PEER_ERR_INVALID_ARG;
    }
    livekit_peer_t *peer = (livekit_peer_t *)handle;

    if (peer->ice_servers == NULL || peer->ice_server_count == 0) {
        ESP_LOGE(TAG, "No ICE servers configured");
        return LIVEKIT_PEER_ERR_INVALID_STATE;
    }
    if (peer->connection != NULL) {
        ESP_LOGI(TAG, "Already connected, ignoring");
        return LIVEKIT_PEER_ERR_NONE;
    }
    // Configuration for the default peer implementation.
    esp_peer_default_cfg_t default_peer_cfg = {
        .agent_recv_timeout = 10000,
        .data_ch_cfg = {
            .cache_timeout = 5000,
            .send_cache_size = 100 * 1024,
            .recv_cache_size = 100 * 1024
        }
    };
    esp_peer_cfg_t peer_cfg = {
        .server_lists = peer->ice_servers,
        .server_num = peer->ice_server_count,
        .ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_RELAY, // Only relay candidates
        .audio_dir = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ?
            ESP_PEER_MEDIA_DIR_RECV_ONLY : ESP_PEER_MEDIA_DIR_SEND_ONLY,
        .video_dir = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ?
            ESP_PEER_MEDIA_DIR_RECV_ONLY : ESP_PEER_MEDIA_DIR_SEND_ONLY,
        .enable_data_channel = peer->kind != LIVEKIT_PEER_KIND_SUBSCRIBER,
        .manual_ch_create = true,
        .no_auto_reconnect = false,
        .extra_cfg = &default_peer_cfg,
        .extra_size = sizeof(default_peer_cfg),
        .on_state = on_state,
        .on_msg = on_msg,
        .on_video_info = on_video_info,
        .on_audio_info = on_audio_info,
        .on_video_data = on_video_data,
        .on_audio_data = on_audio_data,
        .on_channel_open = on_channel_open,
        .on_channel_close = on_channel_close,
        .on_data = on_data,
        .role = peer->ice_role,
        .ctx = peer
    };

    int ret = esp_peer_open(&peer_cfg, esp_peer_get_default_impl(), &peer->connection);
    if (ret != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to open peer connection: %d", ret);
        return LIVEKIT_PEER_ERR_RTC;
    }

    media_lib_event_group_create(&peer->wait_event);
    if (peer->wait_event == NULL) {
        return ESP_PEER_ERR_NO_MEM;
    }

    peer->running = true;
    media_lib_thread_handle_t thread;
    const char* thread_name = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ? "lk_sub_task" : "lk_pub_task";
    if (media_lib_thread_create_from_scheduler(&thread, thread_name, peer_task, peer) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create peer task");
        return LIVEKIT_PEER_ERR_RTC;
    }
    // TODO: Media configuration & capture setup
    return LIVEKIT_PEER_ERR_NONE;
}

livekit_peer_err_t livekit_peer_disconnect(livekit_peer_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_PEER_ERR_INVALID_ARG;
    }
    livekit_peer_t *peer = (livekit_peer_t *)handle;

    if (peer->connection == NULL) {
        esp_peer_disconnect(peer->connection);
        bool still_running = peer->running;
        if (peer->pause) {
            peer->pause = false;
            media_lib_event_group_set_bits(peer->wait_event, PC_RESUME_BIT);
        }
        peer->running = false;
        if (still_running) {
            media_lib_event_group_wait_bits(peer->wait_event, PC_EXIT_BIT, MEDIA_LIB_MAX_LOCK_TIME);
            media_lib_event_group_clr_bits(peer->wait_event, PC_EXIT_BIT);
        }
        esp_peer_close(peer->connection);
        peer->connection = NULL;
    }
    if (peer->wait_event) {
        media_lib_event_group_destroy(peer->wait_event);
        peer->wait_event = NULL;
    }
    return LIVEKIT_PEER_ERR_NONE;
}

livekit_peer_err_t livekit_peer_set_ice_servers(livekit_ice_server_t *servers, int count, livekit_peer_handle_t handle)
{
    if (handle == NULL || servers == NULL || count <= 0) {
        return LIVEKIT_PEER_ERR_INVALID_ARG;
    }
    livekit_peer_t *peer = (livekit_peer_t *)handle;

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
            ESP_LOGI(TAG, "Adding ICE server: %s", servers[i].urls[j]);
            cfgs[cfg_idx].stun_url = strdup(servers[i].urls[j]);
            if (servers[i].username != NULL) {
                cfgs[cfg_idx].user = strdup(servers[i].username);
            }
            if (servers[i].credential != NULL) {
                cfgs[cfg_idx].psw = strdup(servers[i].credential);
            }
            cfg_idx++;
        }
    }

    if (peer->connection != NULL) {
        // Update if already connected
        esp_peer_update_ice_info(peer->connection, peer->ice_role, cfgs, cfg_count);
    }
    free_ice_servers(peer);
    peer->ice_servers = cfgs;
    peer->ice_server_count = cfg_count;

    return LIVEKIT_PEER_ERR_NONE;
}