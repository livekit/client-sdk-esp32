#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "livekit_core.h"

int livekit_rtc_data_handler(esp_webrtc_custom_data_via_t via, uint8_t *data, int size, void *ctx);
int livekit_rtc_event_handler(esp_webrtc_event_t *event, void *ctx);

#ifdef __cplusplus
}
#endif
