// =============================================================================
// isr.cpp — Common CPU exception handler
// Called by all isr0-isr31 assembly stubs via isr_common
// =============================================================================

#include "idt.h"
#include "../drivers/vga.h"

// =============================================================================
// Exception names — index matches the CPU exception vector number
// =============================================================================
static const char* exception_names[] = {
    "Division Error",                   // 0  #DE
    "Debug Exception",                  // 1  #DB
    "Non-Maskable Interrupt",           // 2
    "Breakpoint",                       // 3  #BP
    "Overflow",                         // 4  #OF
    "BOUND Range Exceeded",             // 5  #BR
    "Invalid Opcode",                   // 6  #UD
    "Device Not Available",             // 7  #NM
    "Double Fault",                     // 8  #DF
    "Coprocessor Segment Overrun",      // 9
    "Invalid TSS",                      // 10 #TS
    "Segment Not Present",              // 11 #NP
    "Stack-Segment Fault",              // 12 #SS
    "General Protection Fault",         // 13 #GP
    "Page Fault",                       // 14 #PF
    "Reserved (15)",                    // 15
    "x87 FPU Error",                    // 16 #MF
    "Alignment Check",                  // 17 #AC
    "Machine Check",                    // 18 #MC
    "SIMD FP Exception",                // 19 #XM
    "Virtualization Exception",         // 20 #VE
    "Reserved (21)",                    // 21
    "Reserved (22)",                    // 22
    "Reserved (23)",                    // 23
    "Reserved (24)",                    // 24
    "Reserved (25)",                    // 25
    "Reserved (26)",                    // 26
    "Reserved (27)",                    // 27
    "Reserved (28)",                    // 28
    "Reserved (29)",                    // 29
    "Reserved (30)",                    // 30
    "Reserved (31)"                     // 31
};

// =============================================================================
// panic_screen — draw a full BSOD-style exception screen
// =============================================================================
static void panic_screen(InterruptFrame* frame) {
    // ── Background: fill entire screen with red ────────────────────────────
    uint8_t panic_bg  = vga_make_color(VGA_COLOR_WHITE,      VGA_COLOR_RED);
    uint8_t panic_hdr = vga_make_color(VGA_COLOR_YELLOW,     VGA_COLOR_RED);
    uint8_t panic_reg = vga_make_color(VGA_COLOR_WHITE,       VGA_COLOR_RED);
    uint8_t panic_dim = vga_make_color(VGA_COLOR_LIGHT_GRAY,  VGA_COLOR_RED);

    // Fill all rows with red background
    vga_set_color(panic_bg);
    for (int row = 0; row < VGA_ROWS; row++) {
        vga_draw_hline(row, ' ', panic_bg);
    }

    // ── Header ─────────────────────────────────────────────────────────────
    vga_print_centered("*** KERNEL EXCEPTION ***", panic_hdr, 0);
    vga_draw_hline(1, '-', panic_dim);

    // ── Exception name and vector ──────────────────────────────────────────
    vga_set_cursor(2, 2);
    vga_set_color(panic_hdr);
    kprintf("EXCEPTION #%u: %s",
            frame->int_no,
            frame->int_no < 32 ? exception_names[frame->int_no] : "Unknown");

    // ── Error code (when present) ──────────────────────────────────────────
    vga_set_cursor(3, 2);
    vga_set_color(panic_bg);
    if (frame->err_code != 0) {
        kprintf("Error Code:  0x%X", frame->err_code);

        // For #GP and #PF, decode the error code fields
        if (frame->int_no == 13) {  // #GP
            kprintf("  (");
            if (frame->err_code & 0x1) kprintf("External ");
            if (frame->err_code & 0x2) kprintf("IDT-entry ");
            kprintf("selector=0x%X)", (frame->err_code >> 3) & 0x1FFF);
        }
        if (frame->int_no == 14) {  // #PF
            kprintf("  (");
            kprintf("%s ", (frame->err_code & 0x1) ? "Protection" : "NotPresent");
            kprintf("%s ", (frame->err_code & 0x2) ? "Write"      : "Read");
            kprintf("%s",  (frame->err_code & 0x4) ? "User"       : "Kernel");
            kprintf(")");
        }
    } else {
        kprintf("Error Code:  (none)");
    }

    // ── Dividing line ──────────────────────────────────────────────────────
    vga_draw_hline(4, '-', panic_dim);

    // ── General-purpose registers ──────────────────────────────────────────
    vga_set_cursor(5, 2);
    vga_set_color(panic_hdr);
    vga_print("REGISTERS:\n");

    vga_set_color(panic_reg);
    vga_set_cursor(6, 2);
    kprintf("EAX=0x%X  EBX=0x%X  ECX=0x%X  EDX=0x%X",
            frame->eax, frame->ebx, frame->ecx, frame->edx);

    vga_set_cursor(7, 2);
    kprintf("ESI=0x%X  EDI=0x%X  EBP=0x%X  ESP=0x%X",
            frame->esi, frame->edi, frame->ebp, frame->esp_dummy);

    // ── Instruction pointer and flags ─────────────────────────────────────
    vga_draw_hline(8, '-', panic_dim);
    vga_set_cursor(9, 2);
    vga_set_color(panic_hdr);
    vga_print("EXECUTION STATE:\n");

    vga_set_color(panic_reg);
    vga_set_cursor(10, 2);
    kprintf("EIP    = 0x%X  (faulting instruction address)", frame->eip);

    vga_set_cursor(11, 2);
    kprintf("CS     = 0x%X", frame->cs);

    vga_set_cursor(12, 2);
    kprintf("EFLAGS = 0x%X", frame->eflags);

    // Decode key EFLAGS bits
    vga_set_cursor(13, 2);
    vga_set_color(panic_dim);
    kprintf("  IF=%u  TF=%u  SF=%u  ZF=%u  AF=%u  PF=%u  CF=%u",
            (frame->eflags >> 9)  & 1,   // Interrupt Flag
            (frame->eflags >> 8)  & 1,   // Trap Flag
            (frame->eflags >> 7)  & 1,   // Sign Flag
            (frame->eflags >> 6)  & 1,   // Zero Flag
            (frame->eflags >> 4)  & 1,   // Auxiliary Carry Flag
            (frame->eflags >> 2)  & 1,   // Parity Flag
            (frame->eflags >> 0)  & 1);  // Carry Flag

    // ── Footer ─────────────────────────────────────────────────────────────
    vga_draw_hline(VGA_ROWS - 3, '-', panic_dim);
    vga_set_color(panic_bg);
    vga_print_centered("System halted. Reset QEMU to restart.", panic_bg, VGA_ROWS - 2);
    vga_print_centered("MyOS v0.7 | Stage 7: IDT + Exception Handling", panic_dim, VGA_ROWS - 1);
}

// =============================================================================
// isr_handler — the common C++ exception handler
//
// Called from isr_common in idt_flush.asm with a pointer to the full
// InterruptFrame on the stack. We have read access to every register
// value at the moment the exception occurred.
// =============================================================================
extern "C" void isr_handler(InterruptFrame* frame) {
    // Draw the panic screen with full register dump
    panic_screen(frame);

    // Halt — exception handlers for fatal exceptions never return.
    // If we returned, the CPU would re-execute the faulting instruction,
    // causing another exception immediately (infinite exception loop).
    while (true) {
        asm volatile("cli; hlt");
    }
}
