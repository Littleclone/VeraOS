/* header/Vera_UART/uart.h */
#pragma once
#include "../Vera_Utils/utils.h"



vera_state vera_uart_init_driver(void* raw_dtb_node, uint32_t driver_ID);

void vera_uart_print(const char* str);
void vera_uart_printf(char* str, ...);


