# Paging.c/h Reason to live
Its an Important part of Vera to manage Isolation for User Processes and ensure also its own Memory safety.


# What does paging.c?
The Paging Subsystem (from now on paging) initialises and adds new entries to a Page Table (Beginning with the Root Table) and editing Page Tabel Entry Flags. Also for Optimization we try to reduce the amount of PTE we neet to a Minimum, there for we have bigger Pages, but at one Cost, swapping and Decompressing would be harder, for this we need a splitting function that splits the pages again and there for also makes out of a 2MiB Pages pointing to a Level 0 PT with Full PTE.


### Problem Solving for Splitting
For this we need a structure to make sure that on such cases we don't get Memory Fragmentation, my latest setup made so it does take linear because everything was 4KiB Page, but now we need a bigger and performant structure that allows to fill Memory Holes, and if needed to reconstruct the entire thing.

But how?




# Includes
What does it included and why?

```c
#include "utils.h"  // Here are all utils used across Vera
```