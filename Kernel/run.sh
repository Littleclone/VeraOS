#!/usr/bin/env bash
set -e
rm -rf build
mkdir -p build

# 1. Gemeinsame Basis-Flags (ohne Architektur-spezifische Dinge)
BASE_CFLAGS="-g3 -O0 -std=c17 -ffreestanding -fno-builtin -nostdlib -nostartfiles -mcmodel=medany -mabi=lp64 -Isrc -Wall -Wextra"

# 2. Architektur definieren (Die sichere Basis vs. die optimierte Version)
LILY_ISA="-march=rv64imac_zicsr_zifencei"
KERNEL_ISA="-march=rv64imac_zicsr_zifencei_zba_zbb_zbc_zbs"

# 3. Fertige CFLAGS für beide Welten zusammensetzen
LILY_CFLAGS="$BASE_CFLAGS $LILY_ISA"
KERNEL_CFLAGS="$BASE_CFLAGS $KERNEL_ISA"

# ASM
riscv64-unknown-elf-as $LILY_ISA asm/start.s       -o build/start.o
riscv64-unknown-elf-as $KERNEL_ISA asm/trapVector.s  -o build/trapVector.o
riscv64-unknown-elf-as $KERNEL_ISA asm/paging.s -o build/paging_asm.o
riscv64-unknown-elf-as $KERNEL_ISA asm/sbi_call.s       -o build/sbi_call.o
riscv64-unknown-elf-as $KERNEL_ISA asm/hart_extension.s       -o build/hart_extension.o
riscv64-unknown-elf-as $KERNEL_ISA asm/controll_status_register.s       -o build/controll_status_register.o

# C

# Source
riscv64-unknown-elf-gcc -c src/kernel.c -o build/kernel.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/hart.c -o build/hart.o $KERNEL_CFLAGS

# Vera_Utils
riscv64-unknown-elf-gcc -c src/Vera_Utils/utils.c -o build/utils.o $KERNEL_CFLAGS

# Vera_UART
riscv64-unknown-elf-gcc -c src/Vera_UART/uart.c -o build/uart.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_UART/driver/16550.c -o build/uart_16550.o $KERNEL_CFLAGS

# Vera_Memory
riscv64-unknown-elf-gcc -c src/Vera_Memory/mem_controller.c -o build/mem_controller.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_Memory/paging.c -o build/paging.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_Memory/allocator.c -o build/allocator.o $KERNEL_CFLAGS

# Vera_Interrupt
riscv64-unknown-elf-gcc -c src/Vera_Interrupt/trap_Handler.c -o build/trap_Handler.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_Interrupt/IMSIC.c -o build/imsic.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_Interrupt/APLIC.c -o build/APLIC.o $KERNEL_CFLAGS

# Vera_FS
riscv64-unknown-elf-gcc -c src/Vera_FS/file_system.c -o build/file_system.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_FS/cfi_flash.c -o build/cfi_flash.o $KERNEL_CFLAGS

# Vera_Error
riscv64-unknown-elf-gcc -c src/Vera_Error/error.c -o build/error.o $KERNEL_CFLAGS

# Vera_Device_Driver
riscv64-unknown-elf-gcc -c src/Vera_Device_Driver/dtb_parser.c -o build/dtb_parser.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_Device_Driver/PCIe.c -o build/PCIe.o $KERNEL_CFLAGS
riscv64-unknown-elf-gcc -c src/Vera_Device_Driver/driver_manager.c -o build/driver_manager.o $KERNEL_CFLAGS

# Vera_Debug
riscv64-unknown-elf-gcc -c src/Vera_Debug/debug.c -o build/debug.o $KERNEL_CFLAGS


# Lily
cd ..
riscv64-unknown-elf-gcc -c Lily/src/dtb_parser.c -o Kernel/build/dtb_parser_lily.o $LILY_CFLAGS
riscv64-unknown-elf-gcc -c Lily/src/mem_controller.c -o Kernel/build/mem_controller_lily.o $LILY_CFLAGS
riscv64-unknown-elf-gcc -c Lily/src/paging.c -o Kernel/build/paging_lily.o $LILY_CFLAGS
riscv64-unknown-elf-gcc -c Lily/src/lily_main.c -o Kernel/build/lily.o $LILY_CFLAGS
riscv64-unknown-elf-gcc -c Lily/src/lily_extension.c -o Kernel/build/lily_extension.o $LILY_CFLAGS
riscv64-unknown-elf-as $LILY_ISA Lily/asm/lily.s -o Kernel/build/lily_asm.o

cd Kernel
# Link
riscv64-unknown-elf-ld -T linker.ld \
  build/start.o build/trapVector.o build/paging_asm.o build/sbi_call.o build/kernel.o build/uart.o build/error.o build/utils.o build/trap_Handler.o build/debug.o \
  build/dtb_parser.o build/mem_controller.o build/paging.o  build/allocator.o build/uart_16550.o build/file_system.o build/PCIe.o build/cfi_flash.o build/hart.o \
  build/hart_extension.o  build/imsic.o build/driver_manager.o build/controll_status_register.o build/APLIC.o \
  build/dtb_parser_lily.o build/mem_controller_lily.o build/paging_lily.o build/lily.o build/lily_extension.o build/lily_asm.o \
  -o build/kernel.elf

# Boot

qemu-system-riscv64 \
  -machine virt,aia=aplic-imsic \
  -m 1024M \
  -nographic \
  -kernel build/kernel.elf \
  -bios default \
  -smp 2 \
  -drive if=pflash,format=raw,unit=1,file=drive_test/flash1.img \
  -device pcie-root-port,id=pcie_port1,chassis=1,slot=1 \
  -drive file=drive_test/drive.img,format=raw,if=none,id=hd0 \
  -device nvme,serial=deadbeef,drive=hd0,bus=pcie_port1 
  #-machine dumpdtb=virt.dtb \

# qemu-system-riscv64 \
#   -machine virt \
#   -m 1024M \
#   -nographic \
#   -kernel build/kernel.elf \
#   -bios default \
#   -smp 1 \
#   -drive if=pflash,format=raw,unit=1,file=drive_test/flash1.img \
#   -drive file=drive_test/drive.img,format=raw,if=none,id=hd0 \
#   -device virtio-blk-device,drive=hd0 \
  #-s -S \
  #-machine dumpdtb=virt.dtb \
  #-drive if=pflash,format=raw,unit=0,file=drive_test/flash0.img \
#   #-d guest_errors -D qemu.log
