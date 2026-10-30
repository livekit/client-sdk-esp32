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
// cspell:words AGGR
#include <stdint.h>
/// Minimal API double; this does not simulate acoustic cancellation.
typedef struct { int placeholder; } aec_handle_t;
typedef struct {
    int mic_num, ref_num, out_num, filter_length, sample_rate;
    uint32_t caps;
    int mode, nlp_level;
} aec_config_t;
#define AEC_MODE_FD_HIGH_PERF 6
#define AEC_MODE_VOIP_HIGH_PERF 4
#define AEC_NLP_LEVEL_AGGR 2
aec_handle_t *aec_create_from_config(aec_config_t *cfg);
int aec_get_chunksize(const aec_handle_t *handle);
void aec_process(const aec_handle_t *handle, int16_t *mic, int16_t *ref, int16_t *out);
void aec_destroy(aec_handle_t *handle);
