/* src/Vera_Device_Driver/driver_manager.c */
#include "../../header/Vera_Device_Driver/driver_manager.h"
#include "../../header/Vera_Interrupt/IMSIC.h"
#include "../../header/Vera_Interrupt/APLIC.h"
#include "../../header/Vera_Device_Driver/PCIe.h"
#include "../../header/Vera_UART/uart.h"


typedef enum {
    stopped = 0,
    run = 1,
    prepare = 2,
    error = 3
} flag_driver_states;

typedef struct {
    uint64_t APLIC_run : 2;
    uint64_t IMSIC_run : 2;
    uint64_t PCIe_run : 2;
} driver_state;


vera_state k_init_driver_kernel() {
    #if INCLUDE_DEBUG
    vera_uart_print("Enter k_init_driver_kernel\n");
    #endif
    vera_state status = VERA_OK;

    status = driver_initialise_IMSIC();
    if (VERA_FAILED(status)) {
        return status;
    }
    status = driver_init_aplic();
    if (VERA_FAILED(status)) {
        return status;
    }

    #if INCLUDE_DEBUG
    vera_uart_print("Leave k_init_driver_kernel\n");
    #endif
    return VERA_OK;
}
