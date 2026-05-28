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

#pragma once

#include "livekit_rtc.pb.h"
#include "livekit_models.pb.h"
#include "livekit_metrics.pb.h"
#include "timestamp.pb.h"
#include "livekit_data_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Server identifier (SID) type.
typedef char livekit_pb_sid_t[16];

// MARK: - Data packet

/// Decodes a data packet.
///
/// When the packet is no longer needed, free using `protocol_data_packet_free`.
///
bool protocol_data_packet_decode(const uint8_t *buf, size_t len, livekit_pb_data_packet_t *out);

/// Frees a data packet.
void protocol_data_packet_free(livekit_pb_data_packet_t *packet);

/// Returns the encoded size of a data packet.
///
/// @returns The encoded size of the packet or 0 if the encoded size cannot be determined.
///
size_t protocol_data_packet_encoded_size(const livekit_pb_data_packet_t *packet);

/// Encodes a data packet into the provided buffer.
bool protocol_data_packet_encode(const livekit_pb_data_packet_t *packet, uint8_t *dest, size_t encoded_size);

/// Extract the attribute map from a DataStream.Header or DataStream.Trailer
/// nested inside a DataPacket. Walks the raw DataPacket wire bytes since
/// nanopb's auto-allocated submessages can't have decode callbacks set
/// externally.
///
/// @param buf            Raw DataPacket bytes.
/// @param len            Length of @p buf.
/// @param submsg_tag     DataPacket field tag to descend into
///                       (LIVEKIT_PB_DATA_PACKET_STREAM_HEADER_TAG = 13 or
///                        LIVEKIT_PB_DATA_PACKET_STREAM_TRAILER_TAG = 15).
/// @param attrs_tag      Attributes field tag inside that submessage (8 for
///                       Header, 3 for Trailer).
/// @param out_items[out] Set to a malloc'd array of attribute entries; key
///                       and value strings are independently malloc'd. NULL
///                       if no attributes were present.
/// @param out_count[out] Number of entries in @p out_items.
/// @return true on success (including when no attributes are present);
///         false on malformed input or allocation failure.
bool protocol_data_packet_extract_attributes(
    const uint8_t *buf, size_t len,
    uint32_t submsg_tag, uint32_t attrs_tag,
    livekit_data_stream_attribute_t **out_items, size_t *out_count);

/// Free the array returned by @ref protocol_data_packet_extract_attributes.
void protocol_data_packet_attributes_free(
    livekit_data_stream_attribute_t *items, size_t count);

// MARK: - Signal response

/// Decodes a signal response.
///
/// When the response is no longer needed, free using `protocol_signal_response_free`.
///
bool protocol_signal_response_decode(const uint8_t *buf, size_t len, livekit_pb_signal_response_t* out);

/// Frees a signal response.
void protocol_signal_response_free(livekit_pb_signal_response_t *res);

/// Extract ICE candidate string from a trickle request.
///
/// The caller is responsible for freeing the candidate string if it is not NULL.
///
bool protocol_signal_trickle_get_candidate(
    const livekit_pb_trickle_request_t *trickle,
    char **candidate_out
);

// MARK: - Signal request

/// Returns the encoded size of a signal request.
///
/// @returns The encoded size of the request or 0 if the encoded size cannot be determined.
///
size_t protocol_signal_request_encoded_size(const livekit_pb_signal_request_t *req);

/// Encodes a signal request into the provided buffer.
bool protocol_signal_request_encode(const livekit_pb_signal_request_t *req, uint8_t *dest, size_t encoded_size);

#ifdef __cplusplus
}
#endif