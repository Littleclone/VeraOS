# C Macro Cheat Sheet — MaidCore/HomeOS

> Practical, kernel‑friendly macros with clear rules for when to use them. Keep this next to your editor.

---
## 0) Philosophy — When to use macros
**Prefer `static inline` functions** when you can (type safety, better debug). Use **macros** when you need:
- Compile‑time constants or feature toggles (`#if`, `#ifdef`).
- Conditional compilation across platforms/arch.
- Bit twiddling helpers that must be usable in constant expressions.
- Stringification (`#`) or token pasting (`##`).
- Variadic logging wrappers that must disappear in release builds.
- Portable attributes/annotations via defines.

Avoid macros when:
- A function would be clearer/safer.
- Arguments have side effects (macros evaluate them multiple times!).
- You rely on types (macros are typeless).

**Rule of thumb:** *API = function*, *config/compile‑time tricks = macro*.

---
## 1) Basics
```c
#define BUF_SIZE 4096            // object‑like macro
#define ADD(a,b) ((a) + (b))     // function‑like macro — parenthesize!
#define STR "hello"             // strings okay
```
**Parenthesize** macro parameters and the whole expansion to avoid precedence bugs.

Undefine if needed:
```c
#undef ADD
```

---
## 2) Conditional compilation
```c
#if defined(CONFIG_DEBUG)
  #define DEBUG 1
#else
  #define DEBUG 0
#endif

#ifdef RISCV
  /* RISC‑V specific */
#endif

#ifndef NDEBUG
  /* assertions enabled */
#endif
```

Feature flags usually come from compiler `-DNAME` switches or a central config header.

---
## 3) Stringification `#` and token pasting `##`
```c
#define TO_STR(x) #x
#define EXPAND_AND_STR(x) TO_STR(x)

#define CONCAT(a,b) a##b
#define MK_REG(name, idx) CONCAT(name, idx)

// Usage
const char *s = EXPAND_AND_STR(0x10000000);   // "0x10000000"
int MK_REG(uart_reg_, 5);                     // int uart_reg_5;
```

`#` turns tokens into strings. `##` glues tokens together (after macro expansion).

---
## 4) Variadic macros (`...`)
```c
#define LOG(fmt, ...) kprintf("[LOG] " fmt, ##__VA_ARGS__)
#define DBG(fmt, ...) do { if (DEBUG) kprintf("[DBG] " fmt, ##__VA_ARGS__); } while (0)
```
`##__VA_ARGS__` allows calling with zero extra args: `LOG("hi")`.

---
## 5) Safe multi‑statement macros
```c
#define DO_ONCE(stmt) do { stmt; } while (0)
#define SWAP(a,b) do { __auto_type _t = (a); (a)=(b); (b)=_t; } while (0) // GCC/Clang
```
`do { ... } while (0)` makes a macro behave like a single statement (safe in `if/else`).

---
## 6) Bit helpers (kernel staple)
```c
#define BIT(n)              (1u  << (n))
#define BIT64(n)            (1ull<< (n))
#define MASK(width)         ((1u << (width)) - 1u)
#define FIELD_MASK(w, s)    (MASK(w) << (s))
#define FIELD_PREP(val, s)  ((uint32_t)(val) << (s))
#define FIELD_GET(x, s, w)  (((x) >> (s)) & MASK(w))
```
**Usage**
```c
if (lsr & BIT(5)) { /* THRE set */ }
uint32_t len = FIELD_GET(ctrl, 8, 3);    // width=3 at shift=8
ctrl |= FIELD_PREP(5, 8);                // write value 5 at field shift 8
```

---
## 7) Compile‑time checks
```c
_Static_assert(sizeof(void*)==8, "Kernel requires 64‑bit"); // C11

#define BUILD_BUG_ON(cond) _Static_assert(!(cond), "build bug")
BUILD_BUG_ON(sizeof(long)!=8);
```

---
## 8) Attributes and branch hints (portable wrappers)
```c
#if defined(__GNUC__)
  #define NOINLINE   __attribute__((noinline))
  #define PACKED     __attribute__((packed))
  #define ALIGNED(x) __attribute__((aligned(x)))
  #define LIKELY(x)   __builtin_expect(!!(x), 1)
  #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
  #define NOINLINE
  #define PACKED
  #define ALIGNED(x)
  #define LIKELY(x)   (x)
  #define UNLIKELY(x) (x)
#endif
```

---
## 9) MMIO helpers — prefer `static inline`, not macros
```c
static inline uint8_t  mmio_read8 (uintptr_t a){ return *(volatile uint8_t *)a; }
static inline void     mmio_write8(uintptr_t a, uint8_t v){ *(volatile uint8_t *)a = v; }
static inline uint32_t mmio_read32(uintptr_t a){ return *(volatile uint32_t*)a; }
static inline void     mmio_write32(uintptr_t a, uint32_t v){ *(volatile uint32_t*)a = v; }
```
If you **must** macro:
```c
#define REG8(base,off)   (*(volatile uint8_t *) ((uintptr_t)(base)+(off)))
#define REG32(base,off)  (*(volatile uint32_t*) ((uintptr_t)(base)+(off)))
```
Beware of double evaluation if `base`/`off` are expressions.

---
## 10) Error‑handling macros (your style)
```c
typedef enum { K_OK=0, K_ERR=-1, K_ERR_INVAL=-2, K_ERR_NOMEM=-3, K_ERR_IO=-4 } k_status;

#define K_SUCCEEDED(x)    ((x) >= 0)
#define K_FAILED(x)       ((x) < 0)

#define K_RETURN_IF_FAILED(expr)                 \
    do { k_status _s = (expr);                   \
         if (K_FAILED(_s)) return _s; } while (0)

#define K_EXPECT(cond, errcode)                  \
    do { if (!(cond)) return (errcode); } while (0)
```

---
## 11) Generic macros with `_Generic` (C11 type dispatch)
```c
#define is_signed(T) _Generic(((T)0),              \
    signed char:1, short:1, int:1, long:1, long long:1, default:0)

#define ABS(x) _Generic((x),                       \
    int: abs, long: labs, long long: llabs,       \
    default: fabs)(x)   // requires <math.h> for floating types
```
Useful but keep it simple in kernel start; prefer overloads via different names.

---
## 12) Include guards & exporting
```c
#ifndef UART_H
#define UART_H
/* declarations */
#endif
```
Prefer `#pragma once` if your toolchain allows it (GCC/Clang: yes).

Public API goes in headers; implementation remains in `.c`. Keep **internal** helpers `static` in `.c`.

---
## 13) Debug macros
```c
#ifdef CONFIG_DEBUG
  #define debug_hexdump(buf,len)  debug_do_hexdump((buf),(len))
#else
  #define debug_hexdump(buf,len)  do{}while(0)
#endif
```
Compile‑time removable logging. Keep all debug symbol names prefixed with `debug_` (your rule).

---
## 14) Macro pitfalls (read this!)
- **Multiple evaluation**: `MAX(a++, b++)` increments twice — avoid side‑effects in args.
- **Missing parentheses**: `#define SQR(x) x*x` breaks on `SQR(1+2)` → becomes `1+2*1+2`.
- **Typeless**: no type checks; prefer inline for typed operations.
- **Scope**: macros ignore C scopes; name‑clash → use clear, UPPERCASE names for constants.

---
## 15) Minimal kernel macro kit (recommended)
- `BIT`, `BIT64`, `MASK`, `FIELD_*`
- `BUILD_BUG_ON`, `_Static_assert`
- `LIKELY/UNLIKELY`
- `K_*` error helpers
- Optional: `REG8/REG32` (or better inline mmio)

Keep it small. Expand only when a real pattern appears.

---
## 16) Style for macro names
- **Constants / flags**: `UPPER_SNAKE_CASE` (`UART_BASE`, `LSR_THRE`)
- **Function‑like**: `lower_snake_case` if inline functions, otherwise `CAPS` if true macro (`FIELD_PREP`)
- **Debug toggles**: `CONFIG_DEBUG`, `CONFIG_TRACE_UART`

---
## 17) Examples tied to UART (QEMU virt)
```c
#define UART_BASE  0x10000000UL
#define UART_THR   0x00
#define UART_LSR   0x05
#define LSR_THRE   BIT(5)

static inline int uart_tx_ready(void){
    return (mmio_read8(UART_BASE + UART_LSR) & LSR_THRE) != 0;
}
```

---
## 18) When to refactor a macro into a function
- You start adding types or branching → **inline function**
- You need a stable address for taking function pointers → **function**
- You want breakpoints/stack traces → **function**

---
## 19) Checklist before adding a macro
1. Muss es *zur Compile‑Zeit* existieren?
2. Brauche ich `#` oder `##`?
3. Kann es ein side‑effect‑freier 1‑Zeiler sein?
4. Könnte ein `static inline` klarer/sicherer sein?
5. Passt Name zum **HomeOS/MaidCore Style‑Guide**?

---
**Keep it sharp, keep it small.**

