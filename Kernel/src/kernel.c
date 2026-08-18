/* src/kernel.c */
#include "../header/kernel.h"
#include "../header/Vera_Utils/utils.h"
#include "../header/Vera_UART/uart.h"
#include "../header/Vera_Device_Driver/dtb_parser.h"
#include "../header/Vera_Memory/mem_controller.h"
#include "../header/Vera_Memory/allocator.h"
#include "../header/Vera_Device_Driver/driver_manager.h"

void k_main(kernel_boot_info *boot_info_from_lily, uint64_t hart_id)
{
    if (!vera_utils_str_cmp((char*)boot_info_from_lily->magic, (char *)"LILY_BOOT\0"))
    {
        vera_err_boot_panic(VERA_ERR_INVAL_BOOT_ID);
    }
// Kernel Init nach Lily Loader
#if INCLUDE_DEBUG
    vera_uart_printf("Vera Starts, Lily: %s\n", boot_info_from_lily->magic);
    vera_uart_printf("Vera Starts: %ilx, Vera Ends: %ilx\n", boot_info_from_lily->kernel_region.kernel_start, boot_info_from_lily->kernel_region.kernel_end);
    vera_uart_printf("Text Starts: %p, Text Ends: %p\n", boot_info_from_lily->kernel_region.text_start, boot_info_from_lily->kernel_region.text_end);

    vera_uart_printf("Temp-Heap pointer: %p, Temp-Heap size: %ilx\n", boot_info_from_lily->temp_heap, boot_info_from_lily->temp_heap_size);
    vera_uart_printf("Temp_End: %p\n", boot_info_from_lily->temp_heap + boot_info_from_lily->temp_heap_size);
    vera_uart_print("\n\n");
#endif
    vera_state status = VERA_OK;

    // Paging init
    status = mem_init_kernel(boot_info_from_lily, boot_info_from_lily->dtb);
    if (VERA_FAILED(status))
    {
        vera_err_boot_panic(status);
    }

    // Convert Boot Info to Virtuell.

    boot_info_from_lily = (kernel_boot_info*)PAGING_Phys_to_Virt((uint64_t)boot_info_from_lily);

    // Allokator init
    status = alloc_init_allocator();
    if (VERA_FAILED(status))
    {
        vera_err_boot_panic(status);
    }

    // DTB Parsen:
    status = dtb_init(hart_id, boot_info_from_lily->dtb);

    // Init Interrupts and Driver
    status = k_init_driver_kernel();


#if INCLUDE_DEBUG
    vera_uart_print("Vera Ends\n");
#endif

    sbi_ret temp = sbi_reset(SBI_Shutdown, SBI_Reset_No_Reason);

    for (;;)
    {
        __asm__ __volatile__("wfi");
    }
}
