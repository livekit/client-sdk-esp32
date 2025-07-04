#include <stdlib.h>
#include <math.h>
#include <esp_log.h>
#include "livekit_demo.h"
#include "media_setup.h"
#include "codec_init.h"
#include "board.h"
#include "cJSON.h"

static const char *TAG = "livekit_demo";

static livekit_room_handle_t room_handle;

/// @brief Invoked by a remote participant to set the state of an on-board LED.
static void set_led_state(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    cJSON *root = cJSON_Parse(invocation->payload);
    if (!root) {
        livekit_rpc_return_error("Invalid JSON");
        return;
    }
    const cJSON *color_entry = cJSON_GetObjectItemCaseSensitive(root, "color");
    const cJSON *state_entry = cJSON_GetObjectItemCaseSensitive(root, "state");
    if (!cJSON_IsString(color_entry) || !cJSON_IsBool(state_entry)) {
        livekit_rpc_return_error("Unexpected JSON format");
        cJSON_Delete(root);
        return;
    }

    const char *color = color_entry->valuestring;
    bool state = cJSON_IsTrue(state_entry);

    board_led_t led;
    if (strncmp(color, "red", 3) == 0) {
        led = BOARD_LED_RED;
    } else if (strncmp(color, "blue", 4) == 0) {
        led = BOARD_LED_BLUE;
    } else {
        livekit_rpc_return_error("Unsupported color");
        cJSON_Delete(root);
        return;
    }
    if (board_led_set(led, state) != 0) {
        livekit_rpc_return_error("Failed to set LED state");
        cJSON_Delete(root);
        return;
    }

    livekit_rpc_return_ok(NULL);
    cJSON_Delete(root);
}

/// @brief Invoked by a remote participant to get the current CPU temperature.
static void get_cpu_temp(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    float temp = board_get_temp();
    char temp_string[16];
    snprintf(temp_string, sizeof(temp_string), "%.2f", temp);
    livekit_rpc_return_ok(temp_string);
}

int join_room()
{
    livekit_room_options_t room_options = {
        .publish = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .audio_encode = {
                .codec = LIVEKIT_AUDIO_CODEC_OPUS,
                .sample_rate = 16000,
                .channel_count = 1
            },
            .capturer = media_setup_get_capturer()
        },
        .subscribe = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .renderer = media_setup_get_renderer()
        }
    };

    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return -1;
    }
    if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
        return -1;
    }

    livekit_room_rpc_register(room_handle, "set_led_state", set_led_state);
    livekit_room_rpc_register(room_handle, "get_cpu_temp", get_cpu_temp);

    livekit_err_t connect_res;
#ifdef LK_SANDBOX_ID
    // Option A: Sandbox token server.
    livekit_sandbox_res_t res = {};
    livekit_sandbox_options_t gen_options = {
        .sandbox_id = LK_SANDBOX_ID,
        .room_name = LK_SANDBOX_ROOM_NAME,
        .participant_name = LK_SANDBOX_PARTICIPANT_NAME
    };
    if (!livekit_sandbox_generate(&gen_options, &res)) {
        ESP_LOGE(TAG, "Failed to generate sandbox token");
        return -1;
    }
    connect_res = livekit_room_connect(room_handle, res.server_url, res.token);
    livekit_sandbox_res_free(&res);
#else
    // Option B: Pre-generated token.
    connect_res = livekit_room_connect(room_handle, LK_SERVER_URL, LK_TOKEN);
#endif

    if (connect_res != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect to room");
        return -1;
    }
    return 0;
}

int leave_room()
{
    if (room_handle == NULL) {
        ESP_LOGE(TAG, "Room not created");
        return -1;
    }
    if (livekit_room_close(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to leave room");
        return -1;
    }
    if (livekit_room_destroy(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return -1;
    }
    room_handle = NULL;
    return 0;
}