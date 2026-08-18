/* header/Vera_Device_Driver/driver_support.h */
#pragma once

#include "../Vera_Utils/utils.h"

#define Virt_IO "virtio,mmio"
#define Virt_IO_Block_Device 2

#define Virt_IO_ACKNOWLEDGE 1 // Indicates that the guest OS has found the device and recognized it as a valid virtio device
#define Virt_IO_DRIVER 2 // Indicates that the guest OS knows how to drive the device. Note: There could be a significant (or infinite) delay before setting this bit. For example, under Linux, drivers can be loadable modules
#define Virt_IO_DRIVER_OK 4 // Indicates that the driver is set up and ready to drive the device.
#define Virt_IO_FEATURES_OK 8 // Indicates that the driver has acknowledged all the features it understands, and feature negotiation is complete
#define Virt_IO_DEVICE_NEEDS_RESET 64 // Indicates that the device has experienced an error from which it can’t re-cover.
#define Virt_IO_FAILED 128 // Indicates that something went wrong in the guest, and it has given up on the device. This could be an internal error, or the driver didn’t like the device for some reason, or even a fatal error during device operation


// --- UART ---

#define  UART_16550 "ns16550a"
#define UART_16550_ID 1

struct UART_16550_driver {
    uint32_t* reg;
    uint32_t byte_lenght;
    uint8_t address_cell;
    uint8_t size_cell;
};

// -------

// --- File_System ---

#define FS_cfi_flash "cfi-flash"
#define FS_cfi_flash_ID 1

#define FS_VirtIO_Storage_ID 2

struct FLASH_cfi_driver {
    uint32_t* reg;
    uint32_t byte_lenght;
    uint8_t address_cell;
    uint8_t size_cell;
    uint8_t bank_width;
};

// -------


// --- PCI(e) ---

#define PCIe_Driver "pci-host-ecam-generic"

struct PCIe_driver {
    uint32_t* reg;                      // Start point
    uint32_t byte_lenght;               // Maybe unused?
    uint16_t bank_width;                // how big is the Bus?
    uint8_t address_cell;               // Like always
    uint8_t size_cell;                  // Like always
};


// ------- 


// --- APLIC ---

#define APLIC_Risc_V "riscv,aplic"




struct APLIC_base_information {
    uint32_t* reg;
    uint32_t byte_lenght;               // Reg
    uint32_t phandle;                       // Unique ID
    uint32_t msi_parent;
    uint16_t num_sources;                       
    uint8_t address_cell;               // Like always
    uint8_t size_cell;                  // Like always
    uint8_t interrupt_cells;
};

// -------

// --- IMSICS ---

#define IMSICS_Risc_V "riscv,imsics"

struct IMSICS_base_information {
    uint32_t* reg;
    uint32_t* interrupted_extended;         // Extends den Controller mit dem Phandle und welcher Level (Phandle) (0x09 = S, 0x0b = M)
    uint32_t byte_lenght;               // Reg
    uint32_t lenght_extended;
    uint32_t phandle;                       // Unique ID
    uint16_t id_size;                       // How many ID's can be used (Recommended = 0x1FF)
    uint8_t address_cell;               // Like always
    uint8_t size_cell;                  // Like always
    uint8_t interrupt_cells;
};

// -------

