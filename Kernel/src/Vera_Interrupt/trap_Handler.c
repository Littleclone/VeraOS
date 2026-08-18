/* src/Vera_Interrupt/trap_Handler.c */
#include "../../header/Vera_Interrupt/trap_Handler.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_Utils/CSR_functions.h"



/*
Nach dem Trap Entry wird der Trap Handler aufgerufen, dieser bewertet die Trap aus

note:
- Kernel Only derzeit, außer ausgabe von Informationen passiert nicht viel, danach wird panic
- Is the Kernel Only Early Stage Trap Handler
*/
void trap_Handler_C_pre(trap_register* Kernel_Register, trap_information* trap_infos) {
    bool IsInterrupt = (trap_infos->scause >> 63);
    uint64_t trap_cause = ((trap_infos->scause << 1) >> 1);
    vera_uart_print("\n----------------\n[TRAP]:\n");
    vera_uart_printf("scause: %ix\ncurrent_mode: %i\nextra_info: %ilx\nTrapped_PC: %ilx\n----------------\n", trap_cause, (trap_infos->sstatus & 0x80), trap_infos->stval, trap_infos->sepc);
    if (IsInterrupt) {
        switch (trap_cause) {
            case trap_SPI:
                vera_uart_print("Software Interrupt\n");
                break;
            case trap_STI:
                vera_uart_print("Timer Interrupt\n");
                break;
            case trap_SEI:
                vera_uart_print("Hardware Interrupt\n");
                break;
            default:
                vera_uart_print("Error, Undefined Cause!\n");  // Sollte Nicht erreichbar sein.
                break;
        }
        vera_err_boot_panic(VERA_TRAP);    // TEMP
    }
    else {
        switch (trap_cause)
        {
        case trap_Instruction_address_misaligned:
            vera_uart_print("Instruction address misaligned\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Instruction_access_fault:
            vera_uart_print("Instruction access fault\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Illegal_instruction:
            vera_uart_print("Illegal instruction\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Breakpoint:
            vera_uart_print("Breakpoint\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Load_address_misaligned:
            vera_uart_print("Load Address misaligned\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Load_access_fault:
            vera_uart_print("Load Access fault\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_StoreAMO_address_misaligned:
            vera_uart_print("Store/AMO address misaligned\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_StoreAMO_access_fault:
            vera_uart_print("Store/AMO access fault\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Environment_call_from_U_Mode:
            vera_uart_print("User Syscall. (Environment call from U-Mode)\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Environment_call_from_S_Mode:
            vera_uart_print("Kernel Syscall. (Environemt call from S-Mode\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Instruction_page_fault:
            vera_uart_print("Instruction page fault\n");
            break;
        case trap_Load_page_fault:
            vera_uart_print("Load page fault\n");
            break;
        case trap_StoreAMO_page_fault:
            vera_uart_print("Store/AMO page fault\n");
            break;
        default:
            // Während Entwicklung hier, normal Kill Process / Panic
            vera_err_boot_panic(VERA_ERR_INVAL);
            break;
        }
    }
    vera_err_boot_panic(VERA_TRAP); // TEMP
    return;
}


void kernel_trap_handler(trap_register* process_register, trap_information* trap_infos) {
    bool IsInterrupt = (trap_infos->scause >> 63);
    uint64_t trap_cause = ((trap_infos->scause << 1) >> 1);
    vera_uart_print("\n----------------\n[TRAP]:\n");
    vera_uart_printf("scause: %ix\ncurrent_mode: %i\nextra_info: %ilx\nTrapped_PC: %ilx\n----------------\n", trap_cause, (trap_infos->sstatus & 0x100), trap_infos->stval, trap_infos->sepc);
    if (IsInterrupt) {
        switch (trap_cause) {
            case trap_SPI:
                vera_uart_print("Software Interrupt\n");
                break;
            case trap_STI:
                vera_uart_print("Timer Interrupt\n");
                break;
            case trap_SEI:
                vera_uart_print("Hardware Interrupt\n");
                uint64_t interrupt_ID = get_interrupt_stopei();
                if (interrupt_ID == 0) {
                    vera_uart_print("No Interrupt found on Hardware!\n");
                    return;
                }

                // haben Interrupt ID
                interrupt_ID >>= 16;

                vera_uart_printf("Found Interrupt ID: %ilx\n", interrupt_ID);

                break;
            default:
                vera_uart_print("Error, Undefined Cause!\n");  // Sollte Nicht erreichbar sein.
                break;
        }
        return;
    }
    else {
        switch (trap_cause)
        {
        case trap_Instruction_address_misaligned:
            vera_uart_print("Instruction address misaligned\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Instruction_access_fault:
            vera_uart_print("Instruction access fault\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Illegal_instruction:
            vera_uart_print("Illegal instruction\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Breakpoint:
            vera_uart_print("Breakpoint\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Load_address_misaligned:
            vera_uart_print("Load Address misaligned\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Load_access_fault:
            vera_uart_print("Load Access fault\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_StoreAMO_address_misaligned:
            vera_uart_print("Store/AMO address misaligned\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_StoreAMO_access_fault:
            vera_uart_print("Store/AMO access fault\n");
            vera_err_boot_panic(VERA_TRAP);
            break;
        case trap_Environment_call_from_U_Mode:
            vera_uart_print("User Syscall. (Environment call from U-Mode)\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Environment_call_from_S_Mode:
            vera_uart_print("Kernel Syscall. (Environemt call from S-Mode\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Instruction_page_fault:
            vera_uart_print("Instruction page fault\n");
            break;
        case trap_Load_page_fault:
            vera_uart_print("Load page fault\n");
            break;
        case trap_StoreAMO_page_fault:
            vera_uart_print("Store/AMO page fault\n");
            break;
        default:
            // Während Entwicklung hier, normal Kill Process / Panic
            vera_err_boot_panic(VERA_ERR_INVAL);
            break;
        }
    }
    vera_err_boot_panic(VERA_TRAP); // TEMP
    return;
}


