#include "esp_log.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "livekit.h"
#include "network.h"
#include "media.h"
#include "ui.h"
#include "board.h"
#include "example.h"

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    livekit_system_init();
    board_init();
    media_init();
    ui_init();
    network_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, ui_handle_network_event);
}
