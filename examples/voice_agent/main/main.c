#include "esp_log.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "network.h"
#include "media_setup.h"
#include "board.h"
#include "livekit_demo.h"

#define RUN_ASYNC(name, body)           \
    void run_async##name(void *arg)     \
    {                                   \
        body;                           \
        media_lib_thread_destroy(NULL); \
    }                                   \
    media_lib_thread_create_from_scheduler(NULL, #name, run_async##name, NULL);

static int network_event_handler(bool connected)
{
    // Auto-join when network is connected
    if (connected) {
        RUN_ASYNC(join, {
            join_room();
        });
    }
    return 0;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    livekit_system_init();
    board_init();
    media_setup_init();
    network_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, network_event_handler);
    while (1)
    {
        media_lib_thread_sleep(2000);
    }
}
