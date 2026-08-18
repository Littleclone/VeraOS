/* src/Vera_UART/16550.c */
#include "../../../header/Vera_UART/driver/16550.h"
#include "../../../header/Vera_Device_Driver/driver_support.h"
#include "../../../header/Vera_UART/uart.h"

// The Base Address of the 16550 Memory Register
volatile uint8_t* P_Base_Address = NULL;

// If needed for right now also the Size
uint64_t P_Size = 0;

// Bit 5
#define Mem_Register_Bit_Ready 0x20
#define Buffer_Not_Ready(reg) ((reg & Mem_Register_Bit_Ready) == 0)

/*
Initializes the 16550 UART Driver for use by the UART Overlay

params:
- (ptr) driver_info -> driver_Support Driver Struct for the 16550 Chip

return:
- VERA_ERR_NULL_PTR -> driver_info is NULL
- VERA_OK -> Driver is initialized and usable
*/
vera_state uart_16550_driver_init(void* driver_info) {
    if (driver_info == NULL) {
        return VERA_ERR_NULL_PTR;
    }

    // Get the Driver
    struct UART_16550_driver* driver = (struct UART_16550_driver*)driver_info;
    #if INCLUDE_DEBUG
    vera_uart_print("Enter uart_16550_driver_init\n");
    vera_uart_printf("Address: %i, Size: %i\n", driver->address_cell, driver->size_cell);
    vera_uart_print("Reg: <");
    uint32_t* temp_node_debug = driver->reg;
    for (uint32_t i = 0; i < (driver->byte_lenght / 4); ++i) {
        vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node_debug++));
    }
    vera_uart_print(">\n");
    #endif

    // Get the Reg Values and Save them as the Base Address and Size of the Range
    uint32_t* temp_node = driver->reg;
    uint64_t base_address;
    base_address = vera_utils_swap_endian32(*temp_node++);
    if (driver->address_cell == 2) {
        base_address <<= 32;
        base_address += vera_utils_swap_endian32(*temp_node++);
    }
    P_Base_Address = (volatile uint8_t*)base_address;
    vera_uart_printf("%p\n", base_address);
    uint64_t size = vera_utils_swap_endian32(*temp_node++);
    if (driver->size_cell == 2) {
        size <<= 32;
        size += vera_utils_swap_endian32(*temp_node++);
    }
    P_Size = size;

    // Disable not needed Functions
    #define Interrupt_Enable_Register *(P_Base_Address + 1)
    Interrupt_Enable_Register = 0;
    
    #if INCLUDE_DEBUG
    vera_uart_printf("P_Base: %p, P_Size: %ilx\n", P_Base_Address, P_Size);
    vera_uart_print("Leave uart_16550_driver_init\n");
    #endif
    return VERA_OK;
}

/*
Writes a Character to be sended by the 16550 Chip via UART

params:
- (const) character -> The Char to print

note:
- It will wait for ever character if lsr bit 5 is Set
*/
void uart_16550_putc(const char character) {
    volatile uint8_t* thr = P_Base_Address;
    volatile uint8_t* lsr = P_Base_Address + 5;
    while (Buffer_Not_Ready(*lsr)) {

    }
    *thr = character;
}