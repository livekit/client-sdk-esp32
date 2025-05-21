#include "livekit.h"
#include "livekit_core.h"
#include "livekit_rtc.h"
#include "livekit_signaling.h"

livekit_err_t livekit_create(livekit_options_t *options, livekit_handle_t *handle)
{
    if (options == NULL || handle == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    if (options->server_url == NULL || options->token == NULL) {
        ESP_LOGE(LK_TAG, "Missing server URL or token");
        return LIVEKIT_ERR_INVALID_ARG;
    }
    if (options->event_handler == NULL) {
        ESP_LOGE(LK_TAG, "Missing event handler");
        return LIVEKIT_ERR_INVALID_ARG;
    }

    livekit_room_state_t *room = (livekit_room_state_t *)calloc(1, sizeof(livekit_room_state_t));
    if (room == NULL) {
        return LIVEKIT_ERR_NO_MEM;
    }
    room->event_handler = options->event_handler;
    if (livekit_sig_build_url(options->server_url, options->token, &room->signaling_url) != 0) {
        free(room);
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_system_init();

    esp_peer_video_stream_info_t video_info;
    if (options->video_dir != LIVEKIT_VIDEO_DIR_NONE) {
        video_info.codec = ESP_PEER_VIDEO_CODEC_H264;
        video_info.width = VIDEO_WIDTH;
        video_info.height = VIDEO_HEIGHT;
        video_info.fps = VIDEO_FPS;
    }

    esp_peer_audio_stream_info_t audio_info;
    if (options->audio_dir != LIVEKIT_AUDIO_DIR_NONE) {
#ifdef WEBRTC_SUPPORT_OPUS
        audio_info.codec = ESP_PEER_AUDIO_CODEC_OPUS;
        audio_info.sample_rate = 16000;
        audio_info.channel = 2;
#else
        audio_info.codec = ESP_PEER_AUDIO_CODEC_G711A;
#endif
    }

    esp_peer_default_cfg_t peer_cfg = {
        .agent_recv_timeout = 500,
    };
    esp_webrtc_cfg_t cfg = {
        .peer_cfg = {
            .audio_info = audio_info,
            .video_info = video_info,
            .audio_dir = (esp_peer_media_dir_t)options->audio_dir,
            .video_dir = (esp_peer_media_dir_t)options->video_dir,
            .on_custom_data = livekit_rtc_data_handler,
            .ctx = room,
            .enable_data_channel = true,
            .no_auto_reconnect = true,
            .extra_cfg = &peer_cfg,
            .extra_size = sizeof(peer_cfg),
        },
        .signaling_cfg = {
            .signal_url = room->signaling_url,
            .ctx = room
        },
        .peer_impl = esp_peer_get_default_impl(),
        .signaling_impl = livekit_sig_get_impl()
    };

    // TODO: Setup audio and video if enabled

    if (esp_webrtc_open(&cfg, &room->rtc_handle) != 0) {
        ESP_LOGE(LK_TAG, "Failed to open WebRTC");
        return LIVEKIT_ERR_RTC;
    }

    *handle = room;
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_destroy(livekit_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_state_t *room = (livekit_room_state_t *)handle;
    livekit_err_t ret = LIVEKIT_ERR_NONE;
    if (esp_webrtc_close(room->rtc_handle) != 0) {
        ESP_LOGE(LK_TAG, "Failed to close WebRTC");
        ret = LIVEKIT_ERR_RTC;
    }
    free(room->signaling_url);
    free(room);
    return ret;
}

livekit_err_t livekit_connect(livekit_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_state_t *room = (livekit_room_state_t *)handle;

    // TODO: media setup
    // esp_webrtc_media_provider_t media_provider = {};
    // media_sys_get_provider(&media_provider);
    // esp_webrtc_set_media_provider(room->rtc_handle, &media_provider);

    esp_webrtc_set_event_handler(room->rtc_handle, livekit_rtc_event_handler, room);
    esp_webrtc_enable_peer_connection(room->rtc_handle, false);

    if (esp_webrtc_start(room->rtc_handle) != 0) {
        ESP_LOGE(LK_TAG, "Failed to start WebRTC");
        return LIVEKIT_ERR_RTC;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_disconnect(livekit_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_state_t *room = (livekit_room_state_t *)handle;

    // TODO: media cleanup

    if (esp_webrtc_stop(room->rtc_handle) != 0) {
        ESP_LOGE(LK_TAG, "Failed to stop WebRTC");
        return LIVEKIT_ERR_RTC;
    }
    return LIVEKIT_ERR_NONE;
}

livekit_err_t livekit_perform_rpc(livekit_perform_rpc_data_t *data, livekit_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ERR_INVALID_ARG;
    }
    livekit_room_state_t *room = (livekit_room_state_t *)handle;

    // TODO: perform RPC
    return LIVEKIT_ERR_NONE;
}