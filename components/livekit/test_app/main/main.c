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

#include "unity.h"
#include "livekit.h"

void app_main(void) {
    printf("LiveKit Test Application\n");
    UNITY_BEGIN();
    unity_run_menu();
    UNITY_END();
}

void setUp() {
    printf("Set up\n");
}

void tearDown() {
    printf("Tear down\n");
}

TEST_CASE("Default test", "[basic]")
{
    printf("Running default test\n");
}

TEST_CASE("Default test 2", "[basic]")
{
    printf("Running default test 2\n");
}