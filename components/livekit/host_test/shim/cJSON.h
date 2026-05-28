/*
 * Minimal cJSON header for the host test build. protocol.c uses cJSON
 * only in signal-channel helpers that the extract_attributes test
 * never reaches; these declarations exist solely so protocol.c compiles
 * and the linker resolves against host_shims/cJSON_stubs.c.
 */
#pragma once

typedef struct cJSON {
    char *valuestring;
} cJSON;

cJSON *cJSON_Parse(const char *value);
const char *cJSON_GetErrorPtr(void);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string);
int cJSON_IsString(const cJSON *item);
void cJSON_Delete(cJSON *item);
