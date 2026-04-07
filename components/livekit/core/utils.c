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

#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>
#include "esp_random.h"

#include "utils.h"

#define MAX_BACKOFF_MS 7000

int64_t get_unix_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
}

uint16_t backoff_ms_for_attempt(uint16_t attempt)
{
    if (attempt == 0) return 0;
    uint16_t base = 100 << attempt; // 100 * (2^attempt)
    uint16_t rand = (uint16_t)(esp_random() % 1001); // range [0, 1000]
    uint16_t total = base + rand;
    return total > MAX_BACKOFF_MS ? MAX_BACKOFF_MS : total;
}

void generate_uuid(char out[37])
{
    uint32_t r0 = esp_random();
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();

    // Set version 4 (bits 12-15 of time_hi_and_version)
    r1 = (r1 & 0xFFFF0FFF) | 0x00004000;
    // Set variant 1 (bits 6-7 of clk_seq_hi_res)
    r2 = (r2 & 0x3FFFFFFF) | 0x80000000;

    snprintf(out, 37, "%08x-%04x-%04x-%04x-%04x%08x",
             r0,
             (r1 >> 16) & 0xFFFF,
             r1 & 0xFFFF,
             (r2 >> 16) & 0xFFFF,
             r2 & 0xFFFF,
             r3);
}