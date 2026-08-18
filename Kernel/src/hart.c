/* src/hart.c */
#include "../header/hart.h"
#include "../header/Vera_Memory/allocator.h"
#include "../header/Vera_UART/uart.h"



static vera_utils_safe_array* P_harts;


vera_state k_init_harts(vera_utils_safe_array *hart_array)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter k_init_harts\n");
#endif
    hart_info_struct_deep *harts = (hart_info_struct_deep *)hart_array->ptr_array;

    for (uint32_t i = 0; i < hart_array->counter; ++i)
    {
        if (!(harts->hart_features_t.has_m && harts->hart_features_t.has_a && harts->hart_features_t.has_c && harts->hart_features_t.has_bitmanip && harts->hart_features_t.has_ssaia && harts->hart_features_t.has_sstc))
        {
#if INCLUDE_DEBUG
            vera_err_boot_panic(VERA_ERR_NOSUP);
#else
            k_panic_no_write(VERA_ERR_NOSUP);
#endif
        }
    }

    P_harts = hart_array;

#if INCLUDE_DEBUG
    vera_uart_print("Leave k_init_harts\n");
#endif
}

void hart_init_function_flags(hart_info_struct_deep *hart, uint32_t length)
{
#if INCLUDE_DEBUG
    vera_uart_print("Enter hart_init_function_flags\n");
#endif
    char *isa_string_list = hart->isa_string;
    bool has_zba = false;
    bool has_zbb = false;
    bool has_zbc = false;
    bool has_zbs = false;
    while (length)
    {

        if (vera_utils_str_cmp(isa_string_list, "m"))
        {
            hart->hart_features_t.has_m = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "a"))
        {
            hart->hart_features_t.has_a = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "c"))
        {
            hart->hart_features_t.has_c = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "zba"))
        {
            has_zba = true;
            if (has_zba && has_zbb && has_zbc && has_zbs)
            {
                hart->hart_features_t.has_bitmanip = 1;
            }
        }
        else if (vera_utils_str_cmp(isa_string_list, "zbb"))
        {
            has_zbb = true;
            if (has_zba && has_zbb && has_zbc && has_zbs)
            {
                hart->hart_features_t.has_bitmanip = 1;
            }
        }
        else if (vera_utils_str_cmp(isa_string_list, "zbc"))
        {
            has_zbc = true;
            if (has_zba && has_zbb && has_zbc && has_zbs)
            {
                hart->hart_features_t.has_bitmanip = 1;
            }
        }
        else if (vera_utils_str_cmp(isa_string_list, "zbs"))
        {
            has_zbs = true;
            if (has_zba && has_zbb && has_zbc && has_zbs)
            {
                hart->hart_features_t.has_bitmanip = 1;
            }
        }
        else if (vera_utils_str_cmp(isa_string_list, "ssaia"))
        {
            hart->hart_features_t.has_ssaia = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "sstc"))
        {
            hart->hart_features_t.has_sstc = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "f"))
        {
            hart->hart_features_t.has_f = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "d"))
        {
            hart->hart_features_t.has_d = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "v"))
        {
            hart->hart_features_t.has_v = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "svadu"))
        {
            hart->hart_features_t.has_svadu = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "zicbom"))
        {
            hart->hart_features_t.has_zicbom = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "zicboz"))
        {
            hart->hart_features_t.has_zicboz = 1;
        }
        else if (vera_utils_str_cmp(isa_string_list, "zawrs"))
        {
            hart->hart_features_t.has_zawrs = 1;
        }

        uint32_t str_len = (uint32_t)vera_utils_str_len(isa_string_list) + 1;
        isa_string_list += str_len;
        length -= str_len;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Leave hart_init_function_flags\n");
#endif
}
