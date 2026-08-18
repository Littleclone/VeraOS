/* src/Vera_Interrupt/APLIC.c */
#include "../../header/Vera_Interrupt/APLIC.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_Memory/mem_controller.h"



typedef struct {
    uint64_t base_address;
    uint64_t size;
    uint32_t phandle;                   // Unique ID
    uint32_t msi_parent;
    uint16_t num_sources;                       
    uint8_t address_cell;               // Like always
    uint8_t size_cell;                  // Like always
    uint8_t interrupt_cells;
}aplic_base;

static aplic_base* P_APLIC_base = NULL;

#define Domain_Config_Register 0x0000
#define Source_Config_Register 0x0004
#define Set_Interrupt_Enable_Register 0x1E00
#define Target_Register 0x3000
#define GlobalEnableAPLIC 0x100
#define Deliver_MSI 0x4

#define APLIC_to_Hart(hart_id) (hart_id << 18)
#define APLIC_to_EIID(eiid) (eiid & 0x7FF)
#define APLIC_Target_Register_Format(hart_id, eiid) (APLIC_to_Hart(hart_id) | APLIC_to_EIID(eiid))

void init_aplic_informations(struct APLIC_base_information* base) {
    #if INCLUDE_DEBUG
    vera_uart_print("Enter init_aplic_informations\n");
    #endif
    P_APLIC_base = (aplic_base*)k_malloc(sizeof(aplic_base));
    if (P_APLIC_base == NULL) {
        #if INCLUDE_DEBUG
        vera_err_boot_panic(VERA_ERR_NULL_PTR);
        #else
        k_panic_no_write(VERA_ERR_NULL_PTR);
        #endif
    }
    
    P_APLIC_base->base_address = vera_utils_swap_endian32(*base->reg);
    ++base->reg;
    if (base->byte_lenght > 8) {
        P_APLIC_base->base_address <<= 32;
        P_APLIC_base->base_address |= vera_utils_swap_endian32(*base->reg);
        ++base->reg;
    }
    P_APLIC_base->size = vera_utils_swap_endian32(*base->reg);
    ++base->reg;
    if (base->byte_lenght >= 16) {
        P_APLIC_base->size <<= 32;
        P_APLIC_base->size |= vera_utils_swap_endian32(*base->reg);
        ++base->reg;
    }

    P_APLIC_base->address_cell = base->address_cell;
    P_APLIC_base->interrupt_cells = base->interrupt_cells;
    P_APLIC_base->size_cell = base->size_cell;
    P_APLIC_base->num_sources = base->num_sources;

    k_free(base);
    #if INCLUDE_DEBUG
    vera_uart_print("Leave init_aplic_informations\n");
    #endif
    return;
}


vera_state driver_init_aplic() {
#if INCLUDE_DEBUG
    vera_uart_print("Enter driver_init_aplic\n");
#endif
    volatile uint32_t* base_address = (volatile uint32_t*)P_APLIC_base->base_address;

    // PAGE!!!
    k_mem_page_new_driver_area((phys_address)base_address, (virt_address)base_address, P_APLIC_base->size);
    
    *(base_address + Domain_Config_Register) = (GlobalEnableAPLIC | Deliver_MSI);

    // Deaktiviere alle Interrupts fürs erste in den Source Config Registers
    for (uint16_t i = 1; i <= P_APLIC_base->num_sources; ++i) {
        base_address[i] = 0;
        (base_address + Target_Register / 4)[i] = 0;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Leave driver_init_aplic\n");
#endif
    return VERA_OK;
}


void k_aplic_register_interrupt(uint64_t hart_id, uint16_t eiid, uint16_t interrupt_pin, uint8_t mode) {
    #if INCLUDE_DEBUG
    vera_uart_print("Enter k_aplic_register_interrupt\n");
    #endif
    if (interrupt_pin == 0 || mode == 0) {
        return;
    }
    volatile uint32_t* base_address = (volatile uint32_t*)P_APLIC_base->base_address;
    volatile uint32_t* interrupt_enable_Address = (volatile uint32_t*)base_address + Set_Interrupt_Enable_Register / 4;

    // 
    base_address[interrupt_pin] = mode;
    (base_address + Target_Register / 4)[interrupt_pin] = APLIC_Target_Register_Format(hart_id, eiid);

    // Activated Interrupt
    uint16_t register_index = interrupt_pin / 32;
    uint16_t bit_index = interrupt_pin % 32;

    interrupt_enable_Address[register_index] = (1 << bit_index);
    #if INCLUDE_DEBUG
    vera_uart_print("Leave k_aplic_register_interrupt\n");
    #endif
}


