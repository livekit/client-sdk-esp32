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

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_peer.h"
#include "esp_peer_default.h"
#include "media_lib_os.h"
#include "utils.h"

#include "peer.h"

static const char *SUB_TAG = "livekit_peer.sub";
static const char *PUB_TAG = "livekit_peer.pub";
#define TAG(peer) (peer->options.role == PEER_ROLE_SUBSCRIBER ? SUB_TAG : PUB_TAG)

#define RELIABLE_CHANNEL_LABEL "_reliable"
#define LOSSY_CHANNEL_LABEL "_lossy"
#define STREAM_ID_INVALID 0xFFFF

#define PC_EXIT_BIT      (1 << 0)
#define PC_PAUSED_BIT    (1 << 1)
#define PC_RESUME_BIT    (1 << 2)
#define PC_SEND_QUIT_BIT (1 << 3)

// RFC 6464 audio level RTP header extension constants
#define AUDIO_LEVEL_URI         "urn:ietf:params:rtp-hdrext:ssrc-audio-level"
#define AUDIO_LEVEL_DEFAULT     30  // Placeholder: -30 dBov (fixed level, not measured from actual audio)
#define RTP_EXT_BLOCK_SIZE      8   // 4-byte profile+length header + 1-byte element ID + 1-byte audio level + 2-byte padding

// MARK: - Audio level RTP header extension (RFC 6464)

/// Finds the smallest unused extmap ID (1-14) within an SDP media section.
static uint8_t sdp_find_unused_extmap_id(const char *section_start, const char *section_end)
{
    uint16_t used_ids = 0;
    const char *p = section_start;
    while ((p = strstr(p, "\na=extmap:")) != NULL && p < section_end) {
        p += 10; // skip "\na=extmap:"
        int id = atoi(p);
        if (id >= 1 && id <= 14) {
            used_ids |= (1u << id);
        }
    }
    for (uint8_t id = 1; id <= 14; id++) {
        if (!(used_ids & (1u << id))) {
            return id;
        }
    }
    return 0;
}

/// Parses the Opus payload type from an SDP string.
static uint8_t sdp_parse_opus_payload_type(const char *sdp)
{
    const char *p = sdp;
    while ((p = strstr(p, "a=rtpmap:")) != NULL) {
        p += 9;
        int pt = atoi(p);
        const char *eol = strpbrk(p, "\r\n");
        if (eol == NULL) eol = p + strlen(p);
        const char *opus = strstr(p, " opus/");
        if (opus != NULL && opus < eol && pt >= 0 && pt <= 127) {
            return (uint8_t)pt;
        }
    }
    return 0;
}

/// Injects the ssrc-audio-level extmap attribute into the audio section of an SDP string.
/// Dynamically selects an unused extmap ID (1-14) to avoid conflicts.
/// Sets *out_extmap_id to the chosen ID on success.
/// Returns a newly allocated SDP string with the extmap line inserted, or NULL on failure.
/// The caller must free the returned string.
static char *sdp_inject_audio_level_extmap(const char *sdp, uint8_t *out_extmap_id)
{
    // Find "m=audio" section
    const char *m_audio = strstr(sdp, "m=audio");
    if (m_audio == NULL) {
        return NULL;
    }

    // Determine audio section boundary (ends at next "m=" line or end of string)
    const char *audio_section_end = strstr(m_audio + 1, "\nm=");
    if (audio_section_end == NULL) {
        audio_section_end = sdp + strlen(sdp);
    }

    // Find an unused extmap ID
    uint8_t extmap_id = sdp_find_unused_extmap_id(m_audio, audio_section_end);
    if (extmap_id == 0) {
        return NULL;
    }

    // Find direction attribute as insertion point
    const char *insert_before = NULL;
    const char *p;
    p = strstr(m_audio, "\na=sendrecv");
    if (p != NULL && p < audio_section_end) {
        insert_before = p + 1; // skip the leading \n
    }
    if (insert_before == NULL) {
        p = strstr(m_audio, "\na=sendonly");
        if (p != NULL && p < audio_section_end) {
            insert_before = p + 1;
        }
    }
    if (insert_before == NULL) {
        p = strstr(m_audio, "\na=recvonly");
        if (p != NULL && p < audio_section_end) {
            insert_before = p + 1;
        }
    }
    if (insert_before == NULL) {
        return NULL;
    }

    // Build the extmap line
    char extmap_line[80];
    int extmap_len = snprintf(extmap_line, sizeof(extmap_line),
        "a=extmap:%u %s\r\n", extmap_id, AUDIO_LEVEL_URI);

    size_t orig_len = strlen(sdp);
    char *new_sdp = (char *)malloc(orig_len + (size_t)extmap_len + 1);
    if (new_sdp == NULL) {
        return NULL;
    }

    // Copy everything before the insertion point
    size_t prefix_len = (size_t)(insert_before - sdp);
    memcpy(new_sdp, sdp, prefix_len);
    // Insert the extmap line
    memcpy(new_sdp + prefix_len, extmap_line, (size_t)extmap_len);
    // Copy the rest (including null terminator)
    memcpy(new_sdp + prefix_len + (size_t)extmap_len, insert_before, orig_len - prefix_len + 1);

    *out_extmap_id = extmap_id;
    return new_sdp;
}

typedef struct {
    peer_options_t options;
    esp_peer_role_t ice_role;
    esp_peer_handle_t connection;

    connection_state_t state;

    bool running;
    bool pause;
    media_lib_event_grp_handle_t wait_event;

    uint16_t reliable_stream_id;
    uint16_t lossy_stream_id;

    uint8_t audio_level_extmap_id;  // Negotiated extmap ID for audio level extension
    uint8_t opus_payload_type;      // Negotiated Opus payload type from SDP

#if CONFIG_LK_BENCHMARK
    uint64_t start_time;
#endif
} peer_t;

/// RTP transformer callback: compute encoded size (original + 8 bytes for extension block).
static int audio_level_get_encoded_size(esp_peer_rtp_frame_t *frame, bool *in_place, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    // Skip if Opus PT not yet parsed or doesn't match this packet
    if (peer == NULL || peer->opus_payload_type == 0 ||
        frame->payload_type != peer->opus_payload_type) {
        return ESP_PEER_ERR_NOT_SUPPORT;
    }
    // Validate minimum RTP packet size (12 bytes fixed header)
    if (frame->orig_data == NULL || frame->orig_size < 12) {
        return ESP_PEER_ERR_NOT_SUPPORT;
    }
    // Skip if packet already has header extensions (X bit set)
    if (frame->orig_data[0] & 0x10) {
        return ESP_PEER_ERR_NOT_SUPPORT;
    }
    // Validate CSRC count doesn't exceed packet size
    uint8_t cc = frame->orig_data[0] & 0x0F;
    if (frame->orig_size < (uint32_t)(12 + cc * 4)) {
        return ESP_PEER_ERR_NOT_SUPPORT;
    }
    frame->encoded_size = frame->orig_size + RTP_EXT_BLOCK_SIZE;
    *in_place = false;
    return 0;
}

/// RTP transformer callback: inject audio level header extension into RTP packet.
///
/// Inserts an RFC 5285 one-byte header extension with RFC 6464 audio level data
/// between the RTP fixed header and the payload.
///
/// Packet layout after transform:
///   [RTP Header (X bit set)] [Extension Block (8 bytes)] [Payload]
///
static int audio_level_transform(esp_peer_rtp_frame_t *frame, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    uint8_t *orig = frame->orig_data;
    uint8_t *enc = frame->encoded_data;
    if (orig == NULL || enc == NULL) {
        return ESP_PEER_ERR_NOT_SUPPORT;
    }

    uint8_t extmap_id = (peer != NULL) ? peer->audio_level_extmap_id : 1;

    // Calculate RTP header length: 12 bytes fixed + 4 * CC (CSRC count)
    uint8_t cc = orig[0] & 0x0F;
    uint32_t header_len = 12u + (uint32_t)cc * 4u;

    // Copy RTP header
    memcpy(enc, orig, header_len);

    // Set the X (extension) bit in the first byte
    enc[0] |= 0x10;

    // Build the extension block (8 bytes total, one 32-bit word of extension data)
    uint8_t *ext = enc + header_len;
    ext[0] = 0xBE;                             // RFC 5285 one-byte header profile
    ext[1] = 0xDE;
    ext[2] = 0x00;                             // Extension length: 1 word (32 bits)
    ext[3] = 0x01;
    ext[4] = (extmap_id << 4) | 0;             // ID=negotiated, L=0 (1 byte of data follows)
    ext[5] = 0x80 | AUDIO_LEVEL_DEFAULT;       // V=1 (voice active), placeholder level
    ext[6] = 0x00;                             // Padding
    ext[7] = 0x00;                             // Padding

    // Copy payload after the extension block
    memcpy(enc + header_len + RTP_EXT_BLOCK_SIZE,
           orig + header_len,
           frame->orig_size - header_len);

    return 0;
}

static esp_peer_rtp_transform_cb_t audio_level_transform_cb = {
    .get_encoded_size = audio_level_get_encoded_size,
    .transform = audio_level_transform
};

static esp_peer_media_dir_t get_media_direction(esp_peer_media_dir_t direction, peer_role_t role) {
    switch (role) {
        case PEER_ROLE_PUBLISHER:  return direction & ESP_PEER_MEDIA_DIR_SEND_ONLY;
        case PEER_ROLE_SUBSCRIBER: return direction & ESP_PEER_MEDIA_DIR_RECV_ONLY;
        default:                   return ESP_PEER_MEDIA_DIR_NONE;
    }
}

static void peer_task(void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
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

static void create_data_channels(peer_t *peer)
{
    esp_peer_data_channel_cfg_t reliable_cfg = {
        .label = RELIABLE_CHANNEL_LABEL,
        .type = ESP_PEER_DATA_CHANNEL_RELIABLE,
        .ordered = true
    };
    if (esp_peer_create_data_channel(peer->connection, &reliable_cfg) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to create reliable data channel");
    }

    esp_peer_data_channel_cfg_t lossy_cfg = {
        .label = LOSSY_CHANNEL_LABEL,
        .type = ESP_PEER_DATA_CHANNEL_PARTIAL_RELIABLE_RETX,
        .ordered = false,
        .max_retransmit_count = 0
    };
    if (esp_peer_create_data_channel(peer->connection, &lossy_cfg) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to create lossy data channel");
    }
}

static int on_state(esp_peer_state_t rtc_state, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    ESP_LOGD(TAG(peer), "RTC state changed to %d", rtc_state);

    connection_state_t new_state = peer->state;
    switch (rtc_state) {
        case ESP_PEER_STATE_CONNECT_FAILED:
            new_state = CONNECTION_STATE_FAILED;
            break;
        case ESP_PEER_STATE_DISCONNECTED:
            new_state = CONNECTION_STATE_DISCONNECTED;
            break;
        case ESP_PEER_STATE_PAIRING:
            new_state = CONNECTION_STATE_CONNECTING;
            break;
        case ESP_PEER_STATE_CONNECTED:
            if (peer->options.role == PEER_ROLE_PUBLISHER) {
                create_data_channels(peer);
            }
            break;
        case ESP_PEER_STATE_DATA_CHANNEL_OPENED:
            // Don't enter the connected state until both data channels are opened.
            if (peer->reliable_stream_id == STREAM_ID_INVALID ||
                peer->lossy_stream_id    == STREAM_ID_INVALID ) break;
            new_state = CONNECTION_STATE_CONNECTED;
#if CONFIG_LK_BENCHMARK
            ESP_LOGI(TAG(peer), "[BENCH] Connected in %" PRIu64 "ms",
                get_unix_time_ms() - peer->start_time);
#endif
            break;
        default:
            break;
    }
    if (new_state != peer->state) {
        ESP_LOGI(TAG(peer), "State changed: %d -> %d", peer->state, new_state);
        peer->state = new_state;
        peer->options.on_state_changed(new_state, peer->options.role, peer->options.ctx);
    }
    return 0;
}

static int on_msg(esp_peer_msg_t *info, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    switch (info->type) {
        case ESP_PEER_MSG_TYPE_SDP: {
            const char *sdp = (const char *)info->data;

            // For publisher SDP, inject audio level extmap for Active Speaker detection
            if (peer->options.role == PEER_ROLE_PUBLISHER) {
                uint8_t extmap_id = 0;
                char *patched_sdp = sdp_inject_audio_level_extmap(sdp, &extmap_id);
                if (patched_sdp != NULL) {
                    peer->audio_level_extmap_id = extmap_id;
                    peer->opus_payload_type = sdp_parse_opus_payload_type(patched_sdp);
                    ESP_LOGD(TAG(peer), "Generated offer (with audio-level extmap id=%u):\n%s",
                        extmap_id, patched_sdp);
                    peer->options.on_sdp(patched_sdp, peer->options.role, peer->options.ctx);
                    free(patched_sdp);
                } else {
                    ESP_LOGW(TAG(peer), "Failed to inject extmap, sending original SDP");
                    ESP_LOGD(TAG(peer), "Generated offer:\n%s", sdp);
                    peer->options.on_sdp(sdp, peer->options.role, peer->options.ctx);
                }
            } else {
                ESP_LOGD(TAG(peer), "Generated answer:\n%s", sdp);
                peer->options.on_sdp(sdp, peer->options.role, peer->options.ctx);
            }
            break;
        }
        default:
            ESP_LOGD(TAG(peer), "Unhandled msg type: %d", info->type);
            break;
    }
    return 0;
}

static int on_audio_info(esp_peer_audio_stream_info_t *info, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    if (peer->options.on_audio_info != NULL) {
        peer->options.on_audio_info(info, peer->options.ctx);
    }
    return 0;
}

static int on_audio_data(esp_peer_audio_frame_t *info, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    if (peer->options.on_audio_frame != NULL) {
        peer->options.on_audio_frame(info, peer->options.ctx);
    }
    return 0;
}

static int on_video_info(esp_peer_video_stream_info_t *info, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    if (peer->options.on_video_info != NULL) {
        peer->options.on_video_info(info, peer->options.ctx);
    }
    return 0;
}

static int on_video_data(esp_peer_video_frame_t *info, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    if (peer->options.on_video_frame != NULL) {
        peer->options.on_video_frame(info, peer->options.ctx);
    }
    return 0;
}

static int on_channel_open(esp_peer_data_channel_info_t *ch, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    ESP_LOGI(TAG(peer), "Channel open: label=%s, stream_id=%d", ch->label, ch->stream_id);

    if (strcmp(ch->label, RELIABLE_CHANNEL_LABEL) == 0) {
        peer->reliable_stream_id = ch->stream_id;
    } else if (strcmp(ch->label, LOSSY_CHANNEL_LABEL) == 0) {
        peer->lossy_stream_id = ch->stream_id;
    }
    return 0;
}

static int on_channel_close(esp_peer_data_channel_info_t *ch, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    ESP_LOGI(TAG(peer), "Channel close: label=%s, stream_id=%d", ch->label, ch->stream_id);

    if (strcmp(ch->label, RELIABLE_CHANNEL_LABEL) == 0) {
        peer->reliable_stream_id = STREAM_ID_INVALID;
    } else if (strcmp(ch->label, LOSSY_CHANNEL_LABEL) == 0) {
        peer->lossy_stream_id = STREAM_ID_INVALID;
    }
    return 0;
}

static int on_data(esp_peer_data_frame_t *frame, void *ctx)
{
    peer_t *peer = (peer_t *)ctx;
    ESP_LOGD(TAG(peer), "Data received: size=%d, stream_id=%d", frame->size, frame->stream_id);

    if (peer->options.on_data_packet == NULL) {
        ESP_LOGE(TAG(peer), "Packet received handler is not set");
        return -1;
    }
    if (frame->type != ESP_PEER_DATA_CHANNEL_DATA) {
        ESP_LOGE(TAG(peer), "Unexpected data frame type: %d", frame->type);
        return -1;
    }

    livekit_pb_data_packet_t packet = {};
    if (!protocol_data_packet_decode((const uint8_t *)frame->data, (size_t)frame->size, &packet)) {
        ESP_LOGE(TAG(peer), "Failed to decode data packet");
        return -1;
    }
    if (packet.which_value == 0) {
        // Packet type is not supported yet.
        protocol_data_packet_free(&packet);
        return -1;
    }
    if (!peer->options.on_data_packet(&packet, peer->options.ctx)) {
        // Ownership was not taken.
        protocol_data_packet_free(&packet);
    }
    return 0;
}

peer_err_t peer_create(peer_handle_t *handle, peer_options_t *options)
{
    if (handle == NULL ||
        options->on_state_changed == NULL ||
        options->on_sdp == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    if (options->media->video_info.codec == ESP_PEER_VIDEO_CODEC_MJPEG) {
        // MJPEG over data channel is not supported yet
        return PEER_ERR_INVALID_ARG;
    }

    peer_t *peer = (peer_t *)calloc(1, sizeof(peer_t));
    if (peer == NULL) {
        return PEER_ERR_NO_MEM;
    }
    media_lib_event_group_create(&peer->wait_event);
    if (peer->wait_event == NULL) {
        free(peer);
        return PEER_ERR_NO_MEM;
    }

    peer->options = *options;
    peer->ice_role = options->role == PEER_ROLE_SUBSCRIBER ?
        ESP_PEER_ROLE_CONTROLLED : ESP_PEER_ROLE_CONTROLLING;
    peer->state = CONNECTION_STATE_DISCONNECTED;

    // Set to invalid IDs to indicate that the data channels are not connected yet
    peer->reliable_stream_id = STREAM_ID_INVALID;
    peer->lossy_stream_id = STREAM_ID_INVALID;

     // Configuration for the default peer implementation
    esp_peer_default_cfg_t default_peer_cfg = {
        .data_ch_cfg = {
            .cache_timeout = 5000,
            .send_cache_size = 100 * 1024,
            .recv_cache_size = 100 * 1024
        }
    };
    esp_peer_media_dir_t audio_dir = get_media_direction(options->media->audio_dir, peer->options.role);
    esp_peer_media_dir_t video_dir = get_media_direction(options->media->video_dir, peer->options.role);
    ESP_LOGD(TAG(peer), "Audio dir: %d, Video dir: %d", audio_dir, video_dir);

    esp_peer_cfg_t peer_cfg = {
        .server_lists = options->server_list,
        .server_num = options->server_count,
        .ice_trans_policy = options->force_relay ?
            ESP_PEER_ICE_TRANS_POLICY_RELAY : ESP_PEER_ICE_TRANS_POLICY_ALL,
        .audio_dir = audio_dir,
        .video_dir = video_dir,
        .audio_info = options->media->audio_info,
        .video_info = options->media->video_info,
        .enable_data_channel = true,
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
    if (esp_peer_open(&peer_cfg, esp_peer_get_default_impl(), &peer->connection) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to open peer");
        media_lib_event_group_destroy(peer->wait_event);
        free(peer);
        return PEER_ERR_RTC;
    }

    // Set RTP transformer for publisher to inject audio level header extension
    if (options->role == PEER_ROLE_PUBLISHER &&
        options->media->audio_info.codec != ESP_PEER_AUDIO_CODEC_NONE) {
        int ret = esp_peer_set_rtp_transformer(
            peer->connection,
            ESP_PEER_RTP_TRANSFORM_ROLE_SENDER,
            &audio_level_transform_cb,
            peer
        );
        if (ret != ESP_PEER_ERR_NONE) {
            ESP_LOGW(TAG(peer), "Failed to set audio level RTP transformer: %d", ret);
        } else {
            ESP_LOGI(TAG(peer), "Audio level RTP transformer enabled");
        }
    }

    *handle = (peer_handle_t)peer;
    return PEER_ERR_NONE;
}

peer_err_t peer_destroy(peer_handle_t handle)
{
    if (handle == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;
    free(peer);
    return PEER_ERR_NONE;
}

peer_err_t peer_connect(peer_handle_t handle)
{
    if (handle == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;
#if CONFIG_LK_BENCHMARK
    peer->start_time = get_unix_time_ms();
#endif

    peer->running = true;
    media_lib_thread_handle_t thread;
    const char* thread_name = peer->options.role == PEER_ROLE_SUBSCRIBER ?
        "lk_peer_sub" : "lk_peer_pub";
    if (media_lib_thread_create_from_scheduler(&thread, thread_name, peer_task, peer) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to create thread");
        return PEER_ERR_RTC;
    }

    if (esp_peer_new_connection(peer->connection) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to start connection");
        return PEER_ERR_RTC;
    }
    return PEER_ERR_NONE;
}

peer_err_t peer_disconnect(peer_handle_t handle)
{
    if (handle == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;

    if (peer->connection != NULL) {
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
    return PEER_ERR_NONE;
}

peer_err_t peer_handle_sdp(peer_handle_t handle, const char *sdp)
{
    if (handle == NULL || sdp == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;

    esp_peer_msg_t msg = {
        .type = ESP_PEER_MSG_TYPE_SDP,
        .data = (void *)sdp,
        .size = (int)strlen(sdp)
    };
    if (esp_peer_send_msg(peer->connection, &msg) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to handle answer");
        return PEER_ERR_RTC;
    }
    return PEER_ERR_NONE;
}

peer_err_t peer_handle_ice_candidate(peer_handle_t handle, const char *candidate)
{
    if (handle == NULL || candidate == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;

    esp_peer_msg_t msg = {
        .type = ESP_PEER_MSG_TYPE_CANDIDATE,
        .data = (void *)candidate,
        .size = (int)strlen(candidate)
    };
    if (esp_peer_send_msg(peer->connection, &msg) != ESP_PEER_ERR_NONE) {
        ESP_LOGE(TAG(peer), "Failed to handle ICE candidate");
        return PEER_ERR_RTC;
    }
    return PEER_ERR_NONE;
}

peer_err_t peer_send_data_packet(peer_handle_t handle, const livekit_pb_data_packet_t* packet, bool reliable)
{
    if (handle == NULL || packet == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;

    uint16_t stream_id = reliable ?
        peer->reliable_stream_id : peer->lossy_stream_id;
    if (stream_id == STREAM_ID_INVALID) {
        ESP_LOGE(TAG(peer), "Required data channel not connected");
        return PEER_ERR_INVALID_STATE;
    }
    esp_peer_data_frame_t frame_info = {
        .type = ESP_PEER_DATA_CHANNEL_DATA,
        .stream_id = stream_id
    };

    size_t encoded_size = protocol_data_packet_encoded_size(packet);
    if (encoded_size == 0) {
        return PEER_ERR_MESSAGE;
    }
    uint8_t *enc_buf = (uint8_t *)malloc(encoded_size);
    if (enc_buf == NULL) {
        return PEER_ERR_NO_MEM;
    }
    int ret = PEER_ERR_NONE;
    do {
        if (!protocol_data_packet_encode(packet, enc_buf, encoded_size)) {
            ret = PEER_ERR_MESSAGE;
            break;
        }
        frame_info.data = enc_buf;
        frame_info.size = (int)encoded_size;
        if (esp_peer_send_data(peer->connection, &frame_info) != ESP_PEER_ERR_NONE) {
            ESP_LOGE(TAG(peer), "Data channel send failed");
            ret = PEER_ERR_RTC;
            break;
        }
    } while (0);

    free(enc_buf);
    return ret;
}

peer_err_t peer_send_audio(peer_handle_t handle, esp_peer_audio_frame_t* frame)
{
    if (handle == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;
    assert(peer->options.role == PEER_ROLE_PUBLISHER);

    esp_peer_send_audio(peer->connection, frame);
    return PEER_ERR_NONE;
}

peer_err_t peer_send_video(peer_handle_t handle, esp_peer_video_frame_t* frame)
{
    if (handle == NULL) {
        return PEER_ERR_INVALID_ARG;
    }
    peer_t *peer = (peer_t *)handle;
    assert(peer->options.role == PEER_ROLE_PUBLISHER);

    esp_peer_send_video(peer->connection, frame);
    return PEER_ERR_NONE;
}