#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_log.h>
#include <esp_peer.h>
#include <esp_webrtc.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "esp_webrtc_defaults.h"
#include "esp_peer_default.h"
#include "webrtc_utils_time.h"

#include "protocol/livekit_models.pb.h"
#include "protocol/livekit_metrics.pb.h"
#include "protocol/livekit_rtc.pb.h"

#include "livekit_types.h"
#include "settings.h"

/// @brief Tag for logging
extern const char *LK_TAG;

#define LIVEKIT_PB_ENCODE_MAX_SIZE 2048
#define LIVEKIT_PB_DECODE_MAX_SIZE 2048

/// @brief Internal room state
typedef struct {
    char* signaling_url;
    esp_webrtc_handle_t rtc_handle;
    livekit_event_handler_t event_handler;
    // TODO: Add additional fields
} livekit_room_state_t;

/// @brief Perform one time initialization when creating the first room
void livekit_system_init(void);

#ifdef __cplusplus
}
#endif