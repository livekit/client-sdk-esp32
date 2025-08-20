#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "media_lib_os.h"
#include "esp_codec_dev.h"
#include <inttypes.h>
#include <stdlib.h>
#include "esp_log.h"
#include "url.h"
#include "signaling.h"
#include "peer.h"
#include "utils.h"
#include "engine.h"

// MARK: - Constants
static const char* TAG = "livekit_engine";

// MARK: - Type definitions

/// Engine state machine state.
typedef enum {
    ENGINE_STATE_DISCONNECTED,
    ENGINE_STATE_CONNECTING,
    ENGINE_STATE_CONNECTED,
    ENGINE_STATE_BACKOFF
} engine_state_t;

/// Type of event processed by the engine state machine.
typedef enum {
    EV_CMD_CONNECT,         /// User-initiated connect.
    EV_CMD_CLOSE,           /// User-initiated disconnect.
    EV_SIG_STATE,           /// Signal state changed.
    EV_SIG_RES,             /// Signal response received.
    EV_PEER_PUB_STATE,      /// Publisher peer state changed.
    EV_PEER_SUB_STATE,      /// Subscriber peer state changed.
    EV_PEER_DATA_PACKET,    /// Peer received data packet.
    EV_TIMER_EXP,           /// Timer expired.
    EV_MAX_RETRIES_REACHED, /// Maximum number of retry attempts reached.
    _EV_STATE_ENTER,        /// State enter hook (internal).
    _EV_STATE_EXIT,         /// State exit hook (internal).
} engine_event_type_t;

/// An event processed by the engine state machine.
typedef struct {
    /// Type of event, determines which union member is valid in `detail`.
    engine_event_type_t type;
    union {
        /// Detail for `EV_CMD_CONNECT`.
        struct {
            char *server_url;
            char *token;
        } cmd_connect;

        /// Detail for `EV_SIG_RES`.
        livekit_pb_signal_response_t res;

        /// Detail for `EV_PEER_DATA_PACKET`.
        livekit_pb_data_packet_t data_packet;

        /// Detail for `EV_SIG_STATE`, `EV_PEER_PUB_STATE` and `EV_PEER_SUB_STATE`.
        connection_state_t state;
    } detail;
} engine_event_t;

typedef struct {
    engine_state_t state;
    engine_options_t options;

    signal_handle_t signal_handle;
    peer_handle_t pub_peer_handle;
    peer_handle_t sub_peer_handle;

    esp_codec_dev_handle_t renderer_handle;
    esp_capture_path_handle_t capturer_path;
    bool is_media_streaming;

    // Session state
    bool is_subscriber_primary;
    bool force_relay;
    char* server_url;
    char* token;
    livekit_pb_sid_t local_participant_sid;

    TaskHandle_t task_handle;
    QueueHandle_t event_queue;
    TimerHandle_t timer;
    bool is_running;
    uint16_t retry_count;
} engine_t;

static inline bool event_enqueue(engine_t *eng, engine_event_t *ev, bool send_to_front)
{
    bool enqueued = (send_to_front ?
        xQueueSendToFront(eng->event_queue, ev, 0) :
        xQueueSend(eng->event_queue, ev, 0)) == pdPASS;
    if (!enqueued) {
        ESP_LOGE(TAG, "Failed to enqueue event: type=%d", ev->type);
    }
    return enqueued;
}

// MARK: - Subscribed media

/// Converts `esp_peer_audio_codec_t` to equivalent `av_render_audio_codec_t` value.
static inline av_render_audio_codec_t get_dec_codec(esp_peer_audio_codec_t codec)
{
    switch (codec) {
        case ESP_PEER_AUDIO_CODEC_G711A: return AV_RENDER_AUDIO_CODEC_G711A;
        case ESP_PEER_AUDIO_CODEC_G711U: return AV_RENDER_AUDIO_CODEC_G711U;
        case ESP_PEER_AUDIO_CODEC_OPUS:  return AV_RENDER_AUDIO_CODEC_OPUS;
        default:                         return AV_RENDER_AUDIO_CODEC_NONE;
    }
}

/// Maps `esp_peer_audio_stream_info_t` to `av_render_audio_info_t`.
static inline void convert_dec_aud_info(esp_peer_audio_stream_info_t *info, av_render_audio_info_t *dec_info)
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

// MARK: - Published media

/// Converts `esp_peer_audio_codec_t` to equivalent `esp_capture_codec_type_t` value.
static inline esp_capture_codec_type_t capture_audio_codec_type(esp_peer_audio_codec_t peer_codec)
{
    switch (peer_codec) {
        case ESP_PEER_AUDIO_CODEC_G711A: return ESP_CAPTURE_CODEC_TYPE_G711A;
        case ESP_PEER_AUDIO_CODEC_G711U: return ESP_CAPTURE_CODEC_TYPE_G711U;
        case ESP_PEER_AUDIO_CODEC_OPUS:  return ESP_CAPTURE_CODEC_TYPE_OPUS;
        default:                         return ESP_CAPTURE_CODEC_TYPE_NONE;
    }
}

/// Converts `esp_peer_video_codec_t` to equivalent `esp_capture_codec_type_t` value.
static inline esp_capture_codec_type_t capture_video_codec_type(esp_peer_video_codec_t peer_codec)
{
    switch (peer_codec) {
        case ESP_PEER_VIDEO_CODEC_H264:  return ESP_CAPTURE_CODEC_TYPE_H264;
        case ESP_PEER_VIDEO_CODEC_MJPEG: return ESP_CAPTURE_CODEC_TYPE_MJPEG;
        default:                         return ESP_CAPTURE_CODEC_TYPE_NONE;
    }
}

/// Captures and sends a single audio frame over the peer connection.
__attribute__((always_inline))
static inline void _media_stream_send_audio(engine_t *eng)
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
        peer_send_audio(eng->pub_peer_handle, &audio_send_frame);
        esp_capture_release_path_frame(eng->capturer_path, &audio_frame);
    }
}

/// Captures and sends a single video frame over the peer connection.
__attribute__((always_inline))
static inline void _media_stream_send_video(engine_t *eng)
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
        peer_send_video(eng->pub_peer_handle, &video_send_frame);
        esp_capture_release_path_frame(eng->capturer_path, &video_frame);
    }
}

static void media_stream_task(void *arg)
{
    engine_t *eng = (engine_t *)arg;
    while (eng->is_media_streaming) {
        if (eng->options.media.audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE) {
            _media_stream_send_audio(eng);
        }
        if (eng->options.media.video_info.codec != ESP_PEER_VIDEO_CODEC_NONE) {
            _media_stream_send_video(eng);
        }
        media_lib_thread_sleep(CONFIG_LK_PUB_INTERVAL_MS);
    }
    media_lib_thread_destroy(NULL);
}

static engine_err_t media_stream_begin(engine_t *eng)
{
    if (esp_capture_start(eng->options.media.capturer) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to start capture");
        return ENGINE_ERR_MEDIA;
    }
    media_lib_thread_handle_t handle = NULL;
    eng->is_media_streaming = true;
    if (media_lib_thread_create_from_scheduler(&handle, STREAM_THREAD_NAME, media_stream_task, eng) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create media stream thread");
        eng->is_media_streaming = false;
        return ENGINE_ERR_MEDIA;
    }
    return ENGINE_ERR_NONE;
}

static engine_err_t media_stream_end(engine_t *eng)
{
    if (!eng->is_media_streaming) {
        return ENGINE_ERR_NONE;
    }
    eng->is_media_streaming = false;
    esp_capture_stop(eng->options.media.capturer);
    return ENGINE_ERR_NONE;
}

static engine_err_t send_add_audio_track(engine_t *eng)
{
    bool is_stereo = eng->options.media.audio_info.channel == 2;
    livekit_pb_add_track_request_t req = {
        .cid = "a0",
        .name = CONFIG_LK_PUB_AUDIO_TRACK_NAME,
        .type = LIVEKIT_PB_TRACK_TYPE_AUDIO,
        .source = LIVEKIT_PB_TRACK_SOURCE_MICROPHONE,
        .muted = false,
        .audio_features_count = is_stereo ? 1 : 0,
        .audio_features = { LIVEKIT_PB_AUDIO_TRACK_FEATURE_TF_STEREO },
        .layers_count = 0
    };

    if (signal_send_add_track(eng->signal_handle, &req) != SIGNAL_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to publish audio track");
        return ENGINE_ERR_SIGNALING;
    }
    return ENGINE_ERR_NONE;
}

static engine_err_t send_add_video_track(engine_t *eng)
{
    livekit_pb_video_layer_t video_layer = {
        .quality = LIVEKIT_PB_VIDEO_QUALITY_HIGH,
        .width = eng->options.media.video_info.width,
        .height = eng->options.media.video_info.height
    };
    livekit_pb_add_track_request_t req = {
        .cid = "v0",
        .name = CONFIG_LK_PUB_VIDEO_TRACK_NAME,
        .type = LIVEKIT_PB_TRACK_TYPE_VIDEO,
        .source = LIVEKIT_PB_TRACK_SOURCE_CAMERA,
        .muted = false,
        .layers_count = 1,
        .layers = { video_layer },
        .audio_features_count = 0
    };

    if (signal_send_add_track(eng->signal_handle, &req) != SIGNAL_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to publish video track");
        return ENGINE_ERR_SIGNALING;
    }
    return ENGINE_ERR_NONE;
}

/// Begins media streaming and sends add track requests.
static engine_err_t publish_tracks(engine_t *eng)
{
    if (eng->options.media.audio_info.codec == ESP_PEER_AUDIO_CODEC_NONE &&
        eng->options.media.video_info.codec == ESP_PEER_VIDEO_CODEC_NONE) {
        ESP_LOGI(TAG, "No media tracks to publish");
        return ENGINE_ERR_NONE;
    }

    int ret = ENGINE_ERR_OTHER;
    do {
        if (media_stream_begin(eng) != ENGINE_ERR_NONE) {
            ret = ENGINE_ERR_MEDIA;
            break;
        }
        if (eng->options.media.audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE &&
            send_add_audio_track(eng) != ENGINE_ERR_NONE) {
            ret = ENGINE_ERR_SIGNALING;
            break;
        }
        if (eng->options.media.video_info.codec != ESP_PEER_VIDEO_CODEC_NONE &&
            send_add_video_track(eng) != ENGINE_ERR_NONE) {
            ret = ENGINE_ERR_SIGNALING;
            break;
        }
        return ENGINE_ERR_NONE;
    } while (0);

    media_stream_end(eng);
    return ret;
}

// MARK: - Signal event handlers

static void on_signal_state_changed(connection_state_t state, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_SIG_STATE,
        .detail.state = state
    };
    event_enqueue(eng, &ev, true);
}

static bool on_signal_res(livekit_pb_signal_response_t *res, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_SIG_RES,
        .detail.res = *res
    };
    // Returning true takes ownership of the response; it will be freed later when the
    // queue is processed or flushed.
    bool send_to_front = res->which_message == LIVEKIT_PB_SIGNAL_RESPONSE_LEAVE_TAG;
    return event_enqueue(eng, &ev, send_to_front);
}

// MARK: - Common peer event handlers

static bool on_peer_data_packet(livekit_pb_data_packet_t* packet, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_PEER_DATA_PACKET,
        .detail.data_packet = *packet
    };
    // Returning true indicates ownership of the packet; it will be freed when
    // the queue is processed or flushed.
    return event_enqueue(eng, &ev, false);
}

static void on_peer_ice_candidate(const char *candidate, void *ctx)
{
    // TODO: Handle ICE candidate
}

// MARK: - Publisher peer event handlers

static void on_peer_pub_state_changed(connection_state_t state, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_PEER_PUB_STATE,
        .detail.state = state
    };
    event_enqueue(eng, &ev, true);
}

static void on_peer_pub_offer(const char *sdp, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    signal_send_offer(eng->signal_handle, sdp);
}

// MARK: - Subscriber peer event handlers

static void on_peer_sub_state_changed(connection_state_t state, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_PEER_SUB_STATE,
        .detail.state = state
    };
    event_enqueue(eng, &ev, true);
}

static void on_peer_sub_answer(const char *sdp, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    signal_send_answer(eng->signal_handle, sdp);
}

static void on_peer_sub_audio_info(esp_peer_audio_stream_info_t* info, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    if (eng->state != ENGINE_STATE_CONNECTED) return;

    av_render_audio_info_t render_info = {};
    convert_dec_aud_info(info, &render_info);
    ESP_LOGD(TAG, "Audio render info: codec=%d, sample_rate=%" PRIu32 ", channels=%" PRIu8,
        render_info.codec, render_info.sample_rate, render_info.channel);

    if (av_render_add_audio_stream(eng->renderer_handle, &render_info) != ESP_MEDIA_ERR_OK) {
        ESP_LOGE(TAG, "Failed to add audio stream to renderer");
        return;
    }
}

static void on_peer_sub_audio_frame(esp_peer_audio_frame_t* frame, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    if (eng->state != ENGINE_STATE_CONNECTED) return;

    av_render_audio_data_t audio_data = {
        .pts = frame->pts,
        .data = frame->data,
        .size = frame->size,
    };
    av_render_add_audio_data(eng->renderer_handle, &audio_data);
}

// MARK: - Timer expired handler

static void on_timer_expired(TimerHandle_t timer)
{
    engine_t *eng = (engine_t *)pvTimerGetTimerID(timer);
    engine_event_t ev = { .type = EV_TIMER_EXP };
    event_enqueue(eng, &ev, true);
}

// MARK: - Peer lifecycle

static inline void _create_and_connect_peer(peer_options_t *options, peer_handle_t *peer)
{
    if (peer_create(peer, options) != PEER_ERR_NONE)
        return;
    if (peer_connect(*peer) != PEER_ERR_NONE) {
        peer_destroy(*peer);
        *peer = NULL;
    }
}

static inline void _disconnect_and_destroy_peer(peer_handle_t *peer)
{
    if (!peer || !*peer) return;
    peer_disconnect(*peer);
    peer_destroy(*peer);
    *peer = NULL;
}

static void destroy_peer_connections(engine_t *eng)
{
    _disconnect_and_destroy_peer(&eng->pub_peer_handle);
    _disconnect_and_destroy_peer(&eng->sub_peer_handle);
}

static bool establish_peer_connections(engine_t *eng)
{
    peer_options_t options = {
        .force_relay        = eng->force_relay,
        .media              = &eng->options.media,
        .on_data_packet     = on_peer_data_packet,
        .on_ice_candidate   = on_peer_ice_candidate,
        .ctx                = eng
    };

    // Publisher
    options.target           = LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER;
    options.on_state_changed = on_peer_pub_state_changed;
    options.on_sdp           = on_peer_pub_offer;

    _create_and_connect_peer(&options, &eng->pub_peer_handle);
    if (eng->pub_peer_handle == NULL)
        return false;

    // Subscriber
    options.target           = LIVEKIT_PB_SIGNAL_TARGET_SUBSCRIBER;
    options.on_state_changed = on_peer_sub_state_changed;
    options.on_sdp           = on_peer_sub_answer;
    options.on_audio_info    = on_peer_sub_audio_info;
    options.on_audio_frame   = on_peer_sub_audio_frame;

    _create_and_connect_peer(&options, &eng->sub_peer_handle);
    if (eng->sub_peer_handle == NULL) {
        _disconnect_and_destroy_peer(&eng->pub_peer_handle);
        return false;
    }
    return true;
}

// MARK: - Connection state machine

static bool handle_state(engine_t *eng, engine_event_t *ev, engine_state_t state);
static void flush_event_queue(engine_t *eng);
static void event_free(engine_event_t *ev);

static void engine_task(void *arg)
{
    engine_t *eng = (engine_t *)arg;
    while (eng->is_running) {
        engine_event_t ev;
        if (!xQueueReceive(eng->event_queue, &ev, portMAX_DELAY)) {
            ESP_LOGE(TAG, "Failed to receive event");
            continue;
        }
        assert(ev.type != _EV_STATE_ENTER && ev.type != _EV_STATE_EXIT);
        ESP_LOGI(TAG, "Event: %d", ev.type);

        engine_state_t state = eng->state;

        // Invoke the handler for the current state, passing the event that woke up the
        // state machine. If the handler returns true, it takes ownership of the event
        // and is responsible for freeing it, otherwise, it will be freed after the handler
        // returns.
        if (!handle_state(eng, &ev, state)) {
            event_free(&ev);
        }

        // If the state changed, invoke the exit handler for the old state,
        // the enter handler for the new state, and notify.
        if (eng->state != state) {
            ESP_LOGI(TAG, "State changed: %d -> %d", state, eng->state);

            state = eng->state;
            handle_state(eng, &(engine_event_t){ .type = _EV_STATE_EXIT }, state);
            assert(eng->state == state);
            handle_state(eng, &(engine_event_t){ .type = _EV_STATE_ENTER }, eng->state);
            assert(eng->state == state);

            // Map engine state to external state, notify of state change.
            if (eng->options.on_state_changed) {
                connection_state_t ext_state;
                switch (eng->state) {
                    case ENGINE_STATE_DISCONNECTED: ext_state = CONNECTION_STATE_DISCONNECTED; break;
                    case ENGINE_STATE_CONNECTING:   ext_state = eng->retry_count > 0 ?
                                                                    CONNECTION_STATE_RECONNECTING :
                                                                    CONNECTION_STATE_CONNECTING;
                                                                    break;
                    case ENGINE_STATE_BACKOFF:      ext_state = CONNECTION_STATE_RECONNECTING; break;
                    case ENGINE_STATE_CONNECTED:    ext_state = CONNECTION_STATE_CONNECTED;    break;
                    default:                        ext_state = CONNECTION_STATE_DISCONNECTED; break;
                }
                eng->options.on_state_changed(ext_state, eng->options.ctx);
            }
        }
    }

    // Discard any remaining events in the queue before exiting.
    flush_event_queue(eng);
    vTaskDelete(NULL);
}

static bool handle_state_disconnected(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            // Clean up resources from previous connection (if any)
            media_stream_end(eng);
            signal_close(eng->signal_handle);
            destroy_peer_connections(eng);

            eng->is_subscriber_primary = false;
            eng->force_relay = false;
            eng->local_participant_sid[0] = '\0';
            eng->retry_count = 0;
            break;
        case EV_CMD_CONNECT:
            if (eng->server_url != NULL) free(eng->server_url);
            if (eng->token != NULL) free(eng->token);
            eng->server_url = ev->detail.cmd_connect.server_url;
            eng->token = ev->detail.cmd_connect.token;
            eng->state = ENGINE_STATE_CONNECTING;
            return true;
        default:
            break;
    }
    return false;
}

static bool handle_state_connecting(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            signal_connect(eng->signal_handle, eng->server_url, eng->token);
            break;
        case EV_CMD_CLOSE:
            // TODO: Send leave request
            eng->state = ENGINE_STATE_DISCONNECTED;
            break;
        case EV_CMD_CONNECT:
            ESP_LOGW(TAG, "Engine already connecting, ignoring connect command");
            break;
        case EV_SIG_RES:
            livekit_pb_signal_response_t *res = &ev->detail.res;
            switch (res->which_message) {
                case LIVEKIT_PB_SIGNAL_RESPONSE_LEAVE_TAG:
                    ESP_LOGI(TAG, "Server sent leave before fully connected");
                    eng->state = ENGINE_STATE_DISCONNECTED;
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG:
                    livekit_pb_join_response_t *join = &res->message.join;
                    // Store connection settings
                    eng->is_subscriber_primary = join->subscriber_primary;
                    if (join->has_client_configuration) {
                        eng->force_relay = join->client_configuration.force_relay
                            == LIVEKIT_PB_CLIENT_CONFIG_SETTING_ENABLED;
                    }
                    // Store local participant SID
                    strncpy(
                        eng->local_participant_sid,
                        join->participant.sid,
                        sizeof(eng->local_participant_sid)
                    );
                    if (!establish_peer_connections(eng)) {
                        ESP_LOGE(TAG, "Failed to establish peer connections");
                        eng->state = ENGINE_STATE_DISCONNECTED;
                        break;
                    }
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_ANSWER_TAG:
                    livekit_pb_session_description_t *answer = &res->message.answer;
                    peer_handle_sdp(eng->pub_peer_handle, answer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_OFFER_TAG:
                    livekit_pb_session_description_t *offer = &res->message.offer;
                    peer_handle_sdp(eng->sub_peer_handle, offer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_TRICKLE_TAG:
                    livekit_pb_trickle_request_t *trickle = &res->message.trickle;
                    char* candidate = NULL;
                    if (!protocol_signal_trickle_get_candidate(trickle, &candidate)) {
                        break;
                    }
                    peer_handle_t target_peer = trickle->target == LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER ?
                        eng->pub_peer_handle : eng->sub_peer_handle;
                    peer_handle_ice_candidate(target_peer, candidate);
                    free(candidate);
                    break;
                default:
                    break;
            }
            break;
        case EV_SIG_STATE:
            // TODO: Check error code (4xx should go to disconnected)
            if (ev->detail.state == CONNECTION_STATE_FAILED ||
                ev->detail.state == CONNECTION_STATE_DISCONNECTED) {
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_PUB_STATE:
            if (!eng->is_subscriber_primary &&
                ev->detail.state == CONNECTION_STATE_CONNECTED) {
                eng->state = ENGINE_STATE_CONNECTED;
            } else if (ev->detail.state == CONNECTION_STATE_FAILED ||
                       ev->detail.state == CONNECTION_STATE_DISCONNECTED) {
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_SUB_STATE:
            if (eng->is_subscriber_primary &&
                ev->detail.state == CONNECTION_STATE_CONNECTED) {
                eng->state = ENGINE_STATE_CONNECTED;
            } else if (ev->detail.state == CONNECTION_STATE_FAILED ||
                       ev->detail.state == CONNECTION_STATE_DISCONNECTED) {
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        default:
            break;
    }
    return false;
}

static bool handle_state_connected(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            eng->retry_count = 0;
            publish_tracks(eng);
            break;
        case EV_CMD_CLOSE:
            // TODO: Send leave request
            eng->state = ENGINE_STATE_DISCONNECTED;
            break;
        case EV_CMD_CONNECT:
            ESP_LOGW(TAG, "Engine already connected, ignoring connect command");
            break;
        case EV_PEER_DATA_PACKET:
            livekit_pb_data_packet_t *packet = &ev->detail.data_packet;
            if (eng->options.on_data_packet) {
                eng->options.on_data_packet(packet, eng->options.ctx);
            }
            break;
        case EV_SIG_RES:
            livekit_pb_signal_response_t *res = &ev->detail.res;
            switch (ev->detail.res.which_message) {
                case LIVEKIT_PB_SIGNAL_RESPONSE_LEAVE_TAG:
                    ESP_LOGI(TAG, "Server initiated disconnect");
                    // TODO: Handle leave action
                    eng->state = ENGINE_STATE_DISCONNECTED;
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_ROOM_UPDATE_TAG:
                    livekit_pb_room_update_t *room_update = &res->message.room_update;
                    if (eng->options.on_room_info && room_update->has_room) {
                        eng->options.on_room_info(&room_update->room, eng->options.ctx);
                    }
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_UPDATE_TAG:
                    livekit_pb_participant_update_t *update = &res->message.update;
                    if (!eng->options.on_participant_info) {
                        break;
                    }
                    bool found_local = false;
                    for (pb_size_t i = 0; i < update->participants_count; i++) {
                        livekit_pb_participant_info_t *participant = &update->participants[i];
                        bool is_local = !found_local && strncmp(
                            participant->sid,
                            eng->local_participant_sid,
                            sizeof(eng->local_participant_sid)
                        ) == 0;
                        if (is_local) found_local = true;
                        eng->options.on_participant_info(participant, is_local, eng->options.ctx);
                    }
                    break;
                // TODO: Only handle if needed
                case LIVEKIT_PB_SIGNAL_RESPONSE_ANSWER_TAG:
                    livekit_pb_session_description_t *answer = &res->message.answer;
                    peer_handle_sdp(eng->pub_peer_handle, answer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_OFFER_TAG:
                    livekit_pb_session_description_t *offer = &res->message.offer;
                    peer_handle_sdp(eng->sub_peer_handle, offer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_TRICKLE_TAG:
                    livekit_pb_trickle_request_t *trickle = &res->message.trickle;
                    char* candidate = NULL;
                    if (!protocol_signal_trickle_get_candidate(trickle, &candidate)) {
                        break;
                    }
                    peer_handle_t target_peer = trickle->target == LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER ?
                        eng->pub_peer_handle : eng->sub_peer_handle;
                    peer_handle_ice_candidate(target_peer, candidate);
                    free(candidate);
                    break;
                default:
                    break;
            }
            break;
        case EV_SIG_STATE:
            if (ev->detail.state == CONNECTION_STATE_FAILED ||
                ev->detail.state == CONNECTION_STATE_DISCONNECTED) {
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_PUB_STATE:
            if (ev->detail.state == CONNECTION_STATE_FAILED ||
                ev->detail.state == CONNECTION_STATE_DISCONNECTED) {
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_SUB_STATE:
            if (ev->detail.state == CONNECTION_STATE_FAILED ||
                ev->detail.state == CONNECTION_STATE_DISCONNECTED) {
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        default:
            break;
    }
    return false;
}

static bool handle_state_backoff(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            media_stream_end(eng);
            signal_close(eng->signal_handle);
            destroy_peer_connections(eng);

            eng->retry_count++;

            if (eng->retry_count >= CONFIG_LK_MAX_RETRIES) {
                ESP_LOGW(TAG, "Max retries reached");
                event_enqueue(eng, &(engine_event_t){ .type = EV_MAX_RETRIES_REACHED }, true);
                break;
            }
            uint16_t backoff_ms = backoff_ms_for_attempt(eng->retry_count);
            ESP_LOGI(TAG, "Attempting reconnect %d/%d in %" PRIu16 "ms",
                eng->retry_count, CONFIG_LK_MAX_RETRIES, backoff_ms);

            xTimerChangePeriod(eng->timer, pdMS_TO_TICKS(backoff_ms), 0);
            xTimerStart(eng->timer, 0);
            break;
        case EV_MAX_RETRIES_REACHED:
            eng->state = ENGINE_STATE_DISCONNECTED;
            break;
        case EV_TIMER_EXP:
            eng->state = ENGINE_STATE_CONNECTING;
            break;
        case _EV_STATE_EXIT:
            xTimerStop(eng->timer, portMAX_DELAY);
            break;
        default:
            break;
    }
    return false;
}

static inline bool handle_state(engine_t *eng, engine_event_t *ev, engine_state_t state)
{
    switch (state) {
        case ENGINE_STATE_DISCONNECTED: return handle_state_disconnected(eng, ev);
        case ENGINE_STATE_CONNECTING:   return handle_state_connecting(eng, ev);
        case ENGINE_STATE_CONNECTED:    return handle_state_connected(eng, ev);
        case ENGINE_STATE_BACKOFF:      return handle_state_backoff(eng, ev);
        default:                        esp_system_abort("Unknown engine state");
    }
}

static void flush_event_queue(engine_t *eng)
{
    engine_event_t ev;
    int count = 0;
    while (xQueueReceive(eng->event_queue, &ev, 0) == pdPASS) {
        count++;
        event_free(&ev);
    }
    ESP_LOGI(TAG, "Flushed %d events", count);
}

static void event_free(engine_event_t *ev)
{
    if (ev == NULL) return;
    switch (ev->type) {
        case EV_CMD_CONNECT:
            if (ev->detail.cmd_connect.server_url != NULL)
                free(ev->detail.cmd_connect.server_url);
            if (ev->detail.cmd_connect.token != NULL)
                free(ev->detail.cmd_connect.token);
            break;
        case EV_PEER_DATA_PACKET:
            protocol_data_packet_free(&ev->detail.data_packet);
            break;
        case EV_SIG_RES:
            protocol_signal_res_free(&ev->detail.res);
            break;
        default: break;
    }
}

// MARK: - Public API

engine_err_t engine_create(engine_handle_t *handle, engine_options_t *options)
{
    engine_t *eng = (engine_t *)calloc(1, sizeof(engine_t));
    if (eng == NULL) {
        return ENGINE_ERR_NO_MEM;
    }

    eng->event_queue = xQueueCreate(CONFIG_LK_ENGINE_QUEUE_SIZE, sizeof(engine_event_t));
    if (eng->event_queue == NULL) {
        free(eng);
        return ENGINE_ERR_NO_MEM;
    }

    eng->timer = xTimerCreate(
        "lk_engine_timer",
        pdMS_TO_TICKS(1000),
        pdFALSE,
        eng,
        on_timer_expired);

    if (eng->timer == NULL) {
        free(eng->event_queue);
        free(eng);
        return ENGINE_ERR_NO_MEM;
    }

    signal_options_t signal_options = {
        .ctx = eng,
        .on_state_changed = on_signal_state_changed,
        .on_res = on_signal_res,
    };
    if (signal_create(&eng->signal_handle, &signal_options) != SIGNAL_ERR_NONE) {
        free(eng->event_queue);
        free(eng->timer);
        free(eng);
        return ENGINE_ERR_SIGNALING;
    }

    eng->options = *options;
    eng->state = ENGINE_STATE_DISCONNECTED;
    eng->is_running = true;

    if (xTaskCreate(engine_task, "engine_task", 4096, eng, 5, &eng->task_handle) != pdPASS) {
        free(eng->event_queue);
        free(eng->timer);
        free(eng);
        return ENGINE_ERR_NO_MEM;
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
    vTaskDelete(eng->task_handle);

    xTimerDelete(eng->timer, portMAX_DELAY);
    vQueueDelete(eng->event_queue);

    signal_destroy(eng->signal_handle);
    if (eng->pub_peer_handle != NULL) {
        peer_destroy(eng->pub_peer_handle);
    }

    if (eng->server_url != NULL) free(eng->server_url);
    if (eng->token != NULL) free(eng->token);
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

    engine_event_t ev = {
        .type = EV_CMD_CONNECT,
        .detail.cmd_connect = { .server_url = strdup(server_url), .token = strdup(token) }
    };
    if (!event_enqueue(eng, &ev, true)) {
        return ENGINE_ERR_OTHER;
    }
    return ENGINE_ERR_NONE;
}

engine_err_t engine_close(engine_handle_t handle)
{
    if (handle == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;

    engine_event_t ev = { .type = EV_CMD_CLOSE };
    if (!event_enqueue(eng, &ev, true)) {
        return ENGINE_ERR_OTHER;
    }
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