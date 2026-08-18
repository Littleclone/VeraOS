/* header/Vera_Utils/general_defines.h */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define INCLUDE_DEBUG 1
#define INCLUDE_QEMU 1

// --- Kernel ---

typedef uint64_t PID;    // Process-ID, public visible in Userland
typedef uint64_t I_PID;   // Intern_Process-ID, private and only for Kernel visible

#define Kernel_Process_ID 0

#define To_KiB(x) (x * 1024)
#define To_MiB(x) (x * 1024 * 1024)
#define To_GiB(x) (x * 1024 * 1024 * 1024)

// ------

// --- Utils ---

typedef uint16_t small_size;    // eine Kleine size Variable
typedef uint32_t mid_size;
typedef uint64_t big_size;

#define NOT_NULL ((void*)INT64_MAX)

// ------

// --- Allocator ---

typedef uint64_t memory_address;

// ------

// --- UART ---

#define UART_BASE_QEMU 0x10000000UL

// ------

// --- Memory ---

#define RAM_Min_512MiB 0x20000000

typedef uint64_t memory;

// ------

// --- Paging ---

typedef uint64_t phys_address;
typedef uint64_t virt_address;
typedef uint64_t page_table;
typedef uint64_t page_table_entry;

// Lower Half (User Space)
#define VA_LOWER_HALF_START  0x0000000000000000ULL
#define VA_LOWER_HALF_END    0x0000003FFFFFFFFFULL

// Upper Half (Kernel Space)
#define VA_UPPER_HALF_START  0xFFFFFFC000000000ULL
#define VA_UPPER_HALF_END    0xFFFFFFFFFFFFFFFFULL

#define Kernel_Base VA_UPPER_HALF_START

#define PAGING_GET_PTE(x) ((x >> 10) << 12)
#define PAGING_Phys_to_Virt(ptr) (ptr | VA_UPPER_HALF_START);
#define PAGING_Virt_to_Phys(ptr) (ptr ^ VA_UPPER_HALF_START);

#define Page_Size 0x1000  // 4096 Bytes, 4KiB
#define Page_Table_Size Page_Size
#define Max_entry 512

#define Kernel_text_flags (paging_Valid | paging_Read | paging_Execute | paging_Global | paging_Accessed | paging_Dirty)
#define Kernel_roData_flags (paging_Valid | paging_Read | paging_Global | paging_Accessed | paging_Dirty)
#define Kernel_data_flags (paging_Valid | paging_Read | paging_Write | paging_Global | paging_Accessed | paging_Dirty)
#define Kernel_heap_flags Kernel_data_flags
#define Kernel_memory_map_flags (paging_Accessed | paging_Dirty)

// ------


// --- Lily ---
#define Max_Memory_Nodes 64