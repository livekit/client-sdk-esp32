#include "esp_log.h"
#include "webrtc_utils_time.h"
#include "media_lib_os.h"
#include "esp_codec_dev.h"

#include "livekit_protocol.h"
#include "livekit_engine.h"
#include "livekit_signaling.h"
#include "livekit_peer.h"

static const char *TAG = "livekit_engine";

#define VIDEO_TRACK_CID "video0"
#define AUDIO_TRACK_CID "audio0"

#define VIDEO_TRACK_NAME "Video"
#define AUDIO_TRACK_NAME "Audio"
#define FRAME_INTERVAL_MS 20

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

    esp_capture_path_handle_t capturer_path;
    bool is_media_streaming;

    esp_codec_dev_handle_t        renderer_handle;
    esp_peer_audio_stream_info_t  sub_audio_info;
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

static esp_capture_codec_type_t capture_audio_codec_type(esp_peer_audio_codec_t peer_codec)
{
    switch (peer_codec) {
        case ESP_PEER_AUDIO_CODEC_G711A: return ESP_CAPTURE_CODEC_TYPE_G711A;
        case ESP_PEER_AUDIO_CODEC_G711U: return ESP_CAPTURE_CODEC_TYPE_G711U;
        case ESP_PEER_AUDIO_CODEC_OPUS:  return ESP_CAPTURE_CODEC_TYPE_OPUS;
        default:                         return ESP_CAPTURE_CODEC_TYPE_NONE;
    }
}

static esp_capture_codec_type_t capture_video_codec_type(esp_peer_video_codec_t peer_codec)
{
    switch (peer_codec) {
        case ESP_PEER_VIDEO_CODEC_H264:  return ESP_CAPTURE_CODEC_TYPE_H264;
        case ESP_PEER_VIDEO_CODEC_MJPEG: return ESP_CAPTURE_CODEC_TYPE_MJPEG;
        default:                         return ESP_CAPTURE_CODEC_TYPE_NONE;
    }
}

static av_render_audio_codec_t get_dec_codec(esp_peer_audio_codec_t codec)
{
    switch (codec) {
        case ESP_PEER_AUDIO_CODEC_G711A: return AV_RENDER_AUDIO_CODEC_G711A;
        case ESP_PEER_AUDIO_CODEC_G711U: return AV_RENDER_AUDIO_CODEC_G711U;
        case ESP_PEER_AUDIO_CODEC_OPUS:  return AV_RENDER_AUDIO_CODEC_OPUS;
        default:                         return AV_RENDER_AUDIO_CODEC_NONE;
    }
}

static void convert_dec_aud_info(esp_peer_audio_stream_info_t *info, av_render_audio_info_t *dec_info)
{
    dec_info->codec = get_dec_codec(info->codec);
    if (info->codec == ESP_PEER_AUDIO_CODEC_G711A || info->codec == ESP_PEER_AUDIO_CODEC_G711U) {
        dec_info->sample_rate = 8000;
        dec_info->channel = 1;
    } else {
        dec_info->sample_rate = info->sample_rate;
        dec_info->channel = info->channel;
    }
    dec_info->bits_per_sample = 16;
}

static void _media_stream_send_audio(livekit_eng_t *eng)
{
    esp_capture_stream_frame_t audio_frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO,
    };
    while (esp_capture_acquire_path_frame(eng->capturer_path, &audio_frame, true) == ESP_CAPTURE_ERR_OK) {
        esp_peer_audio_frame_t audio_send_frame = {
            .pts = audio_frame.pts,
            .data = audio_frame.data,
            .size = audio_frame.size,
        };
        livekit_peer_send_audio(eng->pub_peer, &audio_send_frame);
        esp_capture_release_path_frame(eng->capturer_path, &audio_frame);
    }
}

static void _media_stream_send_video(livekit_eng_t *eng)
{
    esp_capture_stream_frame_t video_frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
    };
    if (esp_capture_acquire_path_frame(eng->capturer_path, &video_frame, true) == ESP_CAPTURE_ERR_OK) {
        esp_peer_video_frame_t video_send_frame = {
            .pts = video_frame.pts,
            .data = video_frame.data,
            .size = video_frame.size,
        };
        livekit_peer_send_video(eng->pub_peer, &video_send_frame);
        esp_capture_release_path_frame(eng->capturer_path, &video_frame);
    }
}

static void media_stream_task(void *arg)
{
    livekit_eng_t *eng = (livekit_eng_t *)arg;
    ESP_LOGI(TAG, "Media stream task started");
    while (eng->is_media_streaming) {
        if (eng->options.media.audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE) {
            _media_stream_send_audio(eng);
        }
        if (eng->options.media.video_info.codec != ESP_PEER_VIDEO_CODEC_NONE) {
            _media_stream_send_video(eng);
        }
        media_lib_thread_sleep(FRAME_INTERVAL_MS);
    }
    ESP_LOGI(TAG, "Media stream task ended");
    media_lib_thread_destroy(NULL);
}

static livekit_eng_err_t media_stream_begin(livekit_eng_t *eng)
{
    if (esp_capture_start(eng->options.media.capturer) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start capture");
        return LIVEKIT_ENG_ERR_MEDIA;
    }
    media_lib_thread_handle_t handle = NULL;
    eng->is_media_streaming = true;
    if (media_lib_thread_create_from_scheduler(&handle, "lk_stream", media_stream_task, eng) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create media stream thread");
        eng->is_media_streaming = false;
        return LIVEKIT_ENG_ERR_MEDIA;
    }
    ESP_LOGI(TAG, "Media stream started");
    return LIVEKIT_ENG_ERR_NONE;
}

static livekit_eng_err_t media_stream_end(livekit_eng_t *eng)
{
    if (!eng->is_media_streaming) {
        return LIVEKIT_ENG_ERR_NONE;
    }
    eng->is_media_streaming = false;
    esp_capture_stop(eng->options.media.capturer);
    ESP_LOGI(TAG, "Media stream ended");
    return LIVEKIT_ENG_ERR_NONE;
}

static livekit_eng_err_t send_add_audio_track(livekit_eng_t *eng)
{
    bool is_stereo = eng->options.media.audio_info.channel == 2;
    livekit_pb_add_track_request_t req = {
        .cid = AUDIO_TRACK_CID,
        .name = AUDIO_TRACK_NAME,
        .type = LIVEKIT_PB_TRACK_TYPE_AUDIO,
        .source = LIVEKIT_PB_TRACK_SOURCE_MICROPHONE,
        .muted = false,
        .audio_features_count = is_stereo ? 1 : 0,
        .audio_features = { LIVEKIT_PB_AUDIO_TRACK_FEATURE_TF_STEREO },
        .layers_count = 0
    };

    if (livekit_sig_send_add_track(eng->sig, &req) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to publish audio track");
        return LIVEKIT_ENG_ERR_SIGNALING;
    }
    return LIVEKIT_ENG_ERR_NONE;
}

static livekit_eng_err_t send_add_video_track(livekit_eng_t *eng)
{
    livekit_pb_video_layer_t video_layer = {
        .quality = LIVEKIT_PB_VIDEO_QUALITY_HIGH,
        .width = eng->options.media.video_info.width,
        .height = eng->options.media.video_info.height
    };
    livekit_pb_add_track_request_t req = {
        .cid = VIDEO_TRACK_CID,
        .name = VIDEO_TRACK_NAME,
        .type = LIVEKIT_PB_TRACK_TYPE_VIDEO,
        .source = LIVEKIT_PB_TRACK_SOURCE_CAMERA,
        .muted = false,
        .layers_count = 1,
        .layers = { video_layer },
        .audio_features_count = 0
    };

    if (livekit_sig_send_add_track(eng->sig, &req) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to publish video track");
        return LIVEKIT_ENG_ERR_SIGNALING;
    }
    return LIVEKIT_ENG_ERR_NONE;
}

/// @brief Begins media streaming and sends add track requests.
static livekit_eng_err_t publish_tracks(livekit_eng_t *eng)
{
    if (eng->options.media.audio_info.codec == ESP_PEER_AUDIO_CODEC_NONE &&
        eng->options.media.video_info.codec == ESP_PEER_VIDEO_CODEC_NONE) {
        ESP_LOGI(TAG, "No media tracks to publish");
        return LIVEKIT_ENG_ERR_NONE;
    }

    int ret = LIVEKIT_ENG_ERR_OTHER;
    do {
        if (media_stream_begin(eng) != LIVEKIT_ENG_ERR_NONE) {
            ret = LIVEKIT_ENG_ERR_MEDIA;
            break;
        }
        if (eng->options.media.audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE &&
            send_add_audio_track(eng) != LIVEKIT_ENG_ERR_NONE) {
            ret = LIVEKIT_ENG_ERR_SIGNALING;
            break;
        }
        if (eng->options.media.video_info.codec != ESP_PEER_VIDEO_CODEC_NONE &&
            send_add_video_track(eng) != LIVEKIT_ENG_ERR_NONE) {
            ret = LIVEKIT_ENG_ERR_SIGNALING;
            break;
        }
        ESP_LOGI(TAG, "Published media tracks");
        return LIVEKIT_ENG_ERR_NONE;
    } while (0);

    media_stream_end(eng);
    return ret;
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

    free_ice_servers(eng);
    eng->ice_servers = cfgs;
    eng->ice_server_count = cfg_count;

    return LIVEKIT_PEER_ERR_NONE;
}

static void on_peer_pub_state_changed(livekit_peer_state_t state, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    if (state == LIVEKIT_PEER_STATE_CONNECTED) {
        publish_tracks(eng);
    }
}

static void on_peer_sub_state_changed(livekit_peer_state_t state, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    if (state == LIVEKIT_PEER_STATE_CONNECTED) {
        // TODO: Subscribe
    }
}

static void on_peer_pub_offer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    livekit_sig_send_offer(eng->sig, sdp);
}

static void on_peer_sub_answer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    livekit_sig_send_answer(eng->sig, sdp);
}

static void on_peer_ice_candidate(const char *candidate, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    ESP_LOGI(TAG, "Peer generated ice candidate: %s", candidate);
}

static void on_peer_packet_received(livekit_pb_data_packet_t* packet, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    switch (packet->which_value) {
        case LIVEKIT_PB_DATA_PACKET_USER_TAG:
            eng->options.on_user_packet(&packet->value.user, eng->options.ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_RPC_REQUEST_TAG:
            eng->options.on_rpc_request(&packet->value.rpc_request, eng->options.ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_RPC_ACK_TAG:
            eng->options.on_rpc_ack(&packet->value.rpc_ack, eng->options.ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_RPC_RESPONSE_TAG:
            eng->options.on_rpc_response(&packet->value.rpc_response, eng->options.ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG:
            eng->options.on_stream_header(&packet->value.stream_header, eng->options.ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_STREAM_CHUNK_TAG:
            eng->options.on_stream_chunk(&packet->value.stream_chunk, eng->options.ctx);
            break;
        case LIVEKIT_PB_DATA_PACKET_STREAM_TRAILER_TAG:
            eng->options.on_stream_trailer(&packet->value.stream_trailer, eng->options.ctx);
            break;
        default:
            break;
    }
}

static void on_peer_sub_audio_info(esp_peer_audio_stream_info_t* info, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;

    av_render_audio_info_t render_info = {};
    convert_dec_aud_info(info, &render_info);
    ESP_LOGI(TAG, "Audio render info: codec=%d, sample_rate=%" PRIu32 ", channels=%" PRIu8,
        render_info.codec, render_info.sample_rate, render_info.channel);

    if (av_render_add_audio_stream(eng->renderer_handle, &render_info) != ESP_MEDIA_ERR_OK) {
        ESP_LOGE(TAG, "Failed to add audio stream to renderer");
        return;
    }
    eng->sub_audio_info = *info;
}

static void on_peer_sub_audio_frame(esp_peer_audio_frame_t* frame, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    if (eng->sub_audio_info.codec == ESP_PEER_AUDIO_CODEC_NONE) return;
    // TODO: Check engine state before rendering

     av_render_audio_data_t audio_data = {
        .pts = frame->pts,
        .data = frame->data,
        .size = frame->size,
    };
    av_render_add_audio_data(eng->renderer_handle, &audio_data);
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

static bool disconnect_peer(livekit_peer_handle_t *peer)
{
    if (*peer == NULL) return false;
    if (livekit_peer_disconnect(*peer) != LIVEKIT_PEER_ERR_NONE) return false;
    if (livekit_peer_destroy(*peer) != LIVEKIT_PEER_ERR_NONE) return false;
    *peer = NULL;
    return true;
}

static bool connect_peer(livekit_eng_t *eng, livekit_peer_options_t *options, livekit_peer_handle_t *peer)
{
    disconnect_peer(peer);
    if (livekit_peer_create(peer, options) != LIVEKIT_PEER_ERR_NONE) return false;
    if (livekit_peer_connect(*peer) != LIVEKIT_PEER_ERR_NONE) return false;
    return true;
}

static void on_sig_join(livekit_pb_join_response_t *join_res, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;

    if (join_res->subscriber_primary) {
        ESP_LOGE(TAG, "Subscriber primary is not supported yet");
        return;
    }

    // set_ice_servers(eng, join_res->ice_servers, join_res->ice_servers_count);

    livekit_peer_options_t options = {
        // Options common to both peers
        .force_relay = join_res->has_client_configuration &&
                       join_res->client_configuration.force_relay == LIVEKIT_PB_CLIENT_CONFIG_SETTING_ENABLED,
        .media = &eng->options.media,
        .server_list = eng->ice_servers,
        .server_count = eng->ice_server_count,
        .on_ice_candidate = on_peer_ice_candidate,
        .on_packet_received = on_peer_packet_received,
        .ctx = eng
    };

    // 1. Publisher peer
    options.is_primary = !join_res->subscriber_primary;
    options.target = LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER;
    options.on_state_changed = on_peer_pub_state_changed;
    options.on_sdp = on_peer_pub_offer;

    if (!connect_peer(eng, &options, &eng->pub_peer)) {
       ESP_LOGE(TAG, "Failed to connect publisher peer");
       return;
    }

    // 2. Subscriber peer
    options.is_primary = join_res->subscriber_primary;
    options.target = LIVEKIT_PB_SIGNAL_TARGET_SUBSCRIBER;
    options.on_state_changed = on_peer_sub_state_changed;
    options.on_sdp = on_peer_sub_answer;
    options.on_audio_info = on_peer_sub_audio_info;
    options.on_audio_frame = on_peer_sub_audio_frame;

    if (!connect_peer(eng, &options, &eng->sub_peer)) {
        ESP_LOGE(TAG, "Failed to connect subscriber peer");
        return;
    }
}

static void on_sig_leave(livekit_pb_disconnect_reason_t reason, livekit_pb_leave_request_action_t action, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    // TODO: Handle reconnect, update engine state
    disconnect_peer(&eng->pub_peer);
    disconnect_peer(&eng->sub_peer);
}

static void on_sig_answer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
    livekit_peer_handle_sdp(eng->pub_peer, sdp);
}

static void on_sig_offer(const char *sdp, void *ctx)
{
    livekit_eng_t *eng = (livekit_eng_t *)ctx;
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
        .on_leave = on_sig_leave,
        .on_answer = on_sig_answer,
        .on_offer = on_sig_offer,
        .on_trickle = on_sig_trickle
    };

     if (livekit_sig_create(&eng->sig, &sig_options) != LIVEKIT_SIG_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create signaling client");
        free(eng);
        return LIVEKIT_ENG_ERR_SIGNALING;
    }

     esp_capture_sink_cfg_t sink_cfg = {
        .audio_info = {
            .codec = capture_audio_codec_type(eng->options.media.audio_info.codec),
            .sample_rate = eng->options.media.audio_info.sample_rate,
            .channel = eng->options.media.audio_info.channel,
            .bits_per_sample = 16,
        },
        .video_info = {
            .codec = capture_video_codec_type(eng->options.media.video_info.codec),
            .width = eng->options.media.video_info.width,
            .height = eng->options.media.video_info.height,
            .fps = eng->options.media.video_info.fps,
        },
    };

    if (options->media.audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE) {
        // TODO: Can we ensure the renderer is valid? If not, return error.
        eng->renderer_handle = options->media.renderer;
    }
    esp_capture_setup_path(eng->options.media.capturer, ESP_CAPTURE_PATH_PRIMARY, &sink_cfg, &eng->capturer_path);
    esp_capture_enable_path(eng->capturer_path, ESP_CAPTURE_RUN_TYPE_ALWAYS);
    // TODO: Handle capturer error

    *handle = eng;
    return LIVEKIT_ENG_ERR_NONE;
}

livekit_eng_err_t livekit_eng_destroy(livekit_eng_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_ENG_ERR_INVALID_ARG;
    }
    livekit_eng_t *eng = (livekit_eng_t *)handle;

    if (eng->pub_peer != NULL) {
        livekit_peer_destroy(eng->pub_peer);
    }
    if (eng->sub_peer != NULL) {
        livekit_peer_destroy(eng->sub_peer);
    }
    livekit_sig_destroy(eng->sig);
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

    media_stream_end(eng);
    // TODO: Reset just the stream that was added in case users added their own streams?
    av_render_reset(eng->renderer_handle);

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

    if (eng->pub_peer == NULL ||
        livekit_peer_send_data_packet(eng->pub_peer, packet, kind) != LIVEKIT_PEER_ERR_NONE) {
        return LIVEKIT_ENG_ERR_RTC;
    }
    return LIVEKIT_ENG_ERR_NONE;
}