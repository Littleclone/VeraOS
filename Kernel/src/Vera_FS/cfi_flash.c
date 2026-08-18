/* src/Vera_FS/cfi_flash.c */
#include "../../header/Vera_FS/cfi_flash.h"
#include "../../header/Vera_Utils/utils.h"
#include "../../header/Vera_UART/uart.h"
#include "../../header/Vera_Memory/allocator.h"
#include "../../header/Vera_Memory/mem_controller.h"

struct cfi_flash_controller
{
    uint64_t adress;
    uint64_t size;
    uint8_t bank_width;
};

static vera_utils_safe_array *P_Controllers = NULL;

/*
    Initialises the CFI Flash Driver

    params:
    - (ptr) driver_information -> The Driver informations

    return:
    - VERA_OK -> Okay
    - VERA_ERR_NULL_PTR -> NULL pointer
*/
vera_state init_cfi_flash(struct FLASH_cfi_driver *driver_information)
{
    #if INCLUDE_DEBUG
    vera_uart_print("init_cfi_flash\n");
    #endif
    // Init
    if (driver_information == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
#if INCLUDE_DEBUG
    vera_uart_print("Enter init_cfi_flash\n");
    vera_uart_printf("Address: %i, Size: %i\n", driver_information->address_cell, driver_information->size_cell);
    vera_uart_print("Reg: <");
    uint32_t *temp_node_debug = driver_information->reg;
    for (uint32_t i = 0; i < (driver_information->byte_lenght / 4); ++i)
    {
        vera_uart_printf("%ixX ", vera_utils_swap_endian32(*temp_node_debug++));
    }
    vera_uart_print(">\n");
    vera_uart_printf("Bank Width: %i\n", driver_information->bank_width);
#endif

    // Falls nicht schon vorhanden, brauchen wir es.
    if (P_Controllers == NULL)
    {
        P_Controllers = (vera_utils_safe_array *)k_malloc(sizeof(vera_utils_safe_array));
        if (P_Controllers == NULL)
        {
            return VERA_ERR_NULL_PTR;
        }
        P_Controllers->ptr_array = (void *)k_malloc(sizeof(struct cfi_flash_controller));
        if (P_Controllers->ptr_array == NULL)
        {
            k_free(P_Controllers);
            return VERA_ERR_NULL_PTR;
        }
        P_Controllers->counter = 0;
        P_Controllers->max_elements = 1;
    }

    // Get the Node Information
    for (uint32_t i = 0; i < (driver_information->byte_lenght / 4); i += 2)
    {
        if (P_Controllers->counter >= P_Controllers->max_elements)
        {
            struct cfi_flash_controller *temp_control = (struct cfi_flash_controller *)P_Controllers->ptr_array;
            P_Controllers->ptr_array = (struct cfi_flash_controller *)k_malloc((sizeof(struct cfi_flash_controller) * (P_Controllers->counter + 1)));
            if (P_Controllers->ptr_array == NULL)
            {
                P_Controllers->ptr_array = (void *)temp_control;
                return VERA_ERR_NULL_PTR;
            }
            P_Controllers->max_elements = P_Controllers->counter + 1;
            struct cfi_flash_controller *temp = (struct cfi_flash_controller *)P_Controllers->ptr_array;
            for (uint32_t j = 0; j < P_Controllers->counter; ++j)
            {
                temp[j].adress = temp_control[j].adress;
                temp[j].size = temp_control[j].size;
                temp[j].bank_width = temp_control[j].bank_width;
            }
            k_free(temp_control);
        }
        struct cfi_flash_controller *controller = (struct cfi_flash_controller*)P_Controllers->ptr_array;
        controller = &controller[P_Controllers->counter];
        controller->adress = (uint64_t)vera_utils_swap_endian32(*driver_information->reg++);

        if (driver_information->address_cell >= 2)
        {
            ++i;
            controller->adress <<= 32;
            controller->adress |= (uint64_t)vera_utils_swap_endian32(*driver_information->reg++);
        }

        controller->size = (uint64_t)vera_utils_swap_endian32(*driver_information->reg++);

        if (driver_information->size_cell >= 2)
        {
            ++i;
            controller->size <<= 32;
            controller->size |= (uint64_t)vera_utils_swap_endian32(*driver_information->reg++);
        }

        controller->bank_width = driver_information->bank_width;

        // Page it
        k_mem_page_new_driver_area(controller->adress, controller->adress, controller->size);

        // Check if its CFI in the first place
        volatile uint8_t *flash_test = (uint8_t *)controller->adress;

        *(flash_test + (controller->bank_width * 0x55)) = 0x98;
        #if INCLUDE_DEBUG
        #endif
        // If Test Fails, unpage it, if not, keep
        if (*(flash_test + (controller->bank_width * 0x10)) != 'Q' || *(flash_test + (controller->bank_width * 0x11)) != 'R' || *(flash_test + (controller->bank_width * 0x12)) != 'Y') {
            k_mem_unpage_new_driver_area(controller->adress, controller->adress, controller->size);
        }
        // Not fail
        ++P_Controllers->counter;
        *flash_test = 0xFF;
    }
    #if INCLUDE_DEBUG
    struct cfi_flash_controller *temp = (struct cfi_flash_controller *)P_Controllers->ptr_array;
    // Configure the Flash for init.
    for (uint16_t i = 0; i < P_Controllers->counter; ++i)
    {
        struct cfi_flash_controller *test = &temp[i];
        vera_uart_printf("Address: %p, Size: %p, Bank: %i\n", test->adress, test->size, test->bank_width);
    }
    #endif
    #if INCLUDE_DEBUG
    vera_uart_print("Leave init_cfi_flash\n");
    #endif
    return VERA_OK;
}

/*
    Allows to read by the specifit adress with size written in the buffer of choice

    params:
    - (ptr) base_to_read -> The Base adress we want to start reading
    - size -> The size we want to read in 1 Bytes
    - (ptr) buffer -> The buffer we readed we write in it.

    return:
    - VERA_OK -> Alles okay
    - VERA_ERR_NULL_PTR -> Error
*/
vera_state read_cfi_flash(uint8_t* base_to_read, uint32_t size, uint8_t* buffer) {
    #if INCLUDE_DEBUG
    vera_uart_print("Enter read_cfi_flash\n");
    #endif
    if (base_to_read == NULL || size == 0 || buffer == NULL) {
        return VERA_ERR_NULL_PTR;
    }

    // Get the needed Controller
    struct cfi_flash_controller* needed_controller = (struct cfi_flash_controller*)P_Controllers->ptr_array;
    struct cfi_flash_controller* target_controller = NULL;
    for (uint16_t i = 0; i < P_Controllers->max_elements; ++i) {
        if (needed_controller->adress >= base_to_read && (needed_controller->adress + needed_controller->size) <= (base_to_read + size)) {
            target_controller = needed_controller;
            break;
        }
        ++needed_controller;
        target_controller = NULL;
    }

    if (target_controller == NULL) {
        return VERA_ERR_NULL_PTR;
    }

    // make sure it is set to read mode.
    target_controller->adress = 0xFF;

    // Read and write into the buffer
    for (uint32_t i = 0; i < size; ++i) {
        *buffer = base_to_read[i];
    }

    #if INCLUDE_DEBUG
    vera_uart_print("Leave read_cfi_flash\n");
    #endif
}