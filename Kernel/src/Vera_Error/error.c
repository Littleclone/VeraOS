/* src/Vera_Error/error.c*/
#include "../header/Vera_Error/error.h"
#include "../../header/Vera_UART/uart.h"

/*
    This Functions returns the apropriate String as its Enum Counter part of `vera_state`

    Function description:
    - The Function takes the state and returns in a Switch case statement


    params:
    - status: vera_state -> the State of Vera

    return:
    - (const) (ptr) string: char -> The string translation from the given State
*/
static const char* vera_err_status_str(vera_state state) {
    switch (state) {
    case VERA_OK:          return "VERA_OK";
    case VERA_ERR:         return "VERA_ERR";
    case VERA_ERR_INVAL:   return "VERA_ERR_INVAL";
    case VERA_ERR_NOMEM:   return "VERA_ERR_NOMEM";
    case VERA_ERR_IO:      return "VERA_ERR_IO";
    case VERA_ERR_BUSY:    return "VERA_ERR_BUSY";
    case VERA_ERR_TIMEOUT: return "VERA_ERR_TIMEOUT";
    case VERA_ERR_NOSUP:   return "VERA_ERR_NOSUP";
    case VERA_ERR_PERM:    return "VERA_ERR_PERM";
    case VERA_TRAP:        return "VERA_TRAP";
    case VERA_ERR_NULL_PTR: return "VERA_NULL_PTR";
    case VERA_OVERFLOW:    return "VERA_OVERFLOW";
    case VERA_ERR_INVAL_BOOT_ID: return "VERA_ERR_INVAL_BOOT_ID";
    default:            return "VERA_ERR_UNKNOWN";
    }
}

/*
    Is the boot panic function for the early boot where the kernel is still not stable and in the stabilizing booting phase

    function description:
    - If in Debug Mode prints the Panic Reason out with the Panic Prefix and then disables Interrupts complettely for this Hart 
    goes into a wfi (Wait for interrupt) that never comes.

    params:
    - (const) state: vera_state -> the State given as the reason for the Panic
*/
void vera_err_boot_panic(vera_state state) {
    #if INCLUDE_DEBUG
    vera_uart_printf("PANIC: %s\n", vera_err_status_str(state));
    #endif
    
    __asm__ __volatile__("csrci sstatus, 0x2");
    for(;;){ __asm__ __volatile__("wfi"); }
}
