#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Request options for generating a sandbox token.
typedef struct {
    /// @brief The sandbox ID.
    char *sandbox_id;

    /// @brief The room name the generated token will have.
    /// @note If not provided, one will be generated.
    char *room_name;

    /// @brief The participant identity the generated token will have.
    /// @note If not provided, one will be generated.
    char *participant_name;
} livekit_sandbox_options_t;

/// @brief Response containing the generated token details.
typedef struct {
    /// @brief The LiveKit Cloud URL for the associated project.
    char *server_url;

    /// @brief The access token for the participant. Valid for 15 minutes.
    char *token;

    /// @brief Generated token's room name.
    char *room_name;

    /// @brief Generated token's participant identity.
    char *participant_name;
} livekit_sandbox_res_t;

/// @brief Generate a sandbox token.
/// @param options[in] Options for generating the token.
/// @param res[out] The result to store the token details in.
/// @return True if the token was generated successfully, false otherwise.
/// @note If successful, the result must be freed using livekit_sandbox_res_free.
bool livekit_sandbox_generate(const livekit_sandbox_options_t *options, livekit_sandbox_res_t* res);

/// @brief Frees a sandbox result.
void livekit_sandbox_res_free(livekit_sandbox_res_t *result);

#ifdef __cplusplus
}
#endif