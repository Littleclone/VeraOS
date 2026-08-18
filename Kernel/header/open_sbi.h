#pragma once

#include <stdint.h>

typedef struct {
    long error;
    long value;
}sbi_ret;

extern sbi_ret sbi_call(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7);


#define SBI_Shutdown 0
#define SBI_Reset_No_Reason 0

#define sbi_reset(a0, a1) sbi_call(a0, a1, 0, 0, 0, 0, 0, 0x53525354)
