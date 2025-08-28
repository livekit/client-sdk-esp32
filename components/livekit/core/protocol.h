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