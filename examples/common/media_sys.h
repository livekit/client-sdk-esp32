/* Media system

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "esp_webrtc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Build media system
 *
 * @param[in]  rtc_handle  WebRTC handle
 *
 * @return
 *      - 0       On success
 *      - Others  Fail to build
 */
int media_sys_buildup(void);

/**
 * @brief  Get media provider
 * 
 * @param[out]  provider  Media provider to be returned
 *
 * @return
 *      - 0       On success
 *      - Others  Invalid argument
 */
int media_sys_get_provider(esp_webrtc_media_provider_t *provider);

/**
 * @brief  Play captured media directly
 *
 * @return
 *      - 0       On success
 *      - Others  Fail to capture or play
 */
int test_capture_to_player(void);

/**
 * @brief  Play music
 *
 * @param[in]  data      Music data to be played
 * @param[in]  size      Music data size
 * @param[in]  duration  Play duration, when duration over data duration will replay
 *
 * @return
 *      - 0       On success
 *      - Others  Fail to play
 */
int play_music(const uint8_t *data, int size, int duration);

/**
 * @brief  Stop music
 *
 * @return
 *      - 0       On success
 *      - Others  Fail to stop
 */
int stop_music(void);

#ifdef __cplusplus
}
#endif