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

// Minimal cJSON header for the host test build. protocol.c uses cJSON
// only in signal-channel helpers that the extract_attributes test
// never reaches; these declarations exist solely so protocol.c compiles
// and the linker resolves against host_shims/cJSON_stubs.c.

#pragma once

typedef struct cJSON {
    char *valuestring;
} cJSON;

cJSON *cJSON_Parse(const char *value);
const char *cJSON_GetErrorPtr(void);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string);
int cJSON_IsString(const cJSON *item);
void cJSON_Delete(cJSON *item);
