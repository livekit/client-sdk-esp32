/*
 * Copyright 2025 LiveKit, Inc.
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

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Request options for generating a sandbox token.
typedef struct {
    /// The sandbox ID.
    char *sandbox_id;

    /// The room name the generated token will have.
    /// @note If not provided, one will be generated.
    char *room_name;

    /// The participant identity the generated token will have.
    /// @note If not provided, one will be generated.
    char *participant_name;
} livekit_sandbox_options_t;

/// Response containing the generated token details.
typedef struct {
    /// The LiveKit Cloud URL for the associated project.
    char *server_url;

    /// The access token for the participant. Valid for 15 minutes.
    char *token;

    /// Generated token's room name.
    char *room_name;

    /// Generated token's participant identity.
    char *participant_name;
} livekit_sandbox_res_t;

/// Generate a sandbox token.
/// @param options[in] Options for generating the token.
/// @param res[out] The result to store the token details in.
/// @return True if the token was generated successfully, false otherwise.
/// @note If successful, the result must be freed using livekit_sandbox_res_free.
bool livekit_sandbox_generate(const livekit_sandbox_options_t *options, livekit_sandbox_res_t* res);

/// Frees a sandbox result.
void livekit_sandbox_res_free(livekit_sandbox_res_t *result);

#ifdef __cplusplus
}
#endif