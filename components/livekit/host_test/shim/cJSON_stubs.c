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

// Stub implementations of cJSON entry points used by protocol.c. None
// of the host tests reach the signal-channel code paths that would
// actually call into cJSON; these stubs exist purely so the linker
// is satisfied.

#include "cJSON.h"

cJSON *cJSON_Parse(const char *value)
{
    (void)value;
    return (void *)0;
}

const char *cJSON_GetErrorPtr(void)
{
    return (void *)0;
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string)
{
    (void)object;
    (void)string;
    return (void *)0;
}

int cJSON_IsString(const cJSON *item)
{
    (void)item;
    return 0;
}

void cJSON_Delete(cJSON *item)
{
    (void)item;
}
