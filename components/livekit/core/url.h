
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Options for building a signaling URL.
typedef struct {
    const char *server_url;
    const char *token;
} url_build_options;

/// Constructs a signaling URL.
///
/// @param options The options for building the URL.
/// @param out_url[out] The output URL.
///
/// @return True if the URL is constructed successfully, false otherwise.
/// @note The caller is responsible for freeing the output URL.
///
bool url_build(const url_build_options *options, char **out_url);

#ifdef __cplusplus
}
#endif
