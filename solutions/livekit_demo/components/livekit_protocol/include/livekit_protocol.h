
#pragma once

#include <pb_encode.h>
#include <pb_decode.h>

#include "livekit_rtc.pb.h"
#include "livekit_models.pb.h"
#include "livekit_metrics.pb.h"
#include "timestamp.pb.h"

/// @brief Gets the name of the signaling response type.
const char* livekit_protocol_sig_res_name(pb_size_t which_message);