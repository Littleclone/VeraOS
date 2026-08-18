// header/Vera_Utils/utils.h
#pragma once

#include <stdint.h>
#include "../Vera_Error/error.h"
#include "general_define.h"
#include "../open_sbi.h"

typedef struct {
    void* ptr_array;                            // The Array with the Elements in it
    uint64_t counter;                           // The Amount of Elements currently in it
    uint64_t max_elements;                      // The Max Amount of Elements the save array can hold
}vera_utils_safe_array;


bool vera_utils_is_char(char character);

bool vera_utils_is_digit(char character);

bool vera_utils_is_upper(char character);

bool vera_utils_is_lower(char character);

void vera_utils_to_upper(char* string);

void vera_utils_to_lower(char* string);

uint64_t vera_utils_string_to_int(const char* string);

uint64_t vera_utils_str_len(const char* string);

bool vera_utils_str_cmp(const char* string1, const char* string2);

bool vera_utils_str_has(const char* string1, const char* string2);

uint64_t vera_utils_swap_endian64(uint64_t value);

uint32_t vera_utils_swap_endian32(uint32_t value);

uint64_t vera_utils_align(uint64_t value, uint16_t byte_align);

void vera_utils_mem_set(uint8_t* buffer, uint8_t value, uint64_t buffer_size);

void vera_utils_mem_set_64(uint64_t* buffer, uint64_t value, uint64_t buffer_size);

void vera_utils_mem_cpy(uint8_t* buffer, const uint8_t* ptr_cpy, uint64_t buffer_size);

void vera_utils_mem_cpy64(uint64_t* buffer, const uint64_t* ptr_cpy, uint64_t buffer_size);

bool vera_utils_mem_cmp(const uint8_t* buffer1, const uint8_t* buffer2, uint64_t buffer_size);

bool vera_utils_mem_cmp64(const uint64_t* buffer1, const uint64_t* buffer2, uint64_t buffer_size);

uint64_t vera_utils_calc_space(uint64_t needed_bytes, uint64_t v_address_start);