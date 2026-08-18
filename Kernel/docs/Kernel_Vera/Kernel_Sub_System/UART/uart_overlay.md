# UART.c/h Reason to live
UART.c/h (from now on uart) allows Vera to print strings via UART out, helpfull on Boot Phase or if Vera can't draw do to an error in the framebuffer (Display). Especially helpfull for Debug

# Driver
It has officaly right now only the ns16550a, the Driver struct what is used works as an Overlay, allowing Vera to use the print functions without having to know what UART Driver is initialised.

# Functions and structs

In the following will show the Structs and Function Protypes with they uart.c source file comments

```c
/*
    The Overlay for the UART Driver

    functions:
    - void (*uart_putc)(char character); -> Puts (Prints) a character into the UART
*/
typedef struct
{
    void (*uart_putc)(char character); // Uart_Putc Character
} vera_UART_driver; // <-- The Overlay of UART


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
vera_state vera_uart_init_driver(void *node_info, uint32_t driver_ID);

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
void vera_uart_print(const char *str);

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
void vera_uart_printf(const char *str, ...);

#if INCLUDE_QEMU && INCLUDE_DEBUG
/*
    Only here if `#if INCLUDE_QEMU && INCLUDE_DEBUG` is active, it put a char into the QEMU UART

    function description:
    - Gets the UART base address from QEMU and checks if its ready to get the next Character, if yes
    we save into the first MMIO Register the character.

    params:
    - character: char -> the character we want to print

*/
static void uart_putc(char character);
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
```



# Includes
What does it included and why?

```c
#include "utils.h"  // Here are all utils used across Vera
#include <stdarg.h> // For the `va_list`
#include "../header/Vera_Memory/allocator.h" // for Allocating for the UART Overlay
#include "../header/Vera_Device_Driver/driver_support.h"   // To get the Driver Structure the DTB Sends us
#include "../header/Vera_UART/driver/16550.h"   // To initialise the driver ns16550a
```