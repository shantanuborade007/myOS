#include "../drivers/vga.h"
#include "../include/types.h"
#include "../cpu/gdt.h"

// =============================================================================
// Draw a status bar on the very last row of the screen
// =============================================================================
static void draw_status_bar() {
    uint8_t bar_color = vga_make_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GRAY);

    // Fill the last row with the status bar background
    vga_draw_hline(VGA_ROWS - 1, ' ', bar_color);

    // Print status text on the left
    vga_print_at("  MyOS v0.6  |  Stage 6: GDT in C++  |  Protected Mode 32-bit",
                 bar_color, VGA_ROWS - 1, 0);
}

// =============================================================================
// kernel_main
// =============================================================================
extern "C" void kernel_main() {

    // ── Initialize the VGA driver ─────────────────────────────────────────────
    vga_init();                 // Clears screen, sets default color, shows cursor

    // ── Header banner ─────────────────────────────────────────────────────────
    uint8_t border = vga_make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t title  = vga_make_color(VGA_COLOR_WHITE,      VGA_COLOR_BLACK);

    vga_draw_hline(0, '=', border);
    vga_print_centered("MyOS Kernel  -  Stage 6: GDT in C++", title, 1);
    vga_draw_hline(2, '=', border);

    // Move cursor to row 3 to start output below the header
    vga_set_cursor(3, 0);

    // ── Initialize GDT ────────────────────────────────────────────────────────
    gdt_init();
    gdt_print_info();

    // ── Verify Segment Registers ──────────────────────────────────────────────
    vga_set_colors(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("[VERIFY] Segment register values after GDT reload:\n");

    uint16_t cs, ds, es, ss;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    asm volatile("mov %%ds, %0" : "=r"(ds));
    asm volatile("mov %%es, %0" : "=r"(es));
    asm volatile("mov %%ss, %0" : "=r"(ss));

    vga_set_colors(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    kprintf("  CS = 0x%x   [OK] Kernel Code\n", cs);
    kprintf("  DS = 0x%x  [OK] Kernel Data\n", ds);
    kprintf("  ES = 0x%x  [OK] Kernel Data\n", es);
    kprintf("  SS = 0x%x  [OK] Kernel Data\n\n", ss);

    // ── Status bar at bottom of screen ───────────────────────────────────────
    draw_status_bar();

    // ── Final status ──────────────────────────────────────────────────────────
    vga_set_colors(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("[OK] Stage 6 complete.\n");
    vga_set_colors(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    vga_print("     GDT is now managed in C++ with named fields.\n");
    vga_print("     Ring 3 entries reserved for future user-mode.\n");
    vga_print("     Next: Stage 7 = IDT + CPU exception handlers.\n\n");

    vga_set_colors(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print(">> Kernel halted. <<\n");

    while (true) {
        asm volatile("hlt");
    }
}
