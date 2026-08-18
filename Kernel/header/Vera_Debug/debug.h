/* header/debug.h */
#pragma once

#include "../Vera_Utils/utils.h"

#if INCLUDE_DEBUG
void debug_mmu_simulation(uint64_t* root_table, uint64_t v_address_internal);
#endif
