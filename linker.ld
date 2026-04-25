/*
 * linker.ld — Kernel Linker Script
 *
 * This script tells the GNU linker (ld) exactly how to arrange the kernel
 * binary in memory. Without this, ld produces an ELF file with headers
 * that the bootloader can't execute directly.
 *
 * Key concept: VMA vs LMA
 *   VMA (Virtual Memory Address) = address the code EXPECTS to run at
 *   LMA (Load Memory Address)    = address the code is PHYSICALLY loaded at
 * For now these are the same: 0x10000.
 */

OUTPUT_FORMAT(binary)       /* Output raw binary — no ELF headers, no metadata */
                            /* This is what the bootloader loads directly       */

ENTRY(kernel_entry)         /* The entry point symbol — where execution begins  */
                            /* Matches [global kernel_entry] in kernel_entry.asm */

SECTIONS {

    /*
     * Start address: 0x10000
     * The bootloader loaded the kernel here with BIOS INT 0x13.
     * All symbol addresses will be calculated relative to this.
     */
    . = 0x10000;

    /*
     * .text — executable code section
     *
     * IMPORTANT: kernel_entry.asm's .text must come FIRST.
     * We use KEEP() to prevent the linker from discarding it,
     * and list it explicitly first so it sits at exactly 0x10000.
     * If kernel_main() came first, the bootloader's 'jmp 0x10000'
     * would land in the middle of the wrong function.
     */
    .text : {
        KEEP(*(.text.kernel_entry))     /* kernel_entry.asm's code = byte 0     */
        *(.text)                        /* All other .text sections follow       */
        *(.text.*)                      /* Catch any sub-sections (LTO, etc.)   */
    }

    /*
     * .rodata — read-only data (string literals, const globals, vtables)
     * Placed right after code.
     */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /*
     * .data — initialized global and static variables
     * e.g. 'int x = 5;' → stored here with the value 5
     */
    .data : {
        *(.data)
        *(.data.*)
    }

    /*
     * .bss — uninitialized global and static variables
     * e.g. 'static int cursor_row;' → space reserved, zeroed at boot
     *
     * Note: In a raw binary, .bss takes no space in the FILE
     * (it would waste disk space storing zeros). The OS normally
     * zeros this region before starting. Since we ARE the OS,
     * we should zero it ourselves. We expose the boundaries as
     * symbols so kernel_main can do this (we'll add that in Stage 5).
     */
    .bss : {
        bss_start = .;          /* Symbol: start of BSS region */
        *(.bss)
        *(.bss.*)
        *(COMMON)               /* Uninitialized globals from C             */
        bss_end = .;            /* Symbol: end of BSS region                */
    }

    /* Expose kernel size boundaries as symbols accessible from C++ */
    kernel_start = 0x10000;
    kernel_end   = .;
}
