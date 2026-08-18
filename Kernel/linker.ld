/* linker.ld */
ENTRY(_start)

MEMORY {
  RAM (rwx) : ORIGIN = 0x80200000, LENGTH = 64M
}

SECTIONS {
  . = ORIGIN(RAM);

  __kernel_start = .;    /* Ab hier beginnt der Kernel */
  /* Code */

  __text = .;
  .text : {
    . = ALIGN(4096);
    *(.text .text.*)
    . = ALIGN(4096);
  } > RAM
  __text_end = .;

  /* Nur-Lese-Daten (Strings, Tabellen) */
  __rodata = .;
  .rodata : {
    . = ALIGN(4096);
    *(.rodata .rodata.*)
    . = ALIGN(4096);
  } > RAM 
  __rodata_end = .;

  /* Initialisierte globale Daten */
  __data = .;
  .data : {
    . = ALIGN(4096);
    *(.data .data.*)
    . = ALIGN(4096);
  } > RAM
  __data_end = .;

  /* Small data (optional, schadet nicht) */
  __sdata = .;
  .sdata : {
    . = ALIGN(4096);
    *(.sdata .sdata.*)
    . = ALIGN(4096);
  } > RAM
  __sdata_end = .;

  /* Uninitialisierte Daten + Symbole fürs BSS-Zeroing */
  __bss_start = .;
  .bss (NOLOAD) : {
    . = ALIGN(4096);
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4096);
  } > RAM
  __bss_end = .;

  __stack_guard_start = .;
  .stack_guard(NOLOAD) : {
    . = ALIGN(4096);
    *(.stack_guard .stack_guard.*)
    . = ALIGN(4096);
  } > RAM
  __stack_guard_end = .;

  __stack_start = .;
  .stack(NOLOAD) : {
    . = ALIGN(4096);
    *(.stack .stack.*)
    . = ALIGN(4096);
  } > RAM
  __stack_end = .;

  __kernel_end = .;      /* Hier hört der Kernel auf */
}