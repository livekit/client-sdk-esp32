#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "livekit_core.h"

#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_websocket_client.h"
#include "esp_tls.h"

#include <esp_peer_signaling.h>

#define LIVEKIT_PROTOCOL_VERSION "15"
#define LIVEKIT_SDK_ID "esp32"
#define LIVEKIT_SDK_VERSION "alpha"
#define LIVEKIT_URL_MAX_LEN 2048

const esp_peer_signaling_impl_t *livekit_sig_get_impl(void);

/// @brief Builds a URL with the given URL and token, allocating memory for the result.
/// @param[in] base_url The base URL provided by the user.
/// @param[in] token The token provided by the user.
/// @param[out] out_url The resulting URL, allocated by the function.
/// @return 0 on success, -1 on error
int livekit_sig_build_url(const char *base_url, const char *token, char **out_url);

#ifdef __cplusplus
}
#endif