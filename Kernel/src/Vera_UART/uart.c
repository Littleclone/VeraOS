/* src/Vera_UART/uart.c */

#include "../header/Vera_UART/uart.h"
#include <stdarg.h>
#include "../header/Vera_Memory/allocator.h"
#include "../header/Vera_Device_Driver/driver_support.h"
#include "../header/Vera_UART/driver/16550.h"

/*
    The Overlay for the UART Driver

    functions:
    - void (*uart_putc)(char character); -> Puts (Prints) a character into the UART
*/
typedef struct
{
    void (*uart_putc)(char character); // Uart_Putc Character
} vera_UART_driver; // <-- The Overlay of UART

// The Driver this Overlay will Use to write
static vera_UART_driver *P_overlay = NULL;

// Deklarationen / Prototypen

#if INCLUDE_DEBUG && INCLUDE_QEMU
static void uart_putc(char character);
#endif

static void uart_print_integer(char **str, va_list *ap);

/*
    Initializes the Driver for the UART_Overlay to use, there for Vera don't need to know exactly what UART Driver
    is loaded and has to be used. Only one Driver can take the place as the Overlay, the first one wins.


    function Description:
    - Checks if all Parameters given are valid and no Overlay/Driver is already initialised. After it will
    allocated a new `very_UART_driver`, the overlay, with the given `driver_ID`by the DTB we look
    what Driver we want to initialise, we enter the Initialise Branch of the specific UART Driver
    and cast `node_info`into the struct the specifig Driver needs to initialise.

    params:
    - (ptr) node_info -> All the Information from the Node that the Driver Will need to function properly.
    - driver_ID -> The Driver ID that is prepared from the DTB To be Initialized.

    return:
    - K_OK -> Driver already initialized or everything was fine.

    panic:
    - K_ERR_NULL_PTR -> If no valid parameter is given or Allocation failed
    - status -> the State given by the Driver functions for the specific Driver
*/
vera_state vera_uart_init_driver(void *node_info, uint32_t driver_ID)
{
#if INCLUDE_DEBUG
    vera_uart_printf("Enter uart_init_driver with: %ixX\n", driver_ID);
#endif

    // Check if
    if (node_info == NULL || driver_ID == 0)
    {
        vera_err_boot_panic(VERA_ERR_NULL_PTR);
    }
    // One Driver is already initiliazed.
    if (P_overlay != NULL)
    {
        return VERA_OK;
    }

    vera_UART_driver *driver = (vera_UART_driver *)k_malloc(sizeof(vera_UART_driver));
    if (driver == NULL)
    {
        vera_err_boot_panic(VERA_ERR_NULL_PTR);
    }

    vera_state status = VERA_OK;

    if (driver_ID == UART_16550_ID)
    {
        status = uart_16550_driver_init(node_info);
        if (status != VERA_OK)
        {
            vera_err_boot_panic(status);
        }
        driver->uart_putc = uart_16550_putc;
        P_overlay = driver;
    }

    // Real Driver Check

#if INCLUDE_DEBUG && INCLUDE_QEMU
    vera_uart_print("Leave uart_init_driver\n");
#endif
    return status;
}

/*
    Prints into the UART the given string out.

    function description:
    - The function will check if the pointer is NULL, if not it continues with itterating through the String
    until it hits the Terminating Byte ('\0'), if the Overlay is not initialised and its not Debug Mode In QEMU,
    then it will return directly without doing anything.


    params:
    - (const) (ptr) str: char -> The Plain String we want to print out via UART.

    param checks:
    - str != NULL if false -> return;
*/
void vera_uart_print(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    while (*str)
    {
        if (P_overlay != NULL)
        {
            P_overlay->uart_putc(*str++);
        }
#if INCLUDE_QEMU && INCLUDE_DEBUG
        else
        {
            uart_putc(*str++);
        }
#else
        else
        {
            return;
        }
#endif
    }
    return;
}

/*
    Prints the given string out into the UART, with a Parser that allows dynamic values to be parsed into the string on runtime

    function description:
    - checks if the pointer is null, if not it continues. It takes the va_list with the dynamic arguments list for every parsing.
    it goes through the entire string until it hits the Null Terminater Byte ('\0') and parsed all (if used) formaters, helpfull
    to make dynamic variables printable and there for visible for me (the Kernel Dev, Endmin_/Littleclone) to Debug.


    params:
    - (const) (ptr) str: char -> The String with formaters we want to print out, this needs to be parsed on runtime.
    - ...: va_list -> Variable Argument List of any type. (If not bigger then 32-Bit, it gets promoted to 32-Bits)

    param checks:
    - str != NULL if false -> return;

    checks:
    - P_overlay != NULL, if false -> return;

    format identifyer:
    - %% -> Prints an '%'
    - %c -> Prints an Char
    - %s -> Prints an String
    - %p -> Prints an Pointer value in format(%ilx)
    - %i -> Prints an 32-Bit Integer
    - %il -> Prints an 64-Bit Integer
    - %is -> Prints an Signed 32-Bit Integer
    - %ils -> Prints an Signed 64-Bit Integer
    - %ix -> Prints an 32-Bit Integer as small Hex
    - %iX -> Prints an 32-Bit Integer as Big Hex
    - %ilx -> Prints an 64-Bit Integer as small Hex
    - %ilX -> Prints an 64-Bit Integer as Big Hex
    - %isx -> Prints an signed 32-Bit Integer as small Hex
    - %isX -> Prints an signed 32-Bit Integer as Big Hex
    - %ilsx -> Prints an Signed 64-Bit Integer as small Hex
    - %ilsX -> Prints an Signed 64-Bit Integer as Big Hex
    - %ixX -> Prints an 32-Bit Integer as small Hex, starting with the First 0 From Left Beginning
    - %ilxX -> Prints an 64-Bit Integer as small Hex, starting with the first 0 From Left Beginning
    - %isxX -> Prints an Signed 32-Bit Integer as small Hex, starting with the First 0 From Left Beginning
    - %ilsxX -> Prints an Signed 64-Bit Integer as small Hex, starting with the first 0 From Left Beginning
*/
void vera_uart_printf(char *str, ...)
{
    if (str == NULL)
    {
        return;
    }
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
    if (P_overlay == NULL)
    {
        return;
    }
#endif
    // The Dynamic Paramter List
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
                if (P_overlay != NULL)
                {
                    P_overlay->uart_putc(*str);
                }
#if INCLUDE_QEMU && INCLUDE_DEBUG
                else
                {
                    uart_putc(*str);
                }
#endif
                ++str;
                break;
            case 'i':
                uart_print_integer(&str, &ap);
                break;
            case 'p':
                char *tempStr = "ilx"; // A Hacky trick to save code and reuse already implemented functionality
                uart_print_integer(&tempStr, &ap);
                ++str;
                break;
            case 'c':
                // va_arg gets always promoted to int (32-Bit)
                uint32_t temp = va_arg(ap, uint32_t);
                // get the char
                int8_t c = (int8_t)temp;
                if (P_overlay != NULL)
                {
                    P_overlay->uart_putc(c);
                }
#if INCLUDE_QEMU && INCLUDE_DEBUG
                else
                {
                    uart_putc(c);
                }
#endif
                ++str;
                break;
            case 's':
                char *string = va_arg(ap, char *);
                ++str;
                vera_uart_print(string);
                break;
            default:
                break;
            }
        }
        else
        {
            if (P_overlay != NULL)
            {
                P_overlay->uart_putc(*str);
            }
#if INCLUDE_QEMU
            else
            {
                uart_putc(*str);
            }
#endif
            ++str;
        }
    }
    return;
}

#if INCLUDE_QEMU && INCLUDE_DEBUG
/*
    Only here if `#if INCLUDE_QEMU && INCLUDE_DEBUG` is active, it put a char into the QEMU UART

    function description:
    - Gets the UART base address from QEMU and checks if its ready to get the next Character, if yes
    we save into the first MMIO Register the character.

    params:
    - character: char -> the character we want to print

*/
static void uart_putc(char character)
{
    volatile uint8_t *thr = (uint8_t *)(UART_BASE_QEMU + 0);
    volatile uint8_t *lsr = (uint8_t *)(UART_BASE_QEMU + 5);
    while (((*lsr) & 0x20) == 0)
    {
    }
    *thr = character;
}
#endif

/*
    The Part from `vera_uart_printf` that parses the Integer values into a String, given by our Format.

    function description:
    - It looks first after the formaters `l` and `s` to get the number out of the va_list, after it it shows if there are
    the formaters `xX`, `x` or `X` or no Formater and then prints the string accordingly.

    param:
    - (ptr)(ptr) str: char -> The Pointer of the String we iterrate through
    - (ptr) ap: va_list -> The Arguments we needs to parse the Integer variable
*/
static void uart_print_integer(char **str, va_list *ap)
{
    static const char H[] = "0123456789ABCDEF";
    static const char h[] = "0123456789abcdef";
    uint8_t buffer[32];
    // The Number will be saved here we want to print
    uint64_t working_number = 0;
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
                if (P_overlay != NULL)
                {
                    P_overlay->uart_putc('-');
                }
#if INCLUDE_DEBUG && INCLUDE_QEMU
                else
                {
                    uart_putc('-');
                }
#endif
                working_number = (uint64_t)(-(numberSigned + 1) + 1);
            }
            // if (numberSigned < 0)
            // {
            //     working_number = (uint64_t)(-(numberSigned + 1) + 1);
            // }
            else
            {
                working_number = (uint64_t)numberSigned;
            }
            /* Das Obere ist das Leserlichere von unten! */
            // number32 = (number32Signed < 0) ? (uint32_t)(-(number32Signed + 1)) + 1 : (uint32_t)number32Signed;
        }
        else
        {
            working_number = (uint64_t)numberSigned;
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
                if (P_overlay != NULL)
                {
                    P_overlay->uart_putc('-');
                }
#if INCLUDE_DEBUG && INCLUDE_QEMU
                else
                {

                    uart_putc('-');
                }
#endif
                number32 = (uint32_t)(-(number32Signed + 1) + 1);
            }
            // if (number32Signed < 0)
            // {
            //     number32 = (uint32_t)(-(number32Signed + 1) + 1);
            // }
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
        working_number = (uint64_t)number32;
    }
    if (**str == 'x' && *(*str + 1) == 'X')
    {
        ++*str;
        ++*str;
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
        // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
        P_overlay->uart_putc('0');
        P_overlay->uart_putc('x');
#else
        if (P_overlay != NULL)
        {
            P_overlay->uart_putc('0');
            P_overlay->uart_putc('x');
        }
        else
        {
            uart_putc('0');
            uart_putc('x');
        }
#endif
        if (working_number == 0)
        {
            if (P_overlay != NULL)
            {
                P_overlay->uart_putc('0');
            }
#if INCLUDE_DEBUG && INCLUDE_QEMU
            else
            {
                uart_putc('0');
            }
#endif
            return;
        }
        bool first_num = false;
        for (int16_t i = 60; i >= 0; i -= 4)
        {
            char temp_char = (char)h[(working_number >> i) & 0xF];
            if (temp_char != '0')
            {
                first_num = true;
            }
            else if (!first_num)
            {
                continue;
            }
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
            // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
            P_overlay->uart_putc(temp_char);
#else
            if (P_overlay != NULL)
            {
                P_overlay->uart_putc(temp_char);
            }
            else
            {
                uart_putc(temp_char);
            }
#endif
        }
    }
    else if (**str == 'x')
    {
        ++*str;
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
        // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
        P_overlay->uart_putc('0');
        P_overlay->uart_putc('x');
#else
        if (P_overlay != NULL)
        {
            P_overlay->uart_putc('0');
            P_overlay->uart_putc('x');
        }
        else
        {
            uart_putc('0');
            uart_putc('x');
        }
#endif
        for (int16_t i = 60; i >= 0; i -= 4)
        {
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
            // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
            P_overlay->uart_putc((h[(working_number >> i) & 0xF]));
#else
            if (P_overlay != NULL)
            {
                P_overlay->uart_putc((h[(working_number >> i) & 0xF]));
            }
            else
            {
                uart_putc(h[(working_number >> i) & 0xF]);
            }
#endif
        }
    }
    else if (**str == 'X')
    {
        ++*str;
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
        // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
        P_overlay->uart_putc('0');
        P_overlay->uart_putc('x');
#else
        if (P_overlay != NULL)
        {
            P_overlay->uart_putc('0');
            P_overlay->uart_putc('x');
        }
        else
        {
            uart_putc('0');
            uart_putc('x');
        }
#endif
        for (int16_t i = 60; i >= 0; i -= 4)
        {
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
            // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
            P_overlay->uart_putc((h[(working_number >> i) & 0xF]));
#else
            if (P_overlay != NULL)
            {
                P_overlay->uart_putc((h[(working_number >> i) & 0xF]));
            }
            else
            {
                uart_putc(h[(working_number >> i) & 0xF]);
            }
#endif
        }
    }
    else
    {
        if (working_number == 0)
        {
            if (P_overlay != NULL)
            {
                P_overlay->uart_putc('0');
            }
#if INCLUDE_DEBUG && INCLUDE_QEMU
            else
            {
                uart_putc('0');
            }
#endif
        }
        else
        {
            while (working_number != 0)
            {
                uint64_t tempNumber = working_number % 10;
                working_number /= 10;
                buffer[counter] = (uint8_t)tempNumber;
                ++counter;
            }
            --counter;
            for (; counter >= 0; --counter)
            {
#if !INCLUDE_DEBUG && !INCLUDE_QEMU
                // We know that here the Overlay exists because printf looks under the same condition if the overlay is set
                P_overlay->uart_putc((buffer[counter] + '0'));
#else
                if (P_overlay != NULL)
                {
                    P_overlay->uart_putc((buffer[counter] + '0'));
                }
                else
                {
                    uart_putc(buffer[counter] + '0');
                }
#endif
            }
        }
    }
}