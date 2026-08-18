# Error.c/h Reason to live
Error.c/h (from now Vera_Error) ist für das simple Error State und Handling von Panics zuständig, der Most Basic Error Handler der Kernel hat, vor allem viel genutzt in Early Boot von Vera.


# Boot functions
What is a Boot Function? It's a function Vera uses in its initialising phase where Vera is not stable and doesn't need a big error handler, on boot we go with "Fail Quick", no try to rescue in the boot phase.

### The Boot functions in Vera_Error
The following will the the C Function Prototype and its in error.c source file comments (static files are not in the header file):

```c

/*
    This Functions returns the apropriate String as its Enum Counter part of `vera_state`

    Function description:
    - The Function takes the state and returns in a Switch case statement


    params:
    - (const) status: vera_state -> the State of Vera

    return:
    - (const) (ptr) string: char -> The string translation from the given State
*/
static const char* vera_err_status_str(vera_state state);

/*
    Is the boot panic function for the early boot where the kernel is still not stable and in the stabilizing booting phase

    function description:
    - If in Debug Mode prints the Panic Reason out with the Panic Prefix and then disables Interrupts complettely for this Hart 
    goes into a wfi (Wait for interrupt) that never comes.

    params:
    - (const) state: vera_state -> the State given as the reason for the Panic
*/
void vera_err_boot_panic(vera_state status);
```







# Includes
What does it included and why?

```c
#include "utils.h"  // Here are all utils used across Vera
#include "../../header/Vera_UART/uart.h"   // If the debug mode is active to let us see what the Panic was the reason
```