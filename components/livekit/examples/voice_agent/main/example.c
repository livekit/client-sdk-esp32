#include "esp_log.h"
#include "cJSON.h"
#include "livekit.h"
#include "livekit_sandbox.h"
#include "media.h"
#include "board.h"
#include "example.h"
#if CONFIG_LK_EXAMPLE_ENABLE_INTEROP_TEST
#include "interop_test.h"
#endif

static const char *TAG = "livekit_example";

static livekit_room_handle_t room_handle;
static bool agent_joined = false;

/// Invoked when the room's connection state changes.
static void on_state_changed(livekit_connection_state_t state, void* ctx)
{
    ESP_LOGI(TAG, "Room state changed: %s", livekit_connection_state_str(state));

    livekit_failure_reason_t reason = livekit_room_get_failure_reason(room_handle);
    if (reason != LIVEKIT_FAILURE_REASON_NONE) {
        ESP_LOGE(TAG, "Failure reason: %s", livekit_failure_reason_str(reason));
    }
}

/// Invoked when participant information is received.
static void on_participant_info(const livekit_participant_info_t* info, void* ctx)
{
    if (info->kind != LIVEKIT_PARTICIPANT_KIND_AGENT) {
        // Only handle agent participants for this example.
        return;
    }
    bool joined = false;
    switch (info->state) {
        case LIVEKIT_PARTICIPANT_STATE_ACTIVE:       joined = true; break;
        case LIVEKIT_PARTICIPANT_STATE_DISCONNECTED: joined = false; break;
        default: return;
    }
    if (joined != agent_joined) {
        ESP_LOGI(TAG, "Agent has %s the room", joined ? "joined" : "left");
        agent_joined = joined;
    }
}

/// Invoked by a remote participant to set the state of an on-board LED.
static void set_led_state(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    if (invocation->payload == NULL) {
        livekit_rpc_return_error("Missing payload");
        return;
    }
    cJSON *root = cJSON_Parse(invocation->payload);
    if (!root) {
        livekit_rpc_return_error("Invalid JSON");
        return;
    }

    char* error = NULL;
    do {
        const cJSON *color_entry = cJSON_GetObjectItemCaseSensitive(root, "color");
        const cJSON *state_entry = cJSON_GetObjectItemCaseSensitive(root, "state");
        if (!cJSON_IsString(color_entry) || !cJSON_IsBool(state_entry)) {
            error = "Unexpected JSON format";
            break;
        }

        const char *color = color_entry->valuestring;

        bool state = cJSON_IsTrue(state_entry);

        if (!board_set_led_state(color, state)) {
            error = "Unsupported LED or board";
            break;
        }
    } while (0);

    if (!error) {
        livekit_rpc_return_ok(NULL);
    } else {
        livekit_rpc_return_error(error);
    }
    // Perform necessary cleanup after returning an RPC result.
    cJSON_Delete(root);
}

/// Invoked by a remote participant to get the current CPU temperature.
static void get_cpu_temp(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    float temp = board_get_temp();
    char temp_string[16];
    snprintf(temp_string, sizeof(temp_string), "%.2f", temp);
    livekit_rpc_return_ok(temp_string);
}

/// Invoked by a remote participant to discover the selected hardware profile.
static void get_board_info(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    // The RPC API does not mutate the payload but its result type predates
    // const-correct payload pointers.
    livekit_rpc_return_ok((char *)board_get_info_json());
}

void join_room()
{
    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return;
    }

    livekit_room_options_t room_options = {
        .publish = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .audio_encode = {
                .codec = LIVEKIT_AUDIO_CODEC_OPUS,
                .sample_rate = 16000,
                .channel_count = 1
            },
            .capturer = media_get_capturer()
        },
        .subscribe = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .renderer = media_get_renderer()
        },
        .on_state_changed = on_state_changed,
        .on_participant_info = on_participant_info,
#if CONFIG_LK_EXAMPLE_ENABLE_INTEROP_TEST
        .on_data_received = interop_test_on_data_received,
#endif
    };
    if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
        return;
    }

    // Register RPC handlers so they can be invoked by remote participants.
    livekit_room_rpc_register(room_handle, "set_led_state", set_led_state);
    livekit_room_rpc_register(room_handle, "get_cpu_temp", get_cpu_temp);
    livekit_room_rpc_register(room_handle, "get_board_info", get_board_info);
#if CONFIG_LK_EXAMPLE_ENABLE_INTEROP_TEST
    if (interop_test_start(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to register interoperability test endpoints");
    }
#endif

    livekit_err_t connect_res;
#ifdef CONFIG_LK_EXAMPLE_USE_SANDBOX
    // Option A: Sandbox token server.
    livekit_sandbox_res_t res = {};
    livekit_sandbox_options_t gen_options = {
        .sandbox_id = CONFIG_LK_EXAMPLE_SANDBOX_ID,
        .room_name = CONFIG_LK_EXAMPLE_ROOM_NAME,
        .participant_name = CONFIG_LK_EXAMPLE_PARTICIPANT_NAME
    };
    if (!livekit_sandbox_generate(&gen_options, &res)) {
        ESP_LOGE(TAG, "Failed to generate sandbox token");
        return;
    }
    connect_res = livekit_room_connect(room_handle, res.server_url, res.token);
    livekit_sandbox_res_free(&res);
#else
    // Option B: Pre-generated token.
    connect_res = livekit_room_connect(
        room_handle,
        CONFIG_LK_EXAMPLE_SERVER_URL,
        CONFIG_LK_EXAMPLE_TOKEN);
#endif

    if (connect_res != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect to room");
    }
}

void leave_room()
{
    if (room_handle == NULL) {
        ESP_LOGE(TAG, "Room not created");
        return;
    }
#if CONFIG_LK_EXAMPLE_ENABLE_INTEROP_TEST
    interop_test_stop();
#endif
    if (livekit_room_close(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to leave room");
    }
    if (livekit_room_destroy(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return;
    }
    room_handle = NULL;
}
