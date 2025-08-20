
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

/// Decodes a signal response.
bool protocol_signal_res_decode(const char *buf, size_t len, livekit_pb_signal_response_t* out);

/// Frees a signal response.
///
/// Always use this to discard signal responses, even for messages types that do not have
/// dynamic fields as they may be added in the future.
///
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