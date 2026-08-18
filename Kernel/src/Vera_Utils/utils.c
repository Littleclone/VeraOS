// src/Vera_Utils/utils.c
#include "../../header/Vera_Utils/utils.h"
#include "../../header/Vera_UART/uart.h"


// Chars / Strings

/*
    Checks if the given Char is an ASCII Character

    params:
    - character: char -> The Character we want to test

    returns:
    - true -> Is an ASCII Character
    - false -> Is not an ASCII Character
*/
bool vera_utils_is_char(char character) {
    return ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'z'));
}

/*
    Checks if the given Char is an ASCII Number

    params:
    - character: char -> The Character we want to test

    returns:
    - true -> Is an ASCII Number
    - false -> Is not an ASCII Number
*/
bool vera_utils_is_digit(char character) {
    return character >= '0' && character <= '9';
}

/*
    Checks if the given Char is an Upper Case ASCII Character

    params:
    - character: char -> The Character we want to test

    returns:
    - true -> Is an Upper Case ASCII Character
    - false -> Is not an Upper Case ASCII Character
*/
bool vera_utils_is_upper(char character) {
    return character >= 'A' && character <= 'Z';
}

/*
    Checks if the given Char is an Lower Case ASCII Character

    params:
    - character: char -> The Character we want to test

    returns:
    - true -> Is an Lower Case ASCII Character
    - false -> Is not an Lower Case ASCII Character
*/
bool vera_utils_is_lower(char character) {
    return character >= 'a' && character <= 'z';
}

/*
    Makes the Entire String to an Upper Case character until the Null Terminater byte is reached

    params:
    - (ptr) string: char -> The String we want to make Upper Case

    params checks:
    - if String == NULL, if true, return;
*/
void vera_utils_to_upper(char* string) {
    if (string == NULL) {
        return;
    }
    while (*string) {
        if (vera_utils_is_lower(*string)) {
            *string -= 32;
        }
        ++string;
    }
}

/*
    Makes the Entire String to an Lower Case character until the Null Terminater byte is reached

    params:
    - (ptr) string: char -> The String we want to make Lower Case

    params checks:
    - if String == NULL, if true, return;
*/
void vera_utils_to_lower(char* string) {
    if (string == NULL) {
        return;
    }
    while (*string) {
        if (vera_utils_is_upper(*string)) {
            *string += 32;
        }
        ++string;
    }
}

/*
    Extracts out of a String an 64-Bit Integer, this String is only allowed to be out of Numbers

    params:
    - (const) (ptr) string: char -> The String we want to extract the numbers from

    params check:
    - if string == NULL -> return UINT64_MAX;
    - If there is a non Digit ASCII char, it ends

    returns:
    - X: uint64_t -> The Number from Left to right (1:1, example, "123" would be 123)
    - UINT64_MAX -> String is NULL or there is a non ASCII Digit Char in the String.
*/
uint64_t vera_utils_string_to_int(const char* string) {
    if (string == NULL) {
        return UINT64_MAX;
    }
    uint64_t number = 0;
    while (*string) {
        if (vera_utils_is_digit(*string)) {
            const uint64_t C_temp = (*string - '0');
            number *= 10;
            number += C_temp;
        }
        else {
            return UINT64_MAX;
        }
    }
    return number;
}

/*
    Calculate the string lenght of the given String in Bytes (Without Null terminater byte)

    params:
    - (const) (ptr) string: char -> The String we want the length

    params check:
    - if string == NULL return 0;

    returns:
    - X: uint64_t -> The Length of the String in Bytes (Without Null Terminater Byte)
    - 0: uint64_t -> String Pointer was null.
*/
uint64_t vera_utils_str_len(const char* string) {
    if (string == NULL) {
        return 0;
    }
    uint64_t str_counter = 0;
    while (*string) {
        ++str_counter;
        ++string;
    }
    return str_counter;
}

/*
    Compares two Strings together and looks if they are identical

    params:
    - (const) (ptr) string1: char -> The First string
    - (const) (ptr) string2: char -> The second String

    params check:
    - if string1 or string2 == NULL return false;

    returns:
    - true: bool -> the Strings are identical
    - false: bool -> One Parameter is NULL or they are not identical
*/
bool vera_utils_str_cmp(const char* string1, const char* string2) {
    if (string1 == NULL || string2 == NULL) {
        return false;
    }
    while (*string1 == *string2) {
        if (*string1 == '\0' && *string2 == '\0') {
            return true;
        }
        ++string1;
        ++string2;
    }
    return false;
}

/*
    Looks if the String has a part of `has_part` in itself

    params:
    - (const) (ptr) string: char -> The String we want to check
    - (const) (ptr) has_part: char -> The Part we want to see if its in the String

    params check:
    - if string1 or string2 == NULL return false;

    returns:
    - true: bool -> String contains `has_part`
    - false: bool -> One Parameter is NULL or String does not contain `has_part`
*/
bool vera_utils_str_has(const char* string, const char* has_part) {
    if (string == NULL || has_part == NULL) {
        return false;
    }
    const char* temp_String = has_part;
    while (*string) {
        if (*temp_String == '\0') {
            return true;
        }
        else if (*string == *temp_String) {
            ++temp_String;
        }
        else {
            temp_String = has_part;
        }
        ++string;
    }
    if (*temp_String == '\0') {
        return true;
    }
    return false;
}


// Byte Swaps

/*
    Swaps the An 64-Bit Integer from Big Endian to Little Endin

    params:
    - value: uint64_t -> The Value in Big Endian we want to swap

    return:
    - X: uint64_t -> The 64-Bit Integer in Little Endian
*/
uint64_t vera_utils_swap_endian64(uint64_t value) {
    uint32_t low = (uint32_t)(value & 0xFFFFFFFFu);
    uint32_t high = (uint32_t)(value >> 32);
    // Swap bytes within each 32-bit half
    uint32_t low_swapped = ((low & 0x000000FFu) << 24) |
                           ((low & 0x0000FF00u) << 8)  |
                           ((low & 0x00FF0000u) >> 8)  |
                           ((low & 0xFF000000u) >> 24);
    uint32_t high_swapped = ((high & 0x000000FFu) << 24) |
                            ((high & 0x0000FF00u) << 8)  |
                            ((high & 0x00FF0000u) >> 8)  |
                            ((high & 0xFF000000u) >> 24);
    // Combine halves with proper 64-bit casting to avoid shifting a 32-bit value by 32
    return ((uint64_t)low_swapped << 32) | (uint64_t)high_swapped;
}

/*
    Swaps the An 32-Bit Integer from Big Endian to Little Endin

    params:
    - value: uint32_t -> The Value in Big Endian we want to swap

    return:
    - X: uint32_t -> The 32-Bit Integer in Little Endian
*/
uint32_t vera_utils_swap_endian32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0xFF000000u) >> 24);
}


// Align

/*
    Aligns the value to the byte_align, that is always a 2 Potenz

    params:
    - value: uint64_t -> The Value we want to align
    - byte_align: uint16_t -> The Value we want to align to, example: 2, 4, 8, 16, 32, etc.

    return:
    - X: uint64_t -> `value` aligned to the next `byte_align`
*/
inline uint64_t vera_utils_align(uint64_t value, uint16_t byte_align) {
    // Addiert (Alignment - 1) und maskiert dann die unteren Bits weg
    return (value + byte_align - 1) & ~((uint64_t)byte_align - 1);
}


// Memory

/*
    Sets Memory on byte at the time into the Buffer with a set Value

    params:
    - (ptr) buffer: uint8_t -> The Buffer were we want to set the Bytes to a specifig value
    - value: uint8_t -> The Value we want to set at each byte in the buffer
    - buffer_size: uint64_t -> The size of the buffer we want to write

    params check:
    - if buffer == NULL -> return;
*/
void vera_utils_mem_set(uint8_t* buffer, uint8_t value, uint64_t buffer_size) {
    if (buffer == NULL) {
        return;
    }
    while (buffer_size) {
        *buffer = value;
        ++buffer;
        --buffer_size;
    }
}

/*
    Sets Memory on 8 bytes at the time into the Buffer with a set Value

    params:
    - (ptr) buffer: uint64_t -> The Buffer were we want to set the Bytes to a specifig value
    - value: uint64_t -> The Value we want to set at each 8 bytes in the buffer
    - buffer_size: uint64_t -> The size of the buffer we want to write devided by 8

    params check:
    - if buffer == NULL -> return;
*/
void vera_utils_mem_set_64(uint64_t* buffer, uint64_t value, uint64_t buffer_size) {
    if (buffer == NULL) {
        return;
    }
    buffer_size /= 8;
    while (buffer_size) {
        *buffer = value;
        ++buffer;
        --buffer_size;
    }
}

/*
    Copy from one Buffer to another Buffer one byte at the time

    params:
    - (ptr) buffer: uint8_t -> The Empty buffer we want to fill the Values from another Buffer
    - (ptr) ptr_cpy: uint8_t -> The Buffer we take the values from
    - buffer_size: uint64_t -> The buffer size of both bothers (Must be equal or `buffer` has to be the smaller buffer and buffer size takes the buffer size

    params check:
    - if buffer or ptr_cpy == NULL -> return;
*/
void vera_utils_mem_cpy(uint8_t* buffer, const uint8_t* ptr_cpy, uint64_t buffer_size) {
    if (buffer == NULL || ptr_cpy == NULL) {
        return;
    }
    while (buffer_size) {
        *buffer = *ptr_cpy;
        ++buffer;
        ++ptr_cpy;
        --buffer_size;
    }
}

/*
    Copy from one Buffer to another Buffer 8 byte at the time

    params:
    - (ptr) buffer: uint64_t -> The Empty buffer we want to fill the Values from another Buffer
    - (ptr) ptr_cpy: uint64_t -> The Buffer we take the values from
    - buffer_size: uint64_t -> The buffer size of both bothers (Must be equal or `buffer` has to be the smaller buffer and buffer size takes the buffer size) divided by 8

    params check:
    - if buffer or ptr_cpy == NULL -> return;
*/
void vera_utils_mem_cpy64(uint64_t* buffer, const uint64_t* ptr_cpy, uint64_t buffer_size) {
    if (buffer == NULL || ptr_cpy == NULL) {
        return;
    }
    buffer_size /= 8;
    while (buffer_size) {
        *buffer = *ptr_cpy;
        ++buffer;
        ++ptr_cpy;
        --buffer_size;
    }
}

/*
    Compare both Buffers one byte at the time if they are equal

    params:
    - (ptr) buffer1: uint8_t -> The first buffer
    - (ptr) buffer2: uint8_t -> The Second buffer
    - buffer_size: uint64_t -> The buffer size of both bothers (Must be equal)

    params check:
    - if buffer1 or buffer2 is NULL or buffer_size == 0 -> return;

    return:
    - true: bool -> if both buffer are identical
    - false: bool -> if both buffer are not identical or param check fails
*/
bool vera_utils_mem_cmp(const uint8_t* buffer1, const uint8_t* buffer2, uint64_t buffer_size) {
    if (buffer1 == NULL || buffer2 == NULL || buffer_size == 0) {
        return false;
    }
    while (*buffer1 == *buffer2) {
        ++buffer1;
        ++buffer2;
        --buffer_size;
        if (buffer_size == 0) {
            return true;
        }
    }
    return false;
}

/*
    Compare both Buffers 8 byte at the time if they are equal

    params:
    - (ptr) buffer1: uint64_t -> The first buffer
    - (ptr) buffer2: uint64_t -> The Second buffer
    - buffer_size: uint64_t -> The buffer size of both bothers (Must be equal)

    params check:
    - if buffer1 or buffer2 is NULL or buffer_size == 0 -> return;

    return:
    - true: bool -> if both buffer are identical
    - false: bool -> if both buffer are not identical or param check fails
*/
bool vera_utils_mem_cmp64(const uint64_t* buffer1, const uint64_t* buffer2, uint64_t buffer_size) {
    if (buffer1 == NULL || buffer2 == NULL || (buffer_size == 0 && buffer_size >= 8)) {
        return false;
    }
    buffer_size /= 8;
    while (*buffer1 == *buffer2) {
        ++buffer1;
        ++buffer2;
        --buffer_size;
        if (buffer_size == 0) {
            return true;
        }
    }
    return false;
}

// Calculate

/*
    Calculates the Space needed for Process X Page Tables

    params:
    - needed_bytes: uint64_t -> the bytes needed, the size of Process X in memory
    - v_address_start: uint64_t -> The start Virtuell Address

    return:
    - X: uint64_t -> The Space in bytes what the Process X Page tables need
*/
uint64_t vera_utils_calc_space(uint64_t needed_bytes, uint64_t v_address_start) {
    uint64_t result = 0;
    uint64_t pages_required = needed_bytes;
    pages_required /= Page_Table_Size;
    uint32_t padding_pages = needed_bytes % Page_Size;
    if (padding_pages) {
        ++pages_required;
    }
    if (pages_required == 0) {
        return 0;
    }

    // Root Table added
    result = Page_Table_Size;
    bool needs_PTE_1 = true;
    bool needs_PTE_0 = true;
    while (pages_required) {
        // Make an PTE if needed on Root Table for Page Table 1
        uint16_t vpn_1 = (uint16_t)((uint64_t)v_address_start >> 21) & 0x1ff;
        uint16_t vpn_0 = (uint16_t)((uint64_t)v_address_start >> 12) & 0x1ff;
        if (needs_PTE_1) {
            needs_PTE_1 = false;
            result += Page_Table_Size;
        }
        // Make an PTE in Page Table 1 for Page Table 0
        if (needs_PTE_0) {
            needs_PTE_0 = false;
            result += Page_Table_Size;
        }
        uint64_t pages = pages_required;
        for (uint16_t i = vpn_0; 0 < pages; ++i) {
            if (i > 511 || pages_required <= 0) {
                needs_PTE_0 = true;
                break;
            }
            v_address_start += 0x1000; 
            --pages_required;
        }
        if (vpn_1 >= 511) {
            needs_PTE_1 = true;
        }
    }
    return result;
}

