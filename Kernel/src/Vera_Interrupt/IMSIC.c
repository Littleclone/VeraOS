/* src/Vera_Interrupt/IMSIC.c */
#include "../../header/Vera_Interrupt/IMSIC.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_Memory/mem_controller.h"
#include "../../header/Vera_Utils/CSR_functions.h"

typedef struct {
    uint32_t* reg;
    uint32_t* interrupted_extended;         // Extends den Controller mit dem Phandle und welcher Level (Phandle) (0x09 = S, 0x0b = M)
    uint32_t byte_lenght;                   // Reg
    uint32_t lenght_extended;
    uint32_t phandle;                       // Unique ID
    uint16_t id_size;                       // How many ID's can be used (Recommended = 0x1FF)
    uint8_t address_cell;               // Like always
    uint8_t size_cell;                  // Like always
    uint8_t interrupt_cells;
}base_config_struct;

static base_config_struct* base_config_save;

void IMSIC_init_informations(struct IMSICS_base_information* base_config) {
    base_config_save = (base_config_struct*)base_config;
}

vera_state driver_initialise_IMSIC() {
    #if INCLUDE_DEBUG
    vera_uart_print("Enter driver_initialise_IMSIC\n");
    #endif
    start_imsic();
    #if INCLUDE_DEBUG
    vera_uart_print("Leave driver_initialise_IMSIC\n");
    #endif
    return VERA_OK;
}

