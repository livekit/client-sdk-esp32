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

#pragma once

#include "livekit.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Handle a test data packet and echo supported topics back to the sender.
void interop_test_on_data_received(const livekit_data_received_t *data, void *ctx);

/// Register test-only text and byte stream echo handlers.
livekit_err_t interop_test_start(livekit_room_handle_t room);

/// Unregister test-only handlers before destroying the room.
void interop_test_stop(void);

#ifdef __cplusplus
}
#endif
