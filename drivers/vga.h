// =============================================================================
// vga.h — VGA Text Mode Driver Public API
//
// This header is what other kernel subsystems include to use the display.
// The implementation details (buffer address, cursor state) are hidden in vga.cpp.
// =============================================================================

#pragma once
#include "../include/types.h"

// =============================================================================
// VGA Color constants
//
// These are the 16 colors available in VGA text mode.
// Use vga_make_color(fg, bg) to combine into an attribute byte.
// =============================================================================
enum vga_color : uint8_t {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GRAY    = 7,
    VGA_COLOR_DARK_GRAY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_PINK          = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
};

// =============================================================================
// VGA dimensions
// =============================================================================
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;

// =============================================================================
// Public API — these are the functions other files call
// =============================================================================

// ── Initialization ────────────────────────────────────────────────────────────

// Initialize the VGA driver: clear screen, reset cursor, set default colors
void vga_init();

// ── Color control ─────────────────────────────────────────────────────────────

// Combine a foreground and background color into a single attribute byte
uint8_t vga_make_color(vga_color fg, vga_color bg);

// Set the current text color using a pre-built attribute byte
void vga_set_color(uint8_t color_attr);

// Set foreground and background colors separately (convenience wrapper)
void vga_set_colors(vga_color fg, vga_color bg);

// ── Cursor control ────────────────────────────────────────────────────────────

// Move the software cursor (and update the hardware cursor blinking position)
void vga_set_cursor(int row, int col);

// Get current cursor position
void vga_get_cursor(int* row, int* col);

// Show or hide the hardware blinking cursor
void vga_cursor_show(bool visible);

// ── Output ────────────────────────────────────────────────────────────────────

// Clear the entire screen with the current background color
void vga_clear();

// Write a single character at the current cursor position, advance cursor
void vga_putchar(char c);

// Write a null-terminated string at the current cursor position
void vga_print(const char* str);

// Write a character at a specific (row, col) without moving the cursor
void vga_put_at(char c, uint8_t color, int row, int col);

// Print a string at a specific position with a specific color
void vga_print_at(const char* str, uint8_t color, int row, int col);

// ── Formatted printing ────────────────────────────────────────────────────────

// printf-style formatted output to the VGA screen
// Supports: %c %s %d %u %x %X %%
void kprintf(const char* fmt, ...);

// ── Screen utilities ──────────────────────────────────────────────────────────

// Scroll the screen up by 'lines' rows
void vga_scroll(int lines);

// Draw a horizontal line across the screen at 'row' using character 'c'
void vga_draw_hline(int row, char c, uint8_t color);

// Print a string centered on the given row
void vga_print_centered(const char* str, uint8_t color, int row);
