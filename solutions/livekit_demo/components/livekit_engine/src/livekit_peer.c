#include <stdlib.h>
#include "esp_log.h"
#include "esp_peer.h"
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

typedef struct {
    livekit_peer_kind_t kind;
    esp_peer_handle_t connection;
    esp_peer_state_t state;

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
    livekit_peer_t *peer = (livekit_peer_t *)malloc(sizeof(livekit_peer_t));
    if (peer == NULL) {
        return LIVEKIT_PEER_ERR_NO_MEM;
    }
    peer->kind = kind;
    peer->connection = NULL;
    peer->running = false;
    peer->wait_event = NULL;
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
    free(peer);
    return LIVEKIT_PEER_ERR_NONE;
}

livekit_peer_err_t livekit_peer_connect(esp_peer_ice_server_cfg_t *server_info, int server_num, livekit_peer_handle_t handle)
{
    if (handle == NULL || server_info == NULL || server_num <= 0) {
        return LIVEKIT_PEER_ERR_INVALID_ARG;
    }
    livekit_peer_t *peer = (livekit_peer_t *)handle;

    esp_peer_role_t ice_role = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ?
            ESP_PEER_ROLE_CONTROLLED : ESP_PEER_ROLE_CONTROLLING;

    if (peer->connection != NULL) {
        // Already connected, just update ICE info
        if (esp_peer_update_ice_info(peer->connection, ice_role, server_info, server_num) != ESP_PEER_ERR_NONE) {
            ESP_LOGE(TAG, "Failed to update ICE info");
            return LIVEKIT_PEER_ERR_RTC;
        }
        return LIVEKIT_PEER_ERR_NONE;
    }

    esp_peer_cfg_t peer_cfg = {
        .server_lists = server_info,
        .server_num = server_num,
        .ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_RELAY, // Only relay candidates
        .audio_dir = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ?
            ESP_PEER_MEDIA_DIR_RECV_ONLY : ESP_PEER_MEDIA_DIR_SEND_ONLY,
        .video_dir = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ?
            ESP_PEER_MEDIA_DIR_RECV_ONLY : ESP_PEER_MEDIA_DIR_SEND_ONLY,
        .enable_data_channel = peer->kind != LIVEKIT_PEER_KIND_SUBSCRIBER,
        .manual_ch_create = true,
        .no_auto_reconnect = false,
        .extra_cfg = NULL,
        .extra_size = 0,
        .on_state = on_state,
        .on_msg = on_msg,
        .on_video_info = on_video_info,
        .on_audio_info = on_audio_info,
        .on_video_data = on_video_data,
        .on_audio_data = on_audio_data,
        .on_channel_open = on_channel_open,
        .on_channel_close = on_channel_close,
        .on_data = on_data,
        .role = ice_role,
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
    const char* thread_name = peer->kind == LIVEKIT_PEER_KIND_SUBSCRIBER ? "peer_sub_task" : "peer_pub_task";
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