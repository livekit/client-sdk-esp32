#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "livekit_types.h"

/// @brief Initialize a LiveKit room
/// @param[in] options Options used for initializing the room
/// @param[out] handle Handle for referencing the room for further operations
/// @return livekit_ERR_NONE or error code
livekit_err_t livekit_create(livekit_options_t *options, livekit_handle_t *handle);

/// @brief Destroy a LiveKit room
/// @param[in] handle Room handle
/// @return livekit_ERR_NONE or error code
livekit_err_t livekit_destroy(livekit_handle_t handle);

/// @brief Connect to LiveKit room
/// @param[in] handle Room handle
/// @return livekit_ERR_NONE or error code
livekit_err_t livekit_connect(livekit_handle_t handle);

/// @brief Disconnect from LiveKit room
/// @param[in] handle Room handle
/// @return livekit_ERR_NONE or error code
livekit_err_t livekit_disconnect(livekit_handle_t handle);

/// @brief  Perform an RPC
/// @param[in] data Data for performing an RPC
/// @param[in] handle Room handle
/// @return livekit_ERR_NONE or error code
livekit_err_t livekit_perform_rpc(livekit_perform_rpc_data_t *data, livekit_handle_t handle);

#ifdef __cplusplus
}
#endif