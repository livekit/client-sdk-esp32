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

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Options for building a signaling URL.
typedef struct {
    const char *server_url;
} url_build_options;

/// Constructs a signaling URL.
///
/// @param options The options for building the URL.
/// @param out_url[out] The output URL.
///
/// @return True if the URL is constructed successfully, false otherwise.
/// @note The caller is responsible for freeing the output URL.
///
bool url_build(const url_build_options *options, char **out_url);

#ifdef __cplusplus
}
#endif
