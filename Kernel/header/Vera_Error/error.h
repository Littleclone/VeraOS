/* header/Vera_Error/error.h */
#pragma once

#include "../Vera_Utils/utils.h"

/* Rückgabestandard:
   VERA_OK (0) = Erfolg
   < 0       = Fehlercode (negativ)
   > 0       = optionale „Erfolg mit Zusatzinfo“ (selten nutzen) */
typedef enum {
    VERA_OK         = 0,

    /* generisch */
    VERA_ERR        = -1,   /* unspezifisch */
    VERA_ERR_INVAL  = -2,   /* ungültiges Argument */
    VERA_ERR_NOMEM  = -3,   /* Speicher fehlt */
    VERA_ERR_IO     = -4,   /* I/O-Fehler */
    VERA_ERR_BUSY   = -5,   /* belegt/noch nicht bereit */
    VERA_ERR_TIMEOUT= -6,   /* Zeit überschritten */
    VERA_ERR_NOSUP  = -7,   /* nicht unterstützt */
    VERA_ERR_PERM   = -8,   /* keine Berechtigung */
    VERA_ERR_NULL_PTR = -9, /* Pointer war NULL*/
    VERA_TRAP       = -10,  /* Trap is ausgelöst */
    VERA_OVERFLOW   = -11,  /* Ein Overflow trat auf */
    VERA_ERR_INVAL_BOOT_ID = -12,  /* Invalid Boot hart ID */
} vera_state;

/* Kurzhelfer – ergonomische Checks */
#define VERA_SUCCEEDED(x)    ((x) >= 0)
#define VERA_FAILED(x)       ((x) < 0)

/* Früh raus, wenn Unteraufruf scheitert */
#define VERA_RETURN_IF_FAILED(expr)               \
    do { vera_state _s = (expr);                 \
         if (VERA_FAILED(_s)) return _s; } while (0)

/* Bedingung erzwingen, sonst Fehler zurückgeben (kein Panic) */
/* Beispiel: if (ptr == NULL)*/
#define VERA_EXPECT(cond, errcode)                \
    do { if (!(cond)) return (errcode); } while (0)

void vera_err_boot_panic(vera_state status);
