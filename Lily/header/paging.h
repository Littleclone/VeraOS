/* header/memory/paging.h */
#pragma once

#include "../../Kernel/header/kernel.h"

void paging_init_kernel_lily(kernel_boot_info* boot_info);


extern void activatePaging_Lily(uint64_t* root_table, uint16_t asid);

extern void switchPageTable_Lily(uint64_t* root_table, uint16_t asid);

extern void flushTLB_Cache_Lily(void);

