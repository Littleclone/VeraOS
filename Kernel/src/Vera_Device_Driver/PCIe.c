/* src/Vera_Device_Driver/PCIe.c */
#include "../../header/Vera_Device_Driver/PCIe.h"
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_Memory/mem_controller.h"
#include "../../header/Vera_UART/uart.h"


typedef struct {
    phys_address base_adress;
    uint64_t size;
    uint16_t bus_range;     // How many Bus we have to search
}PCIe_configuration;


static PCIe_configuration* P_PCIe_config;



static vera_state init_PCIe_driver(PCIe_configuration* config);

/*

*/
vera_state init_PCIe(struct PCIe_driver* driver) {
    #if INCLUDE_DEBUG
    vera_uart_print("Enter init_PCIe\n");
    #endif
    P_PCIe_config = (PCIe_configuration*)k_malloc(sizeof(PCIe_configuration));
    if (P_PCIe_config == NULL) {
        #if INCLUDE_DEBUG
        vera_err_boot_panic(VERA_ERR);
        #else
        k_panic_no_write(VERA_ERR_NULL_PTR);
        #endif
    }

    P_PCIe_config->bus_range = driver->bank_width;

    P_PCIe_config->base_adress = (uint32_t)vera_utils_swap_endian32(*driver->reg);
    ++driver->reg;
    if (driver->address_cell >= 2) {
        P_PCIe_config->base_adress <<= 32;
        P_PCIe_config->base_adress |= (uint32_t)vera_utils_swap_endian32(*driver->reg);
        ++driver->reg;
    }

    P_PCIe_config->size = (uint32_t)vera_utils_swap_endian32(*driver->reg);
    ++driver->reg;
    if (driver->address_cell >= 2) {
        P_PCIe_config->size <<= 32;
        P_PCIe_config->size |= (uint32_t)vera_utils_swap_endian32(*driver->reg);
        ++driver->reg;
    }


    #if INCLUDE_DEBUG
    vera_uart_print("Leave init_PCIe\n");
    #endif
}

static vera_state init_PCIe_driver(PCIe_configuration* config) {
    for (uint16_t bus = 0; bus <= config->bus_range; ++bus) {
        for (uint8_t device = 0; device <= 31; ++device) {
            for (uint8_t function = 0; function <= 7; ++function) {
                uint64_t offset = (bus << 20) | (device << 15) | (function << 12);
                uint32_t* adress = (uint32_t*)(config->base_adress + offset);
                if (*adress == 0xFFFFFFFF) {
                    break;
                }
                vera_uart_printf("Found!\nBus: %i, Device %i, Function: %i\nID: %ixX\n", bus, device, function, *adress);
            }
        }
    }
}

