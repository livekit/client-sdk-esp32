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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

#include "protocol.h"

static const char *TAG = "livekit_protocol";

static int32_t decode_first_tag(const pb_byte_t *buf, size_t len)
{
    pb_istream_t stream = pb_istream_from_buffer(buf, len);
    pb_wire_type_t wire_type;
    uint32_t tag;
    bool eof = false;
    if (!pb_decode_tag(&stream, &wire_type, &tag, &eof) || eof) {
        return -1;
    }
    return (int32_t)tag;
}

// MARK: - Data packet

__attribute__((always_inline))
inline bool protocol_data_packet_decode(const uint8_t *buf, size_t len, livekit_pb_data_packet_t *out)
{
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)buf, len);
    if (!pb_decode(&stream, LIVEKIT_PB_DATA_PACKET_FIELDS, out)) {
        ESP_LOGE(TAG, "Failed to decode data packet: type=%" PRId32 ", error=%s",
            decode_first_tag(buf, len), stream.errmsg);
        return false;
    }
    return true;
}

__attribute__((always_inline))
inline void protocol_data_packet_free(livekit_pb_data_packet_t *packet)
{
    pb_release(LIVEKIT_PB_DATA_PACKET_FIELDS, packet);
}

__attribute__((always_inline))
inline size_t protocol_data_packet_encoded_size(const livekit_pb_data_packet_t *packet)
{
    size_t encoded_size = 0;
    if (!pb_get_encoded_size(&encoded_size, LIVEKIT_PB_DATA_PACKET_FIELDS, packet)) {
        return 0;
    }
    return encoded_size;
}

__attribute__((always_inline))
inline bool protocol_data_packet_encode(const livekit_pb_data_packet_t *packet, uint8_t *dest, size_t encoded_size)
{
    pb_ostream_t stream = pb_ostream_from_buffer((pb_byte_t *)dest, encoded_size);
    if (!pb_encode(&stream, LIVEKIT_PB_DATA_PACKET_FIELDS, packet)) {
        ESP_LOGE(TAG, "Failed to encode data packet: type=%" PRIu16 ", error=%s",
            packet->which_value, stream.errmsg);
        return false;
    }
    return stream.bytes_written == encoded_size;
}

void protocol_data_packet_attributes_free(
    livekit_data_stream_attribute_t *items, size_t count)
{
    if (items == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free((void *)items[i].key);
        free((void *)items[i].value);
    }
    free(items);
}

/// Read a length-delimited string field's value (assumes the tag has already
/// been consumed) into a freshly allocated NUL-terminated buffer.
static bool read_string_value(pb_istream_t *stream, char **out)
{
    uint32_t slen = 0;
    if (!pb_decode_varint32(stream, &slen)) {
        return false;
    }
    char *buf = malloc((size_t)slen + 1);
    if (buf == NULL) {
        return false;
    }
    if (!pb_read(stream, (pb_byte_t *)buf, slen)) {
        free(buf);
        return false;
    }
    buf[slen] = '\0';
    free(*out); // tolerate duplicate-tag (last one wins)
    *out = buf;
    return true;
}

static bool decode_attribute_entry(pb_istream_t *entry_stream,
                                   livekit_data_stream_attribute_t *out)
{
    char *key = NULL;
    char *value = NULL;
    while (entry_stream->bytes_left > 0) {
        uint32_t tag = 0;
        pb_wire_type_t wt;
        bool eof = false;
        if (!pb_decode_tag(entry_stream, &wt, &tag, &eof)) {
            free(key);
            free(value);
            return false;
        }
        if (eof) {
            break;
        }
        if (tag == 1 && wt == PB_WT_STRING) {
            if (!read_string_value(entry_stream, &key)) {
                free(key);
                free(value);
                return false;
            }
        } else if (tag == 2 && wt == PB_WT_STRING) {
            if (!read_string_value(entry_stream, &value)) {
                free(key);
                free(value);
                return false;
            }
        } else if (!pb_skip_field(entry_stream, wt)) {
            free(key);
            free(value);
            return false;
        }
    }
    if (key == NULL || value == NULL) {
        // A malformed entry: drop it, but don't fail the whole decode.
        free(key);
        free(value);
        out->key = NULL;
        out->value = NULL;
        return true;
    }
    out->key = key;
    out->value = value;
    return true;
}

bool protocol_data_packet_extract_attributes(
    const uint8_t *buf, size_t len,
    uint32_t submsg_tag, uint32_t attrs_tag,
    livekit_data_stream_attribute_t **out_items, size_t *out_count)
{
    *out_items = NULL;
    *out_count = 0;
    if (buf == NULL || len == 0) {
        return true;
    }

    pb_istream_t outer = pb_istream_from_buffer((const pb_byte_t *)buf, len);
    while (outer.bytes_left > 0) {
        uint32_t tag = 0;
        pb_wire_type_t wt;
        bool eof = false;
        if (!pb_decode_tag(&outer, &wt, &tag, &eof)) {
            return false;
        }
        if (eof) {
            break;
        }
        if (tag != submsg_tag || wt != PB_WT_STRING) {
            if (!pb_skip_field(&outer, wt)) {
                return false;
            }
            continue;
        }
        pb_istream_t inner;
        if (!pb_make_string_substream(&outer, &inner)) {
            return false;
        }
        livekit_data_stream_attribute_t *list = NULL;
        size_t list_count = 0;
        size_t list_cap = 0;
        bool ok = true;
        while (inner.bytes_left > 0) {
            uint32_t itag = 0;
            pb_wire_type_t iwt;
            bool ieof = false;
            if (!pb_decode_tag(&inner, &iwt, &itag, &ieof)) {
                ok = false;
                break;
            }
            if (ieof) {
                break;
            }
            if (itag != attrs_tag || iwt != PB_WT_STRING) {
                if (!pb_skip_field(&inner, iwt)) {
                    ok = false;
                    break;
                }
                continue;
            }
            pb_istream_t entry_stream;
            if (!pb_make_string_substream(&inner, &entry_stream)) {
                ok = false;
                break;
            }
            if (list_count == list_cap) {
                size_t new_cap = list_cap == 0 ? 4 : list_cap * 2;
                livekit_data_stream_attribute_t *grown =
                    realloc(list, new_cap * sizeof(*list));
                if (grown == NULL) {
                    ok = false;
                    break;
                }
                list = grown;
                list_cap = new_cap;
            }
            list[list_count].key = NULL;
            list[list_count].value = NULL;
            if (!decode_attribute_entry(&entry_stream, &list[list_count])) {
                ok = false;
                break;
            }
            if (!pb_close_string_substream(&inner, &entry_stream)) {
                ok = false;
                break;
            }
            if (list[list_count].key != NULL && list[list_count].value != NULL) {
                list_count++;
            }
        }
        if (!ok) {
            protocol_data_packet_attributes_free(list, list_count);
            return false;
        }
        if (!pb_close_string_substream(&outer, &inner)) {
            protocol_data_packet_attributes_free(list, list_count);
            return false;
        }
        // Only the first occurrence of the submsg field is honored. (For
        // DataPacket the stream_header/stream_trailer fields are SINGULAR
        // members of the oneof, so this is the only one anyway.)
        *out_items = list;
        *out_count = list_count;
        return true;
    }
    return true;
}

// MARK: - Signal response

__attribute__((always_inline))
inline bool protocol_signal_response_decode(const uint8_t *buf, size_t len, livekit_pb_signal_response_t* out)
{
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)buf, len);
    if (!pb_decode(&stream, LIVEKIT_PB_SIGNAL_RESPONSE_FIELDS, out)) {
        ESP_LOGE(TAG, "Failed to decode signal res: type=%" PRId32 ", error=%s",
            decode_first_tag(buf, len), stream.errmsg);
        return false;
    }
    return true;
}

__attribute__((always_inline))
inline void protocol_signal_response_free(livekit_pb_signal_response_t *res)
{
    pb_release(LIVEKIT_PB_SIGNAL_RESPONSE_FIELDS, res);
}

bool protocol_signal_trickle_get_candidate(const livekit_pb_trickle_request_t *trickle, char **candidate_out)
{
    if (trickle == NULL || candidate_out == NULL) {
        return false;
    }
    if (trickle->candidate_init == NULL) {
        ESP_LOGE(TAG, "candidate_init is NULL");
        return false;
    }

    bool ret = false;
    cJSON *candidate_init = NULL;
    do {
        candidate_init = cJSON_Parse(trickle->candidate_init);
        if (candidate_init == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                ESP_LOGE(TAG, "Failed to parse candidate_init: %s", error_ptr);
            }
            break;
        }
        cJSON *candidate = cJSON_GetObjectItemCaseSensitive(candidate_init, "candidate");
        if (!cJSON_IsString(candidate) || (candidate->valuestring == NULL)) {
            ESP_LOGE(TAG, "Missing candidate key in candidate_init");
            break;
        }
        *candidate_out = strdup(candidate->valuestring);
        if (*candidate_out == NULL) {
            break;
        }
        ret = true;
    } while(0);

    cJSON_Delete(candidate_init);
    return ret;
}

// MARK: - Signal request

__attribute__((always_inline))
inline size_t protocol_signal_request_encoded_size(const livekit_pb_signal_request_t *req)
{
    size_t encoded_size = 0;
    if (!pb_get_encoded_size(&encoded_size, LIVEKIT_PB_SIGNAL_REQUEST_FIELDS, req)) {
        return 0;
    }
    return encoded_size;
}

__attribute__((always_inline))
inline bool protocol_signal_request_encode(const livekit_pb_signal_request_t *req, uint8_t *dest, size_t encoded_size)
{
    pb_ostream_t stream = pb_ostream_from_buffer((pb_byte_t *)dest, encoded_size);
    if (!pb_encode(&stream, LIVEKIT_PB_SIGNAL_REQUEST_FIELDS, req)) {
        ESP_LOGE(TAG, "Failed to encode signal req: type=%" PRIu16 ", error=%s",
            req->which_message, stream.errmsg);
        return false;
    }
    return stream.bytes_written == encoded_size;
}
