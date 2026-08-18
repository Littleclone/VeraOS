/* header/Vera_Memory/allocator.h */
#pragma once

#include "../Vera_Utils/utils.h"

vera_state alloc_init_allocator();

void* k_malloc(size_t request_bytes);

void k_free(void *ptr);