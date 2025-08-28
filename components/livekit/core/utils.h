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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAFE_FREE(ptr) if (ptr != NULL) { free(ptr); ptr = NULL; }

int64_t get_unix_time_ms(void);

/// Returns the backoff time in milliseconds for the given attempt number.
///
/// Uses an exponential function with a random jitter to calculate the backoff time
/// with the value limited to an upper bound.
///
uint16_t backoff_ms_for_attempt(uint16_t attempt);

#ifdef __cplusplus
}
#endif
