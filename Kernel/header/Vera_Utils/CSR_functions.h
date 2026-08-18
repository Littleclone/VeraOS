/* header/Vera_Utils/CSR_functions.h */
#pragma once

#include "utils.h"

extern void start_imsic(void);

extern void set_indirect_csr(uint64_t index, uint64_t value);

extern uint64_t get_interrupt_stopei(void);