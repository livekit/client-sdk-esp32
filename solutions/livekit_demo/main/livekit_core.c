#include "livekit_core.h"

const char *LK_TAG = "livekit";

void livekit_system_init(void)
{
    static bool sntp_synced = false;
    if (sntp_synced == false) {
        if (0 == webrtc_utils_time_sync_init()) {
            sntp_synced = true;
        }
    }
    // TODO: Initialize media system, etc.
}