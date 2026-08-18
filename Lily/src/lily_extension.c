/* src/lily_debug.c */
/* src/uart.c */

#include "../header/lily_extension.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void lily_utils_mem_set_64(uint64_t* buffer, const uint64_t value, size_t buffer_size) {
    if (buffer == NULL) {
        return;
    }
    while (buffer_size) {
        *buffer = value;
        ++buffer;
        --buffer_size;
    }
}


void lily_utils_mem_set(uint8_t* buffer, const uint8_t value, size_t buffer_size) {
    if (buffer == NULL) {
        return;
    }
    while (buffer_size) {
        *buffer = value;
        ++buffer;
        --buffer_size;
    }
}

uint64_t lily_utils_calc_space(const uint64_t needed_bytes, uint64_t v_address_start) {
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

bool lily_utils_str_cmp(const char* string1, const char* string2) {
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

bool lily_utils_str_has(const char* string1, const char* string2) {
    if (string1 == NULL || string2 == NULL) {
        return false;
    }
    const char* temp_String = string2;
    while (*string1) {
        if (*temp_String == '\0') {
            return true;
        }
        else if (*string1 == *temp_String) {
            ++temp_String;
        }
        else {
            temp_String = string2;
        }
        ++string1;
    }
    if (*temp_String == '\0') {
        return true;
    }
    return false;
}

inline uint64_t lily_utils_align(uint64_t value, const uint16_t byte_align) {
    while ((value % byte_align) != 0) {
        ++value;
    }
    return value;
}

uint64_t lily_utils_str_len(const char* string) {
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

uint64_t lily_utils_swap_endian64(const uint64_t value) {
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

uint32_t lily_utils_swap_endian32(const uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0xFF000000u) >> 24);
}

#if INCLUDE_DEBUG && INCLUDE_QEMU
// --- UART ---

// Deklarationen / Prototypen
static void uart_putc(char character);
static void uart_print_integer(char **str, va_list *ap);

/*
Gibt ein einfachen null Terminierten String zum UART aus.

params:
- (ptr) (const) str -> Der String den wir printen wollen

return:
- VERA_OK = Alles okay
- K_ERR_NULL_PTR = Wenn Pointer NULL ist
*/
vera_state lily_uart_print(const char *str)
{
    if (str == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    while (*str)
    {
        uart_putc(*str++);
    }
    return VERA_OK;
}

/*
Gibt ein String mit Format Zeichens ins UART aus, diese werden
mit den Values in den Variablen ersetzt und geprintet.

formats:
- Integer = %i, %il, %is, %ix/X, %ixX
- Pointer = %p
- Char = %c
- String = %s
- Prozent = %%

params:
- (ptr) str -> Der String den wir printen wollen
- ... -> Weitere Variablen anhand dessen was im string für Formate genutzt wird

return:
- VERA_OK = Alles okay
- K_ERR_NULL_PTR = Wenn Pointer NULL ist
*/
vera_state lily_uart_printf(char *str, ...)
{
    if (str == NULL)
    {
        return VERA_ERR_NULL_PTR;
    }
    va_list ap;
    va_start(ap, str);
    while (*str)
    {
        if (*str == '%')
        {
            ++str;
            switch (*str)
            {
            case '%':
                uart_putc(*str);
                ++str;
                break;
            case 'i':
                uart_print_integer(&str, &ap);
                break;
            case 'p':
                char *tempStr = "ilx"; // A bit Hacky, aber es funktioniert.
                uart_print_integer(&tempStr, &ap);
                ++str;
                break;
            case 'c':
                uint32_t temp = va_arg(ap, uint32_t);
                int8_t c = (int8_t)temp;
                uart_putc(c);
                ++str;
                break;
            case 's':
                char *string = va_arg(ap, char *);
                ++str;
                VERA_RETURN_IF_FAILED(lily_uart_print(string));
                break;
            default:
                break;
            }
        }
        else
        {
            uart_putc(*str);
            ++str;
        }
    }
    return VERA_OK;
}

/*
Printet ein Char durch den UART

params:
- (const) character -> Ein Character der in UART geprintet werden soll

*/
static void uart_putc(const char character)
{
    volatile uint8_t *thr = (uint8_t *)(UART_BASE_QEMU + 0);
    volatile uint8_t *lsr = (uint8_t *)(UART_BASE_QEMU + 5);
    while (((*lsr) & 0x20) == 0)
    {
    }
    *thr = character;
}

/*
Printet die Integer Variablen in Text form

formate:
- x = Hex klein
- X = Hex groß
- s = signed
- l = long
*/
static void uart_print_integer(char **str, va_list *ap)
{
    static const char H[] = "0123456789ABCDEF";
    static const char h[] = "0123456789abcdef";
    uint8_t buffer[32];
    uint64_t number = 0;
    int8_t counter = 0;
    ++*str;
    if (**str == 'l')
    {
        ++*str;
        int64_t numberSigned = va_arg(*ap, int64_t);
        if (**str == 's')
        {
            ++*str;
            if (numberSigned < 0)
            {
                uart_putc('-');
            }
            if (numberSigned < 0)
            {
                number = (uint64_t)(-(numberSigned + 1) + 1);
            }
            else
            {
                number = (uint64_t)numberSigned;
            }
            /* Das Obere ist das Leserlichere von unten! */
            // number32 = (number32Signed < 0) ? (uint32_t)(-(number32Signed + 1)) + 1 : (uint32_t)number32Signed;
        }
        else
        {
            number = (uint64_t)numberSigned;
        }
    }
    else
    {
        int32_t number32Signed = va_arg(*ap, int32_t);
        uint32_t number32 = 0;
        if (**str == 's')
        {
            ++*str;
            if (number32Signed < 0)
            {
                uart_putc('-');
            }
            if (number32Signed < 0)
            {
                number32 = (uint32_t)(-(number32Signed + 1) + 1);
            }
            else
            {
                number32 = (uint32_t)number32Signed;
            }
            /* Das Obere ist das Leserlichere von unten! */
            // number32 = (number32Signed < 0) ? (uint32_t)(-(number32Signed + 1)) + 1 : (uint32_t)number32Signed;
        }
        else
        {
            number32 = (uint32_t)number32Signed;
        }
        number = (uint64_t)number32;
    }
    if (**str == 'x' && *(*str + 1) == 'X')
    {
        ++*str;
        ++*str;
        lily_uart_print("0x");
        if (number == 0) {
            uart_putc('0');
            return;
        }
        bool first_num = false;
        for (int16_t i = 60; i >= 0; i -= 4) {
            char temp_char = (char)h[(number >> i) & 0xF];
            if (temp_char != '0') {
                first_num = true;
            }
            else if (!first_num) {
                continue;
            }
            uart_putc(temp_char);
        }
    }
    else if (**str == 'x')
    {
        ++*str;
        for (int16_t i = 60; i >= 0; i -= 4) {
            uart_putc(h[(number >> i) & 0xF]);
        }

    }
    else if (**str == 'X')
    {
        ++*str;
        for (int16_t i = 60; i >= 0; i -= 4) {
            uart_putc(H[(number >> i) & 0xF]);
        }
    }
    else
    {
        if (number == 0)
        {
            uart_putc('0');
        }
        else
        {
            while (number != 0)
            {
                uint64_t tempNumber = number % 10;
                number /= 10;
                buffer[counter] = (uint8_t)tempNumber;
                ++counter;
            }
            --counter;
            for (; counter >= 0; --counter)
            {
                uart_putc(buffer[counter] + '0');
                
            }
        }
    }
}

// --- Error ---
/*
Gibt den String zum error_code fürs printen zurück
*/
static const char* lily_status_str(const vera_state status) {
    switch (status) {
    case VERA_OK:          return "VERA_OK";
    case VERA_ERR:         return "VERA_ERR";
    case VERA_ERR_INVAL:   return "K_ERR_INVAL";
    case VERA_ERR_NOMEM:   return "VERA_ERR_NOMEM";
    case VERA_ERR_IO:      return "VERA_ERR_IO";
    case VERA_ERR_BUSY:    return "VERA_ERR_BUSY";
    case VERA_ERR_TIMEOUT: return "VERA_ERR_TIMEOUT";
    case VERA_ERR_NOSUP:   return "VERA_ERR_NOSUP";
    case VERA_ERR_PERM:    return "VERA_ERR_PERM";
    case VERA_TRAP:        return "VERA_TRAP";
    case VERA_ERR_NULL_PTR: return "K_NULL_PTR";
    case VERA_OVERFLOW:    return "VERA_OVERFLOW";
    case VERA_ERR_INVAL_BOOT_ID: return "VERA_ERR_INVAL_BOOT_ID";
    default:            return "K_ERR_UNKNOWN";
    }
}

/*
Funktion um den kernel in eine kontrollierte endlessloop zu bringen, dieser printet es in UART
dann auch den grund der Panic.

params:
- status -> den kernel status der zur panic führte
*/
void lily_panic(const vera_state status) {
    lily_uart_printf("PANIC: %s\n", lily_status_str(status));
    
    __asm__ __volatile__("csrci sstatus, 0x2");
    for(;;){ __asm__ __volatile__("wfi"); }
}

#endif
/*
Funktion um den Kernel zu Panicen, endet dann in einer Endless Loop mit Interrupts deaktiviert, schreibt 255 in a0, ein hinweis
falls UART nicht verfügbar ist
*/
void lily_panic_no_write(const vera_state s) {
    __asm__ __volatile__("addi a0, zero, 255");
    __asm__ __volatile__("csrci sstatus, 0x2");
    for(;;){ __asm__ __volatile__("wfi"); }
}

/*
Nach dem Trap Entry wird der Trap Handler aufgerufen, dieser bewertet die Trap aus

note:
- Kernel Only derzeit, außer ausgabe von Informationen passiert nicht viel, danach wird panic
*/
void trap_Handler_C_Lily(K_Trap_Register_Lily* Kernel_Register, Trap_Information_Lily* trap_infos) {
    #if INCLUDE_DEBUG && INCLUDE_QEMU
    bool IsInterrupt = (trap_infos->scause >> 63);
    uint64_t trap_cause = ((trap_infos->scause << 1) >> 1);
    lily_uart_print("\n----------------\n[Lily TRAP]:\n");
    lily_uart_printf("scause: %ix\ncurrent_mode: %i\nextra_info: %ilx\nTrapped_PC: %ilx\n----------------\n", trap_cause, (trap_infos->sstatus & 0x80), trap_infos->stval, trap_infos->sepc);
    if (IsInterrupt) {
        switch (trap_cause) {
            case trap_SPI:
                lily_uart_print("Software Interrupt\n");
                break;
            case trap_STI:
                lily_uart_print("Timer Interrupt\n");
                break;
            case trap_SEI:
                lily_uart_print("Hardware Interrupt\n");
                break;
            default:
                lily_uart_print("Error, Undefined Cause!\n");  // Sollte Nicht erreichbar sein.
                break;
        }
        lily_panic(VERA_TRAP);    // TEMP
    }
    else {
        switch (trap_cause)
        {
        case trap_Instruction_address_misaligned:
            lily_uart_print("Instruction address misaligned\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_Instruction_access_fault:
            lily_uart_print("Instruction access fault\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_Illegal_instruction:
            lily_uart_print("Illegal instruction\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_Breakpoint:
            lily_uart_print("Breakpoint\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Load_address_misaligned:
            lily_uart_print("Load Address misaligned\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_Load_access_fault:
            lily_uart_print("Load Access fault\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_StoreAMO_address_misaligned:
            lily_uart_print("Store/AMO address misaligned\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_StoreAMO_access_fault:
            lily_uart_print("Store/AMO access fault\n");
            lily_panic(VERA_TRAP);
            break;
        case trap_Environment_call_from_U_Mode:
            lily_uart_print("User Syscall. (Environment call from U-Mode)\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Environment_call_from_S_Mode:
            lily_uart_print("Kernel Syscall. (Environemt call from S-Mode\n");
            if ((trap_infos->sepc & 0b11) == 0b11) {
                trap_infos->sepc += 4;
            }
            else {
                trap_infos->sepc += 2;
            }
            break;
        case trap_Instruction_page_fault:
            lily_uart_print("Instruction page fault\n");
            break;
        case trap_Load_page_fault:
            lily_uart_print("Load page fault\n");
            break;
        case trap_StoreAMO_page_fault:
            lily_uart_print("Store/AMO page fault\n");
            break;
        default:
            // Während Entwicklung hier, normal Kill Process / Panic
            lily_panic(VERA_ERR_INVAL);
            break;
        }
    }
    lily_panic(VERA_TRAP); // TEMP
    #else
    lily_panic_no_write(VERA_TRAP);
    #endif

    return;
}