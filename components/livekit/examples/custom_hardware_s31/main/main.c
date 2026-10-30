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

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "livekit.h"
#include "livekit_example_utils.h"

#include "board.h"
#include "example.h"
#include "media.h"
#include "self_test.h"

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    livekit_system_init();
    board_init();
    run_audio_self_tests();
    media_init();

    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        2, ESP_SNTP_SERVER_LIST("time.google.com", "pool.ntp.org"));
    esp_netif_sntp_init(&sntp_config);

    if (lk_example_network_connect()) {
        join_room();
    }
}
