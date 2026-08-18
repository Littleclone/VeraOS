/* header/Vera_UART/16550.h */
#pragma once

#include "../../Vera_Utils/utils.h"

vera_state uart_16550_driver_init(void* driver_info);

void uart_16550_putc(const char character);
