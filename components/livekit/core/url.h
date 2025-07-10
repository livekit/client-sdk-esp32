
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Constructs a signaling URL.
/// @param server_url The server URL beginning with ws:// or wss://.
/// @param token Access token.
/// @param out_url[out] The output URL.
/// @return True if the URL is constructed successfully, false otherwise.
/// @note The caller is responsible for freeing the output URL.
bool url_build(const char *server_url, const char *token, char **out_url);

#ifdef __cplusplus
}
#endif
