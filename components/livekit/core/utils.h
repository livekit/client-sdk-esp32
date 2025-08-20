
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
