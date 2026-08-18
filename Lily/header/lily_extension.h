/* header/lily_debug.h */
#pragma once

#include "../../Kernel/header/Vera_Error/error.h"
#include "../../Kernel/header/Vera_Utils/general_define.h"

void lily_utils_mem_set_64(uint64_t* buffer, const uint64_t value, size_t buffer_size);

void lily_utils_mem_set(uint8_t* buffer, const uint8_t value, size_t buffer_size);

uint64_t lily_utils_calc_space(const uint64_t needed_bytes, uint64_t v_address_start);

bool lily_utils_str_cmp(const char* string1, const char* string2);

bool lily_utils_str_has(const char* string1, const char* string2);

uint64_t lily_utils_align(uint64_t value, const uint16_t byte_align);

uint64_t lily_utils_str_len(const char* string);

uint64_t lily_utils_swap_endian64(const uint64_t value);

uint32_t lily_utils_swap_endian32(const uint32_t value);

void lily_panic_no_write(const vera_state s);

#if INCLUDE_DEBUG && INCLUDE_QEMU
void lily_panic(const vera_state status);
vera_state lily_uart_print(const char* str);
vera_state lily_uart_printf(char* str, ...);
#endif

// Trap Reason Defines
typedef enum {
    trap_SPI = 1,
    trap_STI = 5,
    trap_SEI = 9,
}Trap_Interrupt_Code_Lily;

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
}Trap_Exception_Code_Lily;


// Needs a Refactor for later then Userland exists.
typedef struct {
    uint64_t ra, fp;
    uint64_t a0, a1, a2, a3, a4, a5, a6, a7;            // A-Register, hier werden Argumente übergeben und Return Values gespeichert.
    uint64_t s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;  // Die S-Register, die werden vom Callee Gespeichert.
    uint64_t t0, t1, t2, t3, t4, t5, t6;                // Die T-Register, die werden vom Caller Gespeichert.
    uint64_t gp, tp;
}K_Trap_Register_Lily;

typedef struct {
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
}Trap_Information_Lily;

void trap_Handler_C_Lily(K_Trap_Register_Lily* Kernel_Register, Trap_Information_Lily* trap_infos);