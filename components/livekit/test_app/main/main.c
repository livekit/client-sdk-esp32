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

#include "codec_board.h"
#include "codec_init.h"
#include "media.h"
#include "unity_test_utils_memory.h"
#include "unity.h"

#include "livekit.h"

// MARK: - Main

void app_main(void) {
    livekit_system_init();

    #if CONFIG_IDF_TARGET_ESP32P4
    set_codec_board_type("ESP32_P4_DEV_V14");
    #else
    set_codec_board_type("ESP32_S3_BOX_3");
    #endif
    codec_init_cfg_t cfg = { .reuse_dev = false };
    init_codec(&cfg);
    media_init();

    printf("LiveKit Test Application\n");
    UNITY_BEGIN();
    unity_run_menu();
    UNITY_END();
}

// MARK: - Setup and teardown

#define TEST_MEMORY_LEAK_THRESHOLD 512 // bytes

void setUp() {
    unity_utils_set_leak_level(TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_record_free_mem();
}

void tearDown() {
    unity_utils_evaluate_leaks();
}

// MARK: - Test cases

TEST_CASE("initialize", "[basic]")
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
        }
    };

    livekit_room_handle_t room_handle;
    livekit_err_t res = livekit_room_create(&room_handle, &room_options);
    TEST_ASSERT_NOT_NULL_MESSAGE(room_handle, "Room handle is NULL after creation");
    TEST_ASSERT_EQUAL_MESSAGE(LIVEKIT_ERR_NONE, res, "Failed to create room");

    livekit_connection_state_t state = livekit_room_get_state(room_handle);
    TEST_ASSERT_EQUAL(LIVEKIT_CONNECTION_STATE_DISCONNECTED, state);

    res = livekit_room_destroy(room_handle);
    TEST_ASSERT_EQUAL_MESSAGE(LIVEKIT_ERR_NONE, res, "Failed to destroy room");
}
