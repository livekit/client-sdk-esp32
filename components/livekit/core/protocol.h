
#pragma once

#include "pb_encode.h"
#include "pb_decode.h"

#include "livekit_rtc.pb.h"
#include "livekit_models.pb.h"
#include "livekit_metrics.pb.h"
#include "timestamp.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Server identifier (SID) type.
typedef char livekit_pb_sid_t[16];

/// Decodes a data packet.
///
/// When the packet is no longer needed, free using `protocol_data_packet_free`.
///
bool protocol_data_packet_decode(const uint8_t *buf, size_t len, livekit_pb_data_packet_t *out);

/// Frees a data packet.
void protocol_data_packet_free(livekit_pb_data_packet_t *packet);

/// Decodes a signal response.
///
/// When the response is no longer needed, free using `protocol_signal_res_free`.
///
bool protocol_signal_res_decode(const uint8_t *buf, size_t len, livekit_pb_signal_response_t* out);

/// Frees a signal response.
void protocol_signal_res_free(livekit_pb_signal_response_t *res);

/// Extract ICE candidate string from a trickle request.
///
/// The caller is responsible for freeing the candidate string if it is not NULL.
///
bool protocol_signal_trickle_get_candidate(
    const livekit_pb_trickle_request_t *trickle,
    char **candidate_out
);

#ifdef __cplusplus
}
#endif