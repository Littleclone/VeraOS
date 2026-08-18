/* src/Vera_FS/cfi_flash.h */
#pragma once

#include "../Vera_Utils/utils.h"
#include "../Vera_Device_Driver/driver_support.h"

vera_state init_cfi_flash(struct FLASH_cfi_driver *driver_information);

vera_state read_cfi_flash(uint8_t* base_to_read, uint32_t size, uint8_t* buffer);

