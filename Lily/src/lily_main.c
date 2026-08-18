/* src/lily_main.c */
#include <stdint.h>
#include <stddef.h>

#include "../../Kernel/header/kernel.h"
#include "../header/dtb_parser.h"
#include "../header/mem_controller.h"
#include "../header/paging.h"
#include "../header/lily_extension.h"


// No Return
extern void kernel_jump(kernel_boot_info* boot_info, uint64_t hard_id);

void lily_main(uint64_t hart_id, void* dtb_ptr, kernel_area* area) {
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("\n");
    lily_uart_printf("Lily Starts from Core: %il!\n", hart_id);
    lily_uart_print("\n");
    #endif
    
    // Set the Kernel_Base_VA
    area->kernel_base_VA = Kernel_Base + (area->kernel_start - Kernel_Base); // Kernel_Base
    // Prepare the Memory Nodes for Use in the Kernel
    kernel_boot_info* boot_info_lily = dtb_setup_memory(dtb_ptr, area);
    
    // Paging
    paging_init_kernel_lily(boot_info_lily);
    
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("\nLily Ends!\n\n");
    #endif



    // Jump to Kernel
    kernel_jump(boot_info_lily, hart_id);

    #if INCLUDE_DEBUG && INCLUDE_QEMU
    lily_uart_print("\n\nLily Fault!\n\n");
    #endif
    
    for(;;){ __asm__ __volatile__("wfi"); }
}
