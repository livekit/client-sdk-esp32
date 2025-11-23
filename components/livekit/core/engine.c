/*
 * Copyright 2025 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "media_lib_os.h"
#include "esp_capture_sink.h"
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
    EV_PEER_STATE,          /// Peer state changed.
    EV_PEER_SDP,            /// Peer provided SDP.
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

        /// Detail for `EV_SIG_STATE`.
        signal_state_t sig_state;

        /// Detail for `EV_PEER_SDP`.
        struct {
            char *sdp;
            peer_role_t role;
        } peer_sdp;

        /// Detail for `EV_PEER_STATE`.
        struct {
            connection_state_t state;
            peer_role_t role;
        } peer_state;
    } detail;
} engine_event_t;

typedef struct {
    bool is_subscriber_primary;
    livekit_pb_sid_t local_participant_sid;
    livekit_pb_sid_t sub_audio_track_sid;
} session_state_t;

typedef struct {
    engine_state_t state;
    engine_options_t options;

    signal_handle_t signal_handle;
    peer_handle_t pub_peer_handle;
    peer_handle_t sub_peer_handle;

    av_render_handle_t renderer_handle;
    esp_capture_sink_handle_t capturer_path;
    bool is_media_streaming;

    char* server_url;
    char* token;
    session_state_t session;

    TaskHandle_t task_handle;
    QueueHandle_t event_queue;
    TimerHandle_t timer;
    bool is_running;
    uint16_t retry_count;
    livekit_failure_reason_t failure_reason;
} engine_t;

static bool event_enqueue(engine_t *eng, engine_event_t *ev, bool send_to_front);

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

static engine_err_t subscribe_tracks(engine_t *eng, livekit_pb_track_info_t *tracks, int count)
{
    if (tracks == NULL || count <= 0) {
        return ENGINE_ERR_INVALID_ARG;
    }
    if (eng->session.sub_audio_track_sid[0] != '\0') {
        return ENGINE_ERR_MAX_SUB;
    }
    for (int i = 0; i < count; i++) {
        livekit_pb_track_info_t *track = &tracks[i];
        if (track->type != LIVEKIT_PB_TRACK_TYPE_AUDIO) {
            continue;
        }
        // For now, subscribe to the first audio track.
        ESP_LOGI(TAG, "Subscribing to audio track: sid=%s", track->sid);
        signal_send_update_subscription(eng->signal_handle, track->sid, true);
        strlcpy(eng->session.sub_audio_track_sid, track->sid, sizeof(eng->session.sub_audio_track_sid));
        break;
    }
    return ENGINE_ERR_NONE;
}

static void on_peer_sub_audio_info(esp_peer_audio_stream_info_t* info, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;

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
    av_render_audio_data_t audio_data = {
        .pts = frame->pts,
        .data = frame->data,
        .size = (uint32_t)frame->size,
    };
    av_render_add_audio_data(eng->renderer_handle, &audio_data);
}

// MARK: - Published media

/// Converts `esp_peer_audio_codec_t` to equivalent `esp_capture_format_id_t` value.
static inline esp_capture_format_id_t capture_audio_codec_type(esp_peer_audio_codec_t peer_codec)
{
    switch (peer_codec) {
        case ESP_PEER_AUDIO_CODEC_G711A: return ESP_CAPTURE_FMT_ID_G711A;
        case ESP_PEER_AUDIO_CODEC_G711U: return ESP_CAPTURE_FMT_ID_G711U;
        case ESP_PEER_AUDIO_CODEC_OPUS:  return ESP_CAPTURE_FMT_ID_OPUS;
        default:                         return ESP_CAPTURE_FMT_ID_NONE;
    }
}

/// Converts `esp_peer_video_codec_t` to equivalent `esp_capture_format_id_t` value.
static inline esp_capture_format_id_t capture_video_codec_type(esp_peer_video_codec_t peer_codec)
{
    switch (peer_codec) {
        case ESP_PEER_VIDEO_CODEC_H264:  return ESP_CAPTURE_FMT_ID_H264;
        case ESP_PEER_VIDEO_CODEC_MJPEG: return ESP_CAPTURE_FMT_ID_MJPEG;
        default:                         return ESP_CAPTURE_FMT_ID_NONE;
    }
}

/// Captures and sends a single audio frame over the peer connection.
__attribute__((always_inline))
static inline void _media_stream_send_audio(engine_t *eng)
{
    esp_capture_stream_frame_t audio_frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO,
    };
    while (esp_capture_sink_acquire_frame(eng->capturer_path, &audio_frame, true) == ESP_CAPTURE_ERR_OK) {
        esp_peer_audio_frame_t audio_send_frame = {
            .pts = audio_frame.pts,
            .data = audio_frame.data,
            .size = audio_frame.size,
        };
        peer_send_audio(eng->pub_peer_handle, &audio_send_frame);
        esp_capture_sink_release_frame(eng->capturer_path, &audio_frame);
    }
}

/// Captures and sends a single video frame over the peer connection.
__attribute__((always_inline))
static inline void _media_stream_send_video(engine_t *eng)
{
    esp_capture_stream_frame_t video_frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
    };
    if (esp_capture_sink_acquire_frame(eng->capturer_path, &video_frame, true) == ESP_CAPTURE_ERR_OK) {
        esp_peer_video_frame_t video_send_frame = {
            .pts = video_frame.pts,
            .data = video_frame.data,
            .size = video_frame.size,
        };
        peer_send_video(eng->pub_peer_handle, &video_send_frame);
        esp_capture_sink_release_frame(eng->capturer_path, &video_frame);
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
    if (media_lib_thread_create_from_scheduler(&handle, "lk_eng_stream", media_stream_task, eng) != ESP_OK) {
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
        .width = (uint32_t)eng->options.media.video_info.width,
        .height = (uint32_t)eng->options.media.video_info.height
    };
    livekit_pb_add_track_request_t req = {
        .cid = "v0",
        .name = CONFIG_LK_PUB_VIDEO_TRACK_NAME,
        .type = LIVEKIT_PB_TRACK_TYPE_VIDEO,
        .source = LIVEKIT_PB_TRACK_SOURCE_CAMERA,
        .muted = false,
        .width = video_layer.width,
        .height = video_layer.height,
        .layers_count = 1,
        .layers = { video_layer },
        .backup_codec_policy = LIVEKIT_PB_BACKUP_CODEC_POLICY_REGRESSION
    };

    if (signal_send_add_track(eng->signal_handle, &req) != SIGNAL_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to publish video track");
        return ENGINE_ERR_SIGNALING;
    }
    return ENGINE_ERR_NONE;
}

/// Send add track requests based on the media options.
///
/// Note: SFU expects add track request before publisher peer offer is sent.
///
static engine_err_t send_add_track_requests(engine_t *eng)
{
    if (eng->options.media.audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE &&
        send_add_audio_track(eng) != ENGINE_ERR_NONE) {
        return ENGINE_ERR_SIGNALING;
    }
    if (eng->options.media.video_info.codec != ESP_PEER_VIDEO_CODEC_NONE &&
        send_add_video_track(eng) != ENGINE_ERR_NONE) {
        return ENGINE_ERR_SIGNALING;
    }
    return ENGINE_ERR_NONE;
}

// MARK: - Signal event handlers

static void on_signal_state_changed(signal_state_t state, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_SIG_STATE,
        .detail.sig_state = state
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

static void on_peer_state_changed(connection_state_t state, peer_role_t role, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_PEER_STATE,
        .detail.peer_state = { .state = state, .role = role }
    };
    event_enqueue(eng, &ev, true);
}

static void on_peer_sdp(const char *sdp, peer_role_t role, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    engine_event_t ev = {
        .type = EV_PEER_SDP,
        .detail.peer_sdp = { .sdp = strdup(sdp), .role = role }
    };
    event_enqueue(eng, &ev, false);
}


static bool on_peer_data_packet(livekit_pb_data_packet_t* packet, void *ctx)
{
    engine_t *eng = (engine_t *)ctx;
    // TODO: Implement buffering for incoming data packets
    if (eng->options.on_data_packet) {
        eng->options.on_data_packet(packet, eng->options.ctx);
    }
    return false;
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

/// Maps list of `livekit_pb_ice_server_t` to list of `esp_peer_ice_server_cfg_t`.
///
/// Note:
/// - A single `livekit_pb_ice_server_t` can contain multiple URLs, which
///   will map to multiple `esp_peer_ice_server_cfg_t` entries.
/// - Strings are not copied, so the caller must ensure the original ICE
///   server list stays alive until the peers are created.
///
static inline uint8_t map_ice_servers(
    const livekit_pb_ice_server_t *pb_servers_list,
    pb_size_t pb_servers_count,
    esp_peer_ice_server_cfg_t *server_list,
    uint8_t server_list_capacity
) {
    if (pb_servers_list      == NULL ||
        server_list          == NULL ||
        server_list_capacity == 0) {
        return 0;
    }
    uint8_t count = 0;
    for (pb_size_t i = 0; i < pb_servers_count; i++) {
        for (pb_size_t j = 0; j < pb_servers_list[i].urls_count; j++) {
            if (count >= server_list_capacity) {
                ESP_LOGW(TAG, "ICE server list capacity exceeded");
                return count;
            }
            server_list[count].stun_url = pb_servers_list[i].urls[j];
            server_list[count].user = pb_servers_list[i].username;
            server_list[count].psw = pb_servers_list[i].credential;
            count++;
        }
    }
    return count;
}

static bool establish_peer_connections(engine_t *eng, const livekit_pb_join_response_t *join)
{
    esp_peer_ice_server_cfg_t server_list[CONFIG_LK_MAX_ICE_SERVERS];
    uint8_t server_count = map_ice_servers(
        join->ice_servers,
        join->ice_servers_count,
        server_list,
        sizeof(server_list) / sizeof(server_list[0])
    );
    if (server_count < 1) {
        ESP_LOGW(TAG, "No ICE servers available");
        return false;
    }

    peer_options_t options = {
        .force_relay      = join->client_configuration.force_relay
            == LIVEKIT_PB_CLIENT_CONFIG_SETTING_ENABLED,
        .media            = &eng->options.media,
        .server_list      = server_list,
        .server_count     = server_count,
        .on_state_changed = on_peer_state_changed,
        .on_sdp           = on_peer_sdp,
        .on_data_packet   = on_peer_data_packet,
        .ctx              = eng
    };

    // 1. Publisher
    options.role          = PEER_ROLE_PUBLISHER;
    _create_and_connect_peer(&options, &eng->pub_peer_handle);
    if (eng->pub_peer_handle == NULL)
        return false;

    // 2. Subscriber
    options.role           = PEER_ROLE_SUBSCRIBER;
    options.on_audio_info  = on_peer_sub_audio_info;
    options.on_audio_frame = on_peer_sub_audio_frame;

    _create_and_connect_peer(&options, &eng->sub_peer_handle);
    if (eng->sub_peer_handle == NULL) {
        _disconnect_and_destroy_peer(&eng->pub_peer_handle);
        return false;
    }
    return true;
}

// MARK: - FSM helpers

/// Determines the external state that should be reported.
///
/// This is necessary because the engine FSM's states do not map 1:1 with the states
/// exposed in the public room API.
///
static inline bool map_engine_state(engine_t *eng, livekit_connection_state_t *out_state)
{
    switch (eng->state) {
        case ENGINE_STATE_DISCONNECTED:
            // Engine state machine doesn't have a discrete failed state
            *out_state = eng->failure_reason == LIVEKIT_FAILURE_REASON_NONE ?
                LIVEKIT_CONNECTION_STATE_DISCONNECTED :
                LIVEKIT_CONNECTION_STATE_FAILED;
            break;
        case ENGINE_STATE_CONNECTING:
            // Should only report connecting for initial connection attempt.
            if (eng->retry_count > 0) {
                return false;
            }
            *out_state = LIVEKIT_CONNECTION_STATE_CONNECTING;
            break;
        case ENGINE_STATE_BACKOFF:
            *out_state = LIVEKIT_CONNECTION_STATE_RECONNECTING;
            break;
        case ENGINE_STATE_CONNECTED:
            *out_state = LIVEKIT_CONNECTION_STATE_CONNECTED;
            break;
        default:
            return false;
    }
    return true;
}

/// Map a signal failed state to a failure reason exposed in the public room API.
static livekit_failure_reason_t map_signal_fail_state(signal_state_t state)
{
    switch (state) {
        case SIGNAL_STATE_FAILED_UNREACHABLE:  return LIVEKIT_FAILURE_REASON_UNREACHABLE;
        case SIGNAL_STATE_FAILED_PING_TIMEOUT: return LIVEKIT_FAILURE_REASON_PING_TIMEOUT;
        case SIGNAL_STATE_FAILED_BAD_TOKEN:    return LIVEKIT_FAILURE_REASON_BAD_TOKEN;
        case SIGNAL_STATE_FAILED_UNAUTHORIZED: return LIVEKIT_FAILURE_REASON_UNAUTHORIZED;
        default:                               return LIVEKIT_FAILURE_REASON_OTHER;
    }
}

/// Map a protocol disconnect reason to a failure reason exposed in the public room API.
static livekit_failure_reason_t map_disconnect_reason(livekit_pb_disconnect_reason_t reason)
{
    switch (reason) {
        case LIVEKIT_PB_DISCONNECT_REASON_CLIENT_INITIATED:    return LIVEKIT_FAILURE_REASON_NONE;
        case LIVEKIT_PB_DISCONNECT_REASON_DUPLICATE_IDENTITY:  return LIVEKIT_FAILURE_REASON_DUPLICATE_IDENTITY;
        case LIVEKIT_PB_DISCONNECT_REASON_SERVER_SHUTDOWN:     return LIVEKIT_FAILURE_REASON_SERVER_SHUTDOWN;
        case LIVEKIT_PB_DISCONNECT_REASON_PARTICIPANT_REMOVED: return LIVEKIT_FAILURE_REASON_PARTICIPANT_REMOVED;
        case LIVEKIT_PB_DISCONNECT_REASON_ROOM_DELETED:        return LIVEKIT_FAILURE_REASON_ROOM_DELETED;
        case LIVEKIT_PB_DISCONNECT_REASON_STATE_MISMATCH:      return LIVEKIT_FAILURE_REASON_STATE_MISMATCH;
        case LIVEKIT_PB_DISCONNECT_REASON_JOIN_FAILURE:        return LIVEKIT_FAILURE_REASON_JOIN_INCOMPLETE;
        case LIVEKIT_PB_DISCONNECT_REASON_MIGRATION:           return LIVEKIT_FAILURE_REASON_MIGRATION;
        case LIVEKIT_PB_DISCONNECT_REASON_SIGNAL_CLOSE:        return LIVEKIT_FAILURE_REASON_SIGNAL_CLOSE;
        case LIVEKIT_PB_DISCONNECT_REASON_ROOM_CLOSED:         return LIVEKIT_FAILURE_REASON_ROOM_CLOSED;
        case LIVEKIT_PB_DISCONNECT_REASON_USER_UNAVAILABLE:    return LIVEKIT_FAILURE_REASON_SIP_USER_UNAVAILABLE;
        case LIVEKIT_PB_DISCONNECT_REASON_USER_REJECTED:       return LIVEKIT_FAILURE_REASON_SIP_USER_REJECTED;
        case LIVEKIT_PB_DISCONNECT_REASON_SIP_TRUNK_FAILURE:   return LIVEKIT_FAILURE_REASON_SIP_TRUNK_FAILURE;
        case LIVEKIT_PB_DISCONNECT_REASON_CONNECTION_TIMEOUT:  return LIVEKIT_FAILURE_REASON_CONNECTION_TIMEOUT;
        case LIVEKIT_PB_DISCONNECT_REASON_MEDIA_FAILURE:       return LIVEKIT_FAILURE_REASON_MEDIA_FAILURE;
        default:                                               return LIVEKIT_FAILURE_REASON_OTHER;
    }
}

/// Frees an event's dynamically allocated fields (if any).
static void event_free(engine_event_t *ev)
{
    if (ev == NULL) return;
    switch (ev->type) {
        case EV_CMD_CONNECT:
            SAFE_FREE(ev->detail.cmd_connect.server_url);
            SAFE_FREE(ev->detail.cmd_connect.token);
            break;
        case EV_SIG_RES:
            protocol_signal_response_free(&ev->detail.res);
            break;
        case EV_PEER_SDP:
            SAFE_FREE(ev->detail.peer_sdp.sdp);
            break;
        default: break;
    }
}

/// Enqueues an event.
static bool event_enqueue(engine_t *eng, engine_event_t *ev, bool send_to_front)
{
    bool enqueued = (send_to_front ?
        xQueueSendToFront(eng->event_queue, ev, 0) :
        xQueueSend(eng->event_queue, ev, 0)) == pdPASS;
    if (!enqueued) {
        ESP_LOGE(TAG, "Failed to enqueue event: type=%d", ev->type);
    }
    return enqueued;
}

/// Dequeues all events from the queue and frees them.
static void flush_event_queue(engine_t *eng)
{
    engine_event_t ev;
    while (xQueueReceive(eng->event_queue, &ev, 0) == pdPASS) {
        event_free(&ev);
    }
}

/// Starts the timer for the given period.
///
/// Enqueues `EV_TIMER_EXP` after the period has elapsed.
///
static inline void timer_start(engine_t *eng, uint16_t period)
{
    xTimerChangePeriod(eng->timer, pdMS_TO_TICKS(period), 0);
    xTimerStart(eng->timer, 0);
}

/// Stops the timer.
static inline void timer_stop(engine_t *eng)
{
    xTimerStop(eng->timer, 0);
}

static bool handle_join(engine_t *eng, const livekit_pb_join_response_t *join)
{
    // 1. Store connection settings
    eng->session.is_subscriber_primary = join->subscriber_primary;

    // 2. Store local Participant SID
    strlcpy(
        eng->session.local_participant_sid,
        join->participant.sid,
        sizeof(eng->session.local_participant_sid)
    );

    // 3. Dispatch initial room info
    if (eng->options.on_room_info && join->has_room) {
        eng->options.on_room_info(&join->room, eng->options.ctx);
    }

    // 4. Dispatch initial participant info
    if (eng->options.on_participant_info) {
        eng->options.on_participant_info(&join->participant, true, eng->options.ctx);
        for (pb_size_t i = 0; i < join->other_participants_count; i++) {
            eng->options.on_participant_info(&join->other_participants[i], false, eng->options.ctx);
        }
    }

    // 5. Establish peer connections
    if (!establish_peer_connections(eng, join)) {
        ESP_LOGE(TAG, "Failed to establish peer connections");
        return false;
    }

    // 6. Subscribe to remote tracks that have already been published.
    for (pb_size_t i = 0; i < join->other_participants_count; i++) {
        engine_err_t ret = subscribe_tracks(
            eng,
            join->other_participants[i].tracks,
            join->other_participants[i].tracks_count
        );
        if (ret == ENGINE_ERR_MAX_SUB) break;
    }
    return true;
}

static void handle_trickle(engine_t *eng, const livekit_pb_trickle_request_t *trickle)
{
    char* candidate = NULL;
    if (!protocol_signal_trickle_get_candidate(trickle, &candidate)) {
        return;
    }
    peer_handle_t target_peer = trickle->target == LIVEKIT_PB_SIGNAL_TARGET_PUBLISHER ?
        eng->pub_peer_handle : eng->sub_peer_handle;
    peer_handle_ice_candidate(target_peer, candidate);
    free(candidate);
}

static void handle_room_update(engine_t *eng, const livekit_pb_room_update_t *room_update)
{
    if (eng->options.on_room_info && room_update->has_room) {
        eng->options.on_room_info(&room_update->room, eng->options.ctx);
    }
}

static void handle_participant_update(engine_t *eng, const livekit_pb_participant_update_t *update)
{
    bool found_local = false;
    for (pb_size_t i = 0; i < update->participants_count; i++) {
        const livekit_pb_participant_info_t *participant = &update->participants[i];
        bool is_local = !found_local && strncmp(
            participant->sid,
            eng->session.local_participant_sid,
            sizeof(eng->session.local_participant_sid)
        ) == 0;
        if (is_local) {
            found_local = true;
        } else {
            subscribe_tracks(eng, participant->tracks, participant->tracks_count);
        }
        if (eng->options.on_participant_info) {
            eng->options.on_participant_info(participant, is_local, eng->options.ctx);
        }
    }
}

/// Cleans up resources and state from the previous connection.
static void cleanup_previous_connection(engine_t *eng)
{
    media_stream_end(eng);
    signal_close(eng->signal_handle);
    destroy_peer_connections(eng);
    memset(&eng->session, 0, sizeof(eng->session));
}

// MARK: - State: Disconnected

/// Handler for `ENGINE_STATE_DISCONNECTED`.
static bool handle_state_disconnected(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            cleanup_previous_connection(eng);
            eng->retry_count = 0;
            break;
        case EV_CMD_CONNECT:
            SAFE_FREE(eng->server_url);
            SAFE_FREE(eng->token);
            eng->server_url = ev->detail.cmd_connect.server_url;
            eng->token = ev->detail.cmd_connect.token;
            eng->failure_reason = LIVEKIT_FAILURE_REASON_NONE;
            eng->state = ENGINE_STATE_CONNECTING;
            return true;
        default:
            break;
    }
    return false;
}

// MARK: - State: Connecting

/// Handler for `ENGINE_STATE_CONNECTING`.
static bool handle_state_connecting(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            signal_connect(eng->signal_handle, eng->server_url, eng->token);
            break;
        case EV_CMD_CLOSE:
            signal_send_leave(eng->signal_handle);
            eng->state = ENGINE_STATE_DISCONNECTED;
            break;
        case EV_CMD_CONNECT:
            ESP_LOGW(TAG, "Engine already connecting, ignoring connect command");
            break;
        case EV_SIG_RES:
            const livekit_pb_signal_response_t *res = &ev->detail.res;
            switch (res->which_message) {
                case LIVEKIT_PB_SIGNAL_RESPONSE_LEAVE_TAG:
                    const livekit_pb_leave_request_t *leave = &res->message.leave;
                    eng->failure_reason = map_disconnect_reason(leave->reason);
                    eng->state = ENGINE_STATE_DISCONNECTED;
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_ROOM_UPDATE_TAG:
                    const livekit_pb_room_update_t *room_update = &res->message.room_update;
                    handle_room_update(eng, room_update);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_UPDATE_TAG:
                    const livekit_pb_participant_update_t *update = &res->message.update;
                    handle_participant_update(eng, update);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG:
                    const livekit_pb_join_response_t *join = &res->message.join;
                    if (!handle_join(eng, join)) {
                        eng->state = ENGINE_STATE_BACKOFF;
                    }
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_ANSWER_TAG:
                    const livekit_pb_session_description_t *answer = &res->message.answer;
                    peer_handle_sdp(eng->pub_peer_handle, answer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_OFFER_TAG:
                    const livekit_pb_session_description_t *offer = &res->message.offer;
                    peer_handle_sdp(eng->sub_peer_handle, offer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_TRICKLE_TAG:
                    const livekit_pb_trickle_request_t *trickle = &res->message.trickle;
                    handle_trickle(eng, trickle);
                    break;
                default:
                    break;
            }
            break;
        case EV_SIG_STATE:
            signal_state_t sig_state = ev->detail.sig_state;
            if (sig_state == SIGNAL_STATE_CONNECTED) {
                send_add_track_requests(eng);
            } else if(sig_state == SIGNAL_STATE_DISCONNECTED) {
                eng->failure_reason = LIVEKIT_FAILURE_REASON_OTHER;
                eng->state = ENGINE_STATE_BACKOFF;
            } else if (sig_state & SIGNAL_STATE_FAILED_ANY) {
                eng->failure_reason = map_signal_fail_state(sig_state);
                eng->state = (sig_state & SIGNAL_STATE_FAILED_CLIENT_ANY) ?
                    ENGINE_STATE_DISCONNECTED :
                    ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_STATE:
            connection_state_t peer_state = ev->detail.peer_state.state;
            peer_role_t role = ev->detail.peer_state.role;

            // If either peer fails or disconnects, transition to backoff
            if (peer_state == CONNECTION_STATE_DISCONNECTED ||
                peer_state == CONNECTION_STATE_FAILED) {
                eng->failure_reason = LIVEKIT_FAILURE_REASON_RTC;
                eng->state = ENGINE_STATE_BACKOFF;
                break;
            }
            // Once the primary peer is connected, transition to connected
            if (peer_state == CONNECTION_STATE_CONNECTED) {
                if ((role == PEER_ROLE_PUBLISHER && !eng->session.is_subscriber_primary) ||
                    (role == PEER_ROLE_SUBSCRIBER && eng->session.is_subscriber_primary)) {
                    eng->state = ENGINE_STATE_CONNECTED;
                }
            }
            break;
        case EV_PEER_SDP:
            const char *sdp = ev->detail.peer_sdp.sdp;
            peer_role_t sdp_role = ev->detail.peer_sdp.role;
            if (sdp_role == PEER_ROLE_PUBLISHER) {
                signal_send_offer(eng->signal_handle, sdp);
            } else {
                signal_send_answer(eng->signal_handle, sdp);
            }
            break;
        default:
            break;
    }
    return false;
}

// MARK: - State: Connected

/// Handler for `ENGINE_STATE_CONNECTED`.
static bool handle_state_connected(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            eng->retry_count = 0;
            eng->failure_reason = LIVEKIT_FAILURE_REASON_NONE;
            media_stream_begin(eng);
            break;
        case EV_CMD_CLOSE:
            signal_send_leave(eng->signal_handle);
            eng->state = ENGINE_STATE_DISCONNECTED;
            break;
        case EV_CMD_CONNECT:
            ESP_LOGW(TAG, "Engine already connected, ignoring connect command");
            break;
        case EV_SIG_RES:
            const livekit_pb_signal_response_t *res = &ev->detail.res;
            switch (res->which_message) {
                case LIVEKIT_PB_SIGNAL_RESPONSE_LEAVE_TAG:
                    const livekit_pb_leave_request_t *leave = &res->message.leave;
                    eng->failure_reason = map_disconnect_reason(leave->reason);
                    eng->state = ENGINE_STATE_DISCONNECTED;
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_ROOM_UPDATE_TAG:
                    const livekit_pb_room_update_t *room_update = &res->message.room_update;
                    handle_room_update(eng, room_update);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_UPDATE_TAG:
                    const livekit_pb_participant_update_t *update = &res->message.update;
                    handle_participant_update(eng, update);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_ANSWER_TAG:
                    const livekit_pb_session_description_t *answer = &res->message.answer;
                    peer_handle_sdp(eng->pub_peer_handle, answer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_OFFER_TAG:
                    const livekit_pb_session_description_t *offer = &res->message.offer;
                    peer_handle_sdp(eng->sub_peer_handle, offer->sdp);
                    break;
                case LIVEKIT_PB_SIGNAL_RESPONSE_TRICKLE_TAG:
                    const livekit_pb_trickle_request_t *trickle = &res->message.trickle;
                    handle_trickle(eng, trickle);
                    break;
                default:
                    break;
            }
            break;
        case EV_SIG_STATE:
            signal_state_t sig_state = ev->detail.sig_state;
            if (sig_state == SIGNAL_STATE_DISCONNECTED) {
                eng->failure_reason = LIVEKIT_FAILURE_REASON_OTHER;
                eng->state = ENGINE_STATE_BACKOFF;
            } else if (sig_state & SIGNAL_STATE_FAILED_ANY) {
                eng->failure_reason = map_signal_fail_state(sig_state);
                eng->state = (sig_state & SIGNAL_STATE_FAILED_CLIENT_ANY) ?
                    ENGINE_STATE_DISCONNECTED :
                    ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_STATE:
            connection_state_t peer_state = ev->detail.peer_state.state;
            peer_role_t role = ev->detail.peer_state.role;

            // If either peer fail or disconnects, transition to backoff
            if (peer_state == CONNECTION_STATE_DISCONNECTED ||
                peer_state == CONNECTION_STATE_FAILED) {
                ESP_LOGE(TAG, "%s peer connection failed",
                    role == PEER_ROLE_PUBLISHER ? "Publisher" : "Subscriber");
                eng->failure_reason = LIVEKIT_FAILURE_REASON_RTC;
                eng->state = ENGINE_STATE_BACKOFF;
            }
            break;
        case EV_PEER_SDP:
            const char *sdp = ev->detail.peer_sdp.sdp;
            peer_role_t sdp_role = ev->detail.peer_sdp.role;
            if (sdp_role != PEER_ROLE_SUBSCRIBER) {
                ESP_LOGW(TAG, "Unexpected SDP from publisher");
                break;
            }
            signal_send_answer(eng->signal_handle, sdp);
            break;
        default:
            break;
    }
    return false;
}

// MARK: - State: Backoff

/// Handler for `ENGINE_STATE_BACKOFF`.
static bool handle_state_backoff(engine_t *eng, const engine_event_t *ev)
{
    switch (ev->type) {
        case _EV_STATE_ENTER:
            cleanup_previous_connection(eng);

            eng->retry_count++;
            if (eng->retry_count > CONFIG_LK_MAX_RETRIES) {
                // State changes within enter/exit are not allowed; enqueue event instead.
                event_enqueue(eng, &(engine_event_t){ .type = EV_MAX_RETRIES_REACHED }, true);
                break;
            }
            uint16_t backoff_ms = backoff_ms_for_attempt(eng->retry_count);
            ESP_LOGI(TAG, "Reconnect in %" PRIu16 "ms: attempt=%d/%d, reason=%d",
                backoff_ms, eng->retry_count, CONFIG_LK_MAX_RETRIES, eng->failure_reason);

            timer_start(eng, backoff_ms);
            break;
        case EV_MAX_RETRIES_REACHED:
            eng->failure_reason = LIVEKIT_FAILURE_REASON_MAX_RETRIES;
            eng->state = ENGINE_STATE_DISCONNECTED;
            break;
        case EV_TIMER_EXP:
            eng->state = ENGINE_STATE_CONNECTING;
            break;
        case _EV_STATE_EXIT:
            timer_stop(eng);
            break;
        default:
            break;
    }
    return false;
}

/// Invokes the handler for the given state.
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

// MARK: - FSM task

static void engine_task(void *arg)
{
    engine_t *eng = (engine_t *)arg;
    while (eng->is_running) {
        engine_event_t ev;
        if (!xQueueReceive(eng->event_queue, &ev, portMAX_DELAY)) {
            ESP_LOGE(TAG, "Failed to receive event");
            continue;
        }
        // Internal events are not allowed to be enqueued.
        assert(ev.type != _EV_STATE_ENTER && ev.type != _EV_STATE_EXIT);
        ESP_LOGD(TAG, "Event: type=%d", ev.type);

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
            ESP_LOGD(TAG, "State changed: %d -> %d", state, eng->state);

            state = eng->state;
            handle_state(eng, &(engine_event_t){ .type = _EV_STATE_EXIT }, state);
            assert(eng->state == state);
            handle_state(eng, &(engine_event_t){ .type = _EV_STATE_ENTER }, eng->state);
            assert(eng->state == state);

            if (eng->options.on_state_changed) {
                livekit_connection_state_t ext_state;
                if (map_engine_state(eng, &ext_state)) {
                    eng->options.on_state_changed(ext_state, eng->options.ctx);
                }
            }
        }
    }

    // Discard any remaining events in the queue before exiting.
    flush_event_queue(eng);
    vTaskDelete(NULL);
}

static engine_err_t enable_capture_sink(engine_t *eng)
{
    esp_capture_sink_cfg_t sink_cfg = {
        .audio_info = {
            .format_id = capture_audio_codec_type(eng->options.media.audio_info.codec),
            .sample_rate = eng->options.media.audio_info.sample_rate,
            .channel = eng->options.media.audio_info.channel,
            .bits_per_sample = 16,
        },
        .video_info = {
            .format_id = ESP_CAPTURE_FMT_ID_H264,
            .width = (uint16_t)eng->options.media.video_info.width,
            .height = (uint16_t)eng->options.media.video_info.height,
            .fps = (uint8_t)eng->options.media.video_info.fps,
        },
    };

    if (esp_capture_sink_setup(
        eng->options.media.capturer,
        0, // Path index
        &sink_cfg,
        &eng->capturer_path
    ) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Capture sink setup failed");
        return ENGINE_ERR_MEDIA;
    }

    // TODO: Add muxer

    if (esp_capture_sink_enable(
        eng->capturer_path,
        ESP_CAPTURE_RUN_MODE_ALWAYS
    ) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Capture sink enable failed");
        return ENGINE_ERR_MEDIA;
    }
    return ENGINE_ERR_NONE;
}

// MARK: - Public API

engine_handle_t engine_init(const engine_options_t *options)
{
    engine_t *eng = (engine_t *)calloc(1, sizeof(engine_t));
    if (eng == NULL) {
        return NULL;
    }
    eng->options = *options;
    eng->state = ENGINE_STATE_DISCONNECTED;
    eng->is_running = true;

    eng->event_queue = xQueueCreate(
        CONFIG_LK_ENGINE_QUEUE_SIZE,
        sizeof(engine_event_t)
    );
    if (eng->event_queue == NULL) {
        goto _init_failed;
    }

    if (xTaskCreate(
        engine_task,
        "engine_task",
        CONFIG_LK_ENGINE_TASK_STACK_SIZE,
        (void *)eng,
        5,
        &eng->task_handle
    ) != pdPASS) {
        goto _init_failed;
    }

    eng->timer = xTimerCreate(
        "lk_engine_timer",
        pdMS_TO_TICKS(1000),
        pdFALSE,
        (void *)eng,
        on_timer_expired
    );
    if (eng->timer == NULL) {
        goto _init_failed;
    }

    signal_options_t signal_options = {
        .ctx = eng,
        .on_state_changed = on_signal_state_changed,
        .on_res = on_signal_res,
    };
    eng->signal_handle = signal_init(&signal_options);
    if (eng->signal_handle == NULL) {
        goto _init_failed;
    }
    eng->renderer_handle = options->media.renderer;

    if (enable_capture_sink(eng) != ENGINE_ERR_NONE) {
        goto _init_failed;
    }
    return eng;

_init_failed:
    engine_destroy(eng);
    return NULL;
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
        vTaskDelete(eng->task_handle);
    }
    if (eng->timer != NULL) {
        xTimerDelete(eng->timer, portMAX_DELAY);
    }
    if (eng->event_queue != NULL) {
        vQueueDelete(eng->event_queue);
    }
    if (eng->signal_handle != NULL) {
        signal_destroy(eng->signal_handle);
    }
    if (eng->pub_peer_handle != NULL) {
        peer_destroy(eng->pub_peer_handle);
    }
    if (eng->sub_peer_handle != NULL) {
        peer_destroy(eng->sub_peer_handle);
    }
    SAFE_FREE(eng->server_url);
    SAFE_FREE(eng->token);
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
        event_free(&ev);
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

livekit_failure_reason_t engine_get_failure_reason(engine_handle_t handle)
{
    if (handle == NULL) {
        return LIVEKIT_FAILURE_REASON_NONE;
    }
    engine_t *eng = (engine_t *)handle;
    return eng->failure_reason;
}

engine_err_t engine_send_data_packet(engine_handle_t handle, const livekit_pb_data_packet_t* packet, bool reliable)
{
    if (handle == NULL) {
        return ENGINE_ERR_INVALID_ARG;
    }
    engine_t *eng = (engine_t *)handle;
    // TODO: Implement buffering for reliable packets
    if (eng->state != ENGINE_STATE_CONNECTED) {
        return ENGINE_ERR_OTHER;
    }
    if (eng->pub_peer_handle == NULL ||
        peer_send_data_packet(eng->pub_peer_handle, packet, reliable) != PEER_ERR_NONE) {
        return ENGINE_ERR_RTC;
    }
    return ENGINE_ERR_NONE;
}