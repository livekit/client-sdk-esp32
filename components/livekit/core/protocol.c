#include "esp_log.h"
#include "cJSON.h"
#include "protocol.h"

static const char *TAG = "livekit_protocol";

bool protocol_signal_res_decode(const char *buf, size_t len, livekit_pb_signal_response_t* out)
{
    pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)buf, len);
    if (!pb_decode(&stream, LIVEKIT_PB_SIGNAL_RESPONSE_FIELDS, out)) {
        ESP_LOGE(TAG, "Failed to decode signal res: %s", stream.errmsg);
        return false;
    }
    return true;
}

void protocol_signal_res_free(livekit_pb_signal_response_t *res)
{
    pb_release(LIVEKIT_PB_SIGNAL_RESPONSE_FIELDS, res);
}

bool protocol_signal_trickle_get_candidate(const livekit_pb_trickle_request_t *trickle, char **candidate_out)
{
    if (trickle == NULL || candidate_out == NULL) {
        return false;
    }
    if (trickle->candidate_init == NULL) {
        ESP_LOGE(TAG, "candidate_init is NULL");
        return false;
    }

    bool ret = false;
    cJSON *candidate_init = NULL;
    do {
        candidate_init = cJSON_Parse(trickle->candidate_init);
        if (candidate_init == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                ESP_LOGE(TAG, "Failed to parse candidate_init: %s", error_ptr);
            }
            break;
        }
        cJSON *candidate = cJSON_GetObjectItemCaseSensitive(candidate_init, "candidate");
        if (!cJSON_IsString(candidate) || (candidate->valuestring == NULL)) {
            ESP_LOGE(TAG, "Missing candidate key in candidate_init");
            break;
        }
        *candidate_out = strdup(candidate->valuestring);
        if (*candidate_out == NULL) {
            break;
        }
        ret = true;
    } while(0);

    cJSON_Delete(candidate_init);
    return ret;
}