
#pragma once

#include "livekit_rpc.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *rpc_manager_handle_t;

typedef enum {
    RPC_MANAGER_ERR_NONE           =  0,
    RPC_MANAGER_ERR_INVALID_ARG    = -1,
    RPC_MANAGER_ERR_NO_MEM         = -2,
    RPC_MANAGER_ERR_INVALID_STATE  = -3,
    RPC_MANAGER_ERR_SEND_FAILED    = -4,
    RPC_MANAGER_ERR_REGISTRATION   = -5,
} rpc_manager_err_t;

typedef struct {
    void (*on_result)(const livekit_rpc_result_t* result, void* ctx);
    bool (*send_packet)(const livekit_pb_data_packet_t* packet, void *ctx);
    void* ctx;
} rpc_manager_options_t;

/// @brief Creates a new RPC manager.
rpc_manager_err_t rpc_manager_create(rpc_manager_handle_t *handle, const rpc_manager_options_t *options);

/// @brief Destroys an RPC manager.
rpc_manager_err_t rpc_manager_destroy(rpc_manager_handle_t handle);

/// @brief Registers a handler for an RPC method.
rpc_manager_err_t rpc_manager_register(rpc_manager_handle_t handle, const char* method, livekit_rpc_handler_t handler);

/// @brief Unregisters a handler for an RPC method.
rpc_manager_err_t rpc_manager_unregister(rpc_manager_handle_t handle, const char* method);

/// @brief Handles an incoming RPC packet.
rpc_manager_err_t rpc_manager_handle_packet(rpc_manager_handle_t handle, const livekit_pb_data_packet_t* packet);

#ifdef __cplusplus
}
#endif