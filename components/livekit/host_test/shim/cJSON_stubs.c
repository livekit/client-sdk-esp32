/*
 * Stub implementations of cJSON entry points used by protocol.c. None
 * of the host tests reach the signal-channel code paths that would
 * actually call into cJSON; these stubs exist purely so the linker
 * is satisfied.
 */
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
