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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Prints `Enter <name>` and reads one line from the console into `buf`.
void read_param(const char *name, char *buf, size_t len);

/// Connects to Wi-Fi or Ethernet (chosen by Kconfig), failing the test on error.
/// The credentials are ignored on Ethernet builds.
void network_connect(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
