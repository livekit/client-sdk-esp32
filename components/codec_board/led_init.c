#include "codec_init.h"
#include "extend_io.h"

static int16_t get_pin_by_led_number(int led_number)
{
    switch (led_number) {
        case 0: return 6;
        case 1: return 7;
        default: return -1;
    }
}

int board_led_init(void)
{
    return extend_io_init(0);
}

int board_led_set(int led_number, const bool state)
{
    int16_t pin = get_pin_by_led_number(led_number);
    if (pin == -1) return -1;

    extend_io_set_pin_dir(pin, true);
    extend_io_set_pin_state(pin, state);
    return 0;
}