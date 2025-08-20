#include <sys/time.h>
#include <stdint.h>
#include "esp_random.h"

#include "utils.h"

#define MAX_BACKOFF_MS 7000

int64_t get_unix_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL);
}

uint16_t backoff_ms_for_attempt(uint16_t attempt)
{
    if (attempt == 0) return 0;
    uint16_t base = 100 << attempt; // 100 * (2^attempt)
    uint16_t rand = (uint16_t)(esp_random() % 1001); // range [0, 1000]
    uint16_t total = base + rand;
    return total > MAX_BACKOFF_MS ? MAX_BACKOFF_MS : total;
}