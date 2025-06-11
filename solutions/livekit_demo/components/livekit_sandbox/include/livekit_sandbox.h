
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Response from the sandbox token generator.
typedef struct {
    /// @brief The LiveKit Cloud URL for the associated project.
    char *server_url;

    /// @brief The access token for the participant. Valid for 15 minutes.
    char *token;

    /// @brief The room name associated with the token.
    char *room_name;

    /// @brief The participant identity associated with the token.
    char *participant_name;
} livekit_sandbox_res_t;

/// @brief Generate a sandbox token.
/// @param sandbox_id The ID of your sandbox.
/// @param res The result to store the sandbox details.
/// @return True if the sandbox token was generated successfully, false otherwise.
/// @note If successful, the result must be freed using livekit_sandbox_res_free.
bool livekit_sandbox_generate(const char* sandbox_id, livekit_sandbox_res_t* res);

/// @brief Frees a sandbox result.
void livekit_sandbox_res_free(livekit_sandbox_res_t *result);

#ifdef __cplusplus
}
#endif