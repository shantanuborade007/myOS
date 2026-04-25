; =============================================================================
; kernel_entry.asm
; Entry point of the kernel binary — lives at 0x10000
; Calls kernel_main() written in C++
;
; Why a separate ASM file?
; The C++ compiler doesn't guarantee which function comes first in the binary.
; We need this specific entry to be at exactly byte 0 of the kernel binary.
; The linker script ensures this file's .text section comes absolutely first.
; =============================================================================

BITS 32                         ; We arrive here already in Protected Mode

[extern kernel_main]            ; Tell NASM that kernel_main is defined elsewhere
                                ; (in kernel.cpp, resolved by the linker)

[global kernel_entry]           ; Export this symbol so the linker can find it

kernel_entry:
    ; At this point:
    ;   - CPU is in 32-bit Protected Mode
    ;   - All segment registers loaded with data selector 0x10
    ;   - Stack at 0x90000
    ;   - No interrupts yet
    ;   - No GDT/IDT beyond what the bootloader set up

    call kernel_main            ; Call our C++ kernel

    ; kernel_main should never return. If it does, something went wrong.
    ; Hang safely rather than executing garbage memory.
.hang:
    cli                         ; Disable interrupts
    hlt                         ; Halt CPU
    jmp .hang                   ; Loop in case of NMI waking the CPU
