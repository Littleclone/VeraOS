/* header/Vera_Interrupt/trap_Handler.h */
#pragma once

#include "../Vera_Utils/utils.h"

// Trap Reason Defines
typedef enum {
    trap_SPI = 1,
    trap_STI = 5,
    trap_SEI = 9,
}Trap_Interrupt_Code;

typedef enum {
    trap_Instruction_address_misaligned = 0,
    trap_Instruction_access_fault = 1,
    trap_Illegal_instruction = 2,
    trap_Breakpoint = 3,
    trap_Load_address_misaligned = 4,
    trap_Load_access_fault = 5,
    trap_StoreAMO_address_misaligned = 6,
    trap_StoreAMO_access_fault = 7,
    trap_Environment_call_from_U_Mode = 8,
    trap_Environment_call_from_S_Mode = 9,
    trap_Instruction_page_fault = 12,
    trap_Load_page_fault = 13,
    trap_StoreAMO_page_fault = 15,
}Trap_Exception_Code;


// Needs a Refactor for later then Userland exists.
typedef struct {
    uint64_t ra, fp;
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7;            // A-Register, hier werden Argumente übergeben und Return Values gespeichert.
    uint64_t s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;  // Die S-Register, die werden vom Callee Gespeichert.
    uint64_t t0, t1, t2, t3, t4, t5, t6;                // Die T-Register, die werden vom Caller Gespeichert.
    uint64_t gp, tp;
}trap_register;

typedef struct {
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
}trap_information;

void trap_Handler_C_pre(trap_register* Kernel_Register, trap_information* trap_infos);

void kernel_trap_handler(trap_register* process_register, trap_information* trap_infos);

