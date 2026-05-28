/*
 * Copyright 2026 LiveKit, Inc.
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

// Build a real DataPacket wire-format buffer containing a stream_header
// with an attributes map, then verify protocol_data_packet_extract_attributes
// (the manual second-pass parser used because nanopb's auto-allocated
// submessages cannot have decode callbacks installed externally) recovers
// every key/value pair correctly.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pb_encode.h"
#include "livekit_models.pb.h"
#include "livekit_data_stream.h"
#include "protocol.h"

#define DIE(fmt, ...) do { \
    fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__); \
    return 1; \
} while (0)

#define DIE_VOID(fmt, ...) do { \
    fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__); \
    exit(1); \
} while (0)

typedef struct {
    const char **keys;
    const char **vals;
    size_t count;
} attrs_ctx_t;

static bool encode_string_field(pb_ostream_t *stream, const pb_field_t *field,
                                void *const *arg)
{
    const char *str = (const char *)*arg;
    if (str == NULL) {
        return true;
    }
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }
    return pb_encode_string(stream, (const uint8_t *)str, strlen(str));
}

static bool encode_attrs(pb_ostream_t *stream, const pb_field_t *field,
                         void *const *arg)
{
    attrs_ctx_t *ctx = (attrs_ctx_t *)*arg;
    for (size_t i = 0; i < ctx->count; i++) {
        livekit_pb_data_stream_header_attributes_entry_t entry =
            LIVEKIT_PB_DATA_STREAM_HEADER_ATTRIBUTES_ENTRY_INIT_ZERO;
        entry.key.funcs.encode = encode_string_field;
        entry.key.arg = (void *)ctx->keys[i];
        entry.value.funcs.encode = encode_string_field;
        entry.value.arg = (void *)ctx->vals[i];
        if (!pb_encode_tag_for_field(stream, field)) {
            return false;
        }
        if (!pb_encode_submessage(stream,
                                  &livekit_pb_data_stream_header_attributes_entry_t_msg,
                                  &entry)) {
            return false;
        }
    }
    return true;
}

static size_t build_packet_with_attrs(uint8_t *buf, size_t cap,
                                      const char **keys, const char **vals,
                                      size_t count)
{
    attrs_ctx_t ctx = { .keys = keys, .vals = vals, .count = count };

    livekit_pb_data_stream_header_t header = LIVEKIT_PB_DATA_STREAM_HEADER_INIT_ZERO;
    strncpy(header.stream_id, "00000000-0000-0000-0000-000000000001",
            sizeof(header.stream_id) - 1);
    header.attributes.funcs.encode = encode_attrs;
    header.attributes.arg = &ctx;
    header.which_content_header = LIVEKIT_PB_DATA_STREAM_HEADER_TEXT_HEADER_TAG;

    livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
    packet.which_value = LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG;
    packet.value.stream_header = &header;

    pb_ostream_t stream = pb_ostream_from_buffer(buf, cap);
    if (!pb_encode(&stream, LIVEKIT_PB_DATA_PACKET_FIELDS, &packet)) {
        DIE_VOID("encode failed: %s", stream.errmsg ? stream.errmsg : "(no msg)");
    }
    return stream.bytes_written;
}

int main(void)
{
    // Case 1: three plain attributes.
    {
        const char *keys[] = { "lk.rpc_request_id", "lk.rpc_request_method", "lk.rpc_request_version" };
        const char *vals[] = { "11111111-2222-3333-4444-555555555555", "echo", "2" };
        uint8_t buf[2048];
        size_t len = build_packet_with_attrs(buf, sizeof(buf), keys, vals, 3);

        livekit_data_stream_attribute_t *items = NULL;
        size_t count = 0;
        if (!protocol_data_packet_extract_attributes(
                buf, len,
                LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG,
                LIVEKIT_PB_DATA_STREAM_HEADER_ATTRIBUTES_TAG,
                &items, &count)) {
            DIE("extract_attributes returned false");
        }
        if (count != 3) {
            DIE("expected 3 attributes, got %zu", count);
        }
        for (size_t i = 0; i < 3; i++) {
            if (strcmp(items[i].key, keys[i]) != 0) {
                DIE("attr[%zu].key: expected '%s', got '%s'", i, keys[i], items[i].key);
            }
            if (strcmp(items[i].value, vals[i]) != 0) {
                DIE("attr[%zu].value: expected '%s', got '%s'", i, vals[i], items[i].value);
            }
        }
        protocol_data_packet_attributes_free(items, count);
        printf("PASS: three attributes round-tripped\n");
    }

    // Case 2: no attributes at all.
    {
        const char *keys[] = { NULL };
        const char *vals[] = { NULL };
        (void)keys; (void)vals;
        uint8_t buf[256];
        // Encode a header without invoking the attributes callback by
        // leaving funcs.encode NULL.
        livekit_pb_data_stream_header_t header = LIVEKIT_PB_DATA_STREAM_HEADER_INIT_ZERO;
        strncpy(header.stream_id, "00000000-0000-0000-0000-000000000002",
                sizeof(header.stream_id) - 1);
        header.which_content_header = LIVEKIT_PB_DATA_STREAM_HEADER_TEXT_HEADER_TAG;
        livekit_pb_data_packet_t packet = LIVEKIT_PB_DATA_PACKET_INIT_ZERO;
        packet.which_value = LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG;
        packet.value.stream_header = &header;
        pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
        if (!pb_encode(&stream, LIVEKIT_PB_DATA_PACKET_FIELDS, &packet)) {
            DIE("encode (no attrs) failed: %s",
                stream.errmsg ? stream.errmsg : "(no msg)");
        }

        livekit_data_stream_attribute_t *items = NULL;
        size_t count = 0;
        if (!protocol_data_packet_extract_attributes(
                buf, stream.bytes_written,
                LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG,
                LIVEKIT_PB_DATA_STREAM_HEADER_ATTRIBUTES_TAG,
                &items, &count)) {
            DIE("extract_attributes returned false for empty attrs");
        }
        if (count != 0 || items != NULL) {
            DIE("expected empty result, got count=%zu items=%p",
                count, (void *)items);
        }
        printf("PASS: empty attributes\n");
    }

    // Case 3: one attribute with a long value.
    {
        char long_val[1024];
        memset(long_val, 'x', sizeof(long_val) - 1);
        long_val[sizeof(long_val) - 1] = '\0';
        const char *keys[] = { "blob" };
        const char *vals[] = { long_val };
        uint8_t buf[4096];
        size_t len = build_packet_with_attrs(buf, sizeof(buf), keys, vals, 1);

        livekit_data_stream_attribute_t *items = NULL;
        size_t count = 0;
        if (!protocol_data_packet_extract_attributes(
                buf, len,
                LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG,
                LIVEKIT_PB_DATA_STREAM_HEADER_ATTRIBUTES_TAG,
                &items, &count)) {
            DIE("extract_attributes returned false for long value");
        }
        if (count != 1) {
            DIE("expected 1 attribute, got %zu", count);
        }
        if (strcmp(items[0].key, "blob") != 0) {
            DIE("long-attr key: expected 'blob', got '%s'", items[0].key);
        }
        if (strcmp(items[0].value, long_val) != 0) {
            DIE("long-attr value mismatch");
        }
        protocol_data_packet_attributes_free(items, count);
        printf("PASS: long value round-tripped\n");
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
