#include "esp_log.h"
#include "cJSON.h"
#include "bsp/esp-bsp.h"
#include "livekit.h"
#include "livekit_sandbox.h"
#include "media.h"
#include "board.h"
#include "ui.h"
#include "example.h"

static const char *TAG = "livekit_example";

static livekit_room_handle_t room_handle;

extern lv_subject_t ui_room_state;
extern lv_subject_t ui_is_call_active;

/// Invoked when the room's connection state changes.
static void on_state_changed(livekit_connection_state_t state, void* ctx)
{
    ESP_LOGI(TAG, "Room state: %s", livekit_connection_state_str(state));
    ui_acquire();
    lv_subject_set_int(&ui_room_state, (int)state);
    ui_release();
}

/// Invoked when participant information is received.
static void on_participant_info(const livekit_participant_info_t* info, void* ctx)
{
    if (info->kind != LIVEKIT_PARTICIPANT_KIND_AGENT) {
        // Only handle agent participants for this example.
        return;
    }
    char* verb;
    switch (info->state) {
        case LIVEKIT_PARTICIPANT_STATE_ACTIVE:
            verb = "joined";
            break;
        case LIVEKIT_PARTICIPANT_STATE_DISCONNECTED:
            verb = "left";
            break;
        default:
            return;
    }
    ESP_LOGI(TAG, "Agent has %s the room", verb);
}

/// Invoked by a remote participant to get the current CPU temperature.
static void get_cpu_temp(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    float temp = board_get_temp();
    char temp_string[16];
    snprintf(temp_string, sizeof(temp_string), "%.2f", temp);
    livekit_rpc_return_ok(temp_string);
}

/// Creates and configures the LiveKit room object.
static void init_room()
{
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
        .on_participant_info = on_participant_info
    };
    ESP_ERROR_CHECK(livekit_room_create(&room_handle, &room_options));

    // Register RPC handlers so they can be invoked by remote participants.
    livekit_room_rpc_register(room_handle, "get_cpu_temp", get_cpu_temp);
}

static void connect_room_async(void* arg)
{
    livekit_err_t connect_res;
#ifdef CONFIG_LK_USE_SANDBOX
    // Option A: Sandbox token server.
    livekit_sandbox_res_t res = {};
    livekit_sandbox_options_t gen_options = {
        .sandbox_id = CONFIG_LK_SANDBOX_ID,
        .room_name = CONFIG_LK_SANDBOX_ROOM_NAME,
        .participant_name = CONFIG_LK_SANDBOX_PARTICIPANT_NAME
    };
    if (!livekit_sandbox_generate(&gen_options, &res)) {
        ESP_LOGE(TAG, "Failed to generate sandbox token");
        return;
    }
    connect_res = livekit_room_connect(room_handle, res.server_url, res.token);
    livekit_sandbox_res_free(&res);
#else
    // Option B: Pre-generated token.
    connect_res = livekit_room_connect(room_handle, CONFIG_LK_SERVER_URL, CONFIG_LK_TOKEN);
#endif

    if (connect_res != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect to room");
    }
    media_lib_thread_destroy(NULL);
}

static void close_room_async(void* arg)
{
    livekit_room_close(room_handle);
    media_lib_thread_destroy(NULL);
}

static void on_ui_is_call_active_changed(lv_observer_t* observer, lv_subject_t* subject)
{
    bool is_call_active = lv_subject_get_int(subject);
    ESP_LOGI(TAG, "Call active changed: %d", is_call_active);

    const char* name = is_call_active ? "connect" : "close";
    void (*body)(void*) = is_call_active ? connect_room_async : close_room_async;
    ESP_ERROR_CHECK(media_lib_thread_create_from_scheduler(NULL, name, body, NULL));
}

void example_init()
{
    init_room();

    // Observe UI state changes
    lv_subject_add_observer(&ui_is_call_active, on_ui_is_call_active_changed, NULL);
}