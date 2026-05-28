/*
 * Host-side stub for esp_log.h. The RPC and protocol modules use these
 * macros only for diagnostics, so silencing them is harmless on a
 * Linux test build.
 */
#pragma once

#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
