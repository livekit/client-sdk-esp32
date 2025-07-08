#include <stdbool.h>
#include "tca9554.h"
#include "codec_board.h"
#include "extend_io.h"
#include "driver/gpio.h"

typedef struct {
    int (*init)(uint8_t io_i2c_port);
    int (*set_dir)(int16_t gpio, bool output);
    int (*set_gpio)(int16_t gpio, bool high);
} extend_io_ops_t;

static extend_io_ops_t        extend_io_ops;

static int tca9554_io_init(uint8_t io_i2c_port)
{
    return tca9554_init(io_i2c_port);
}

static int tca9554_io_set_dir(int16_t gpio, bool output)
{
    gpio = (1 << gpio);
    tca9554_set_io_config(gpio, output ? TCA9554_IO_OUTPUT : TCA9554_IO_INPUT);
    return 0;
}

static int tca9554_io_set(int16_t gpio, bool high)
{
    gpio = (1 << gpio);
    return tca9554_set_output_state(gpio, high ? TCA9554_IO_HIGH : TCA9554_IO_LOW);
}

static void register_tca9554(void)
{
    extend_io_ops.init = tca9554_io_init;
    extend_io_ops.set_dir = tca9554_io_set_dir;
    extend_io_ops.set_gpio = tca9554_io_set;
}

int extend_io_init(uint8_t io_i2c_port)
{
    register_tca9554();
    return extend_io_ops.init(io_i2c_port);
}

int extend_io_set_pin_dir(int16_t pin, bool output)
{
    pin &= ~BOARD_EXTEND_IO_START;
    extend_io_ops.set_dir(pin, output);
    return 0;
}

int extend_io_set_pin_state(int16_t pin, bool high)
{
    extend_io_ops.set_gpio(pin, high);
    return 0;
}

int16_t extend_io_get_hw_gpio(int16_t pin)
{
    if (pin == -1) {
        return pin;
    }
    if (pin & BOARD_EXTEND_IO_START) {
        return -1;
    }
    return pin;
}