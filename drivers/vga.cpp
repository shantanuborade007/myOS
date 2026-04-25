// =============================================================================
// vga.cpp — VGA Text Mode Driver Implementation
// =============================================================================

#include "vga.h"
#include <stdarg.h>             // va_list, va_start, va_arg, va_end
                                // stdarg.h is provided by the COMPILER (not OS)
                                // so it's available in freestanding mode

// =============================================================================
// Port I/O helpers
//
// x86 has a separate I/O address space accessed via IN/OUT instructions.
// These wrap the instructions as inline assembly so C++ can call them.
// =============================================================================

// Write a byte to an I/O port
static inline void outb(uint16_t port, uint8_t value) {
    // "outb %0, %1"  — AT&T syntax: source, destination
    // "a"(value)     — value goes into AL (the 'a' register, byte width)
    // "Nd"(port)     — port goes into DX ('d' register) or immediate constant
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Read a byte from an I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    // "inb %1, %0"   — read from port into result
    // "=a"(result)   — output: store AL into result
    // "Nd"(port)     — input: port in DX or immediate
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// =============================================================================
// VGA hardware constants
// =============================================================================
static const uintptr_t VGA_BUFFER_ADDR = 0xB8000;

// VGA CRT Controller ports
// We write the register index to 0x3D4, then the value to 0x3D5
static const uint16_t VGA_CTRL_PORT = 0x3D4;   // Index register
static const uint16_t VGA_DATA_PORT = 0x3D5;   // Data register

// VGA register indices for cursor position
static const uint8_t VGA_REG_CURSOR_HIGH = 0x0E;
static const uint8_t VGA_REG_CURSOR_LOW  = 0x0F;

// VGA register indices for cursor shape
static const uint8_t VGA_REG_CURSOR_START = 0x0A;
static const uint8_t VGA_REG_CURSOR_END   = 0x0B;

// =============================================================================
// Driver state — private to this file (static = file-scope linkage)
// =============================================================================
static volatile uint16_t* vga_buffer =
    reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_ADDR);

static int     cursor_row   = 0;
static int     cursor_col   = 0;
static uint8_t color_attr   = 0x07;    // Default: light gray on black

// =============================================================================
// Internal helpers — not part of the public API
// =============================================================================

// Pack character + color into a 16-bit VGA cell value
static inline uint16_t make_vga_entry(char c, uint8_t color) {
    return static_cast<uint16_t>(static_cast<uint8_t>(c))
         | (static_cast<uint16_t>(color) << 8);
}

// Write the hardware cursor position to VGA controller registers
// The cursor position is a single linear index: row * 80 + col
static void hw_cursor_update(int row, int col) {
    uint16_t pos = static_cast<uint16_t>(row * VGA_COLS + col);

    // Send the HIGH byte of the cursor position
    outb(VGA_CTRL_PORT, VGA_REG_CURSOR_HIGH);
    outb(VGA_DATA_PORT, static_cast<uint8_t>(pos >> 8));

    // Send the LOW byte of the cursor position
    outb(VGA_CTRL_PORT, VGA_REG_CURSOR_LOW);
    outb(VGA_DATA_PORT, static_cast<uint8_t>(pos & 0xFF));
}

// =============================================================================
// Public API Implementation
// =============================================================================

uint8_t vga_make_color(vga_color fg, vga_color bg) {
    // Color attribute byte:
    //   bits 3-0 = foreground color
    //   bits 7-4 = background color
    return static_cast<uint8_t>(fg) | (static_cast<uint8_t>(bg) << 4);
}

void vga_set_color(uint8_t color) {
    color_attr = color;
}

void vga_set_colors(vga_color fg, vga_color bg) {
    color_attr = vga_make_color(fg, bg);
}

void vga_set_cursor(int row, int col) {
    // Clamp to screen boundaries
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= VGA_ROWS) row = VGA_ROWS - 1;
    if (col >= VGA_COLS)  col = VGA_COLS  - 1;

    cursor_row = row;
    cursor_col = col;
    hw_cursor_update(row, col);
}

void vga_get_cursor(int* row, int* col) {
    if (row) *row = cursor_row;
    if (col) *col = cursor_col;
}

void vga_cursor_show(bool visible) {
    if (visible) {
        // Cursor shape: scanlines 14–15 (bottom two lines of character cell)
        // Bit 5 of the start register = 0 means cursor ON
        outb(VGA_CTRL_PORT, VGA_REG_CURSOR_START);
        outb(VGA_DATA_PORT, 14);            // Start scanline
        outb(VGA_CTRL_PORT, VGA_REG_CURSOR_END);
        outb(VGA_DATA_PORT, 15);            // End scanline
    } else {
        // Bit 5 of the start register = 1 means cursor OFF
        outb(VGA_CTRL_PORT, VGA_REG_CURSOR_START);
        outb(VGA_DATA_PORT, 0x20);          // Bit 5 set = disable cursor
    }
}

void vga_clear() {
    uint16_t blank = make_vga_entry(' ', color_attr);
    for (int i = 0; i < VGA_ROWS * VGA_COLS; i++) {
        vga_buffer[i] = blank;
    }
    cursor_row = 0;
    cursor_col = 0;
    hw_cursor_update(0, 0);
}

void vga_init() {
    color_attr = vga_make_color(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    vga_clear();
    vga_cursor_show(true);
}

void vga_scroll(int lines) {
    if (lines <= 0) return;
    if (lines >= VGA_ROWS) {
        vga_clear();
        return;
    }

    // Shift every row up by 'lines' positions
    for (int row = 0; row < VGA_ROWS - lines; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            vga_buffer[row * VGA_COLS + col] =
                vga_buffer[(row + lines) * VGA_COLS + col];
        }
    }

    // Clear the newly exposed rows at the bottom
    uint16_t blank = make_vga_entry(' ', color_attr);
    for (int row = VGA_ROWS - lines; row < VGA_ROWS; row++) {
        for (int col = 0; col < VGA_COLS; col++) {
            vga_buffer[row * VGA_COLS + col] = blank;
        }
    }
}

void vga_put_at(char c, uint8_t color, int row, int col) {
    if (row < 0 || row >= VGA_ROWS) return;
    if (col < 0 || col >= VGA_COLS)  return;
    vga_buffer[row * VGA_COLS + col] = make_vga_entry(c, color);
}

void vga_print_at(const char* str, uint8_t color, int row, int col) {
    int c = col;
    for (int i = 0; str[i] != '\0'; i++) {
        if (c >= VGA_COLS) break;          // Don't overflow the row
        vga_put_at(str[i], color, row, c);
        c++;
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        // Tab: advance to the next multiple-of-8 column
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    } else if (c == '\b') {
        // Backspace: move cursor back and erase character
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }
        vga_buffer[cursor_row * VGA_COLS + cursor_col] =
            make_vga_entry(' ', color_attr);
    } else {
        vga_buffer[cursor_row * VGA_COLS + cursor_col] =
            make_vga_entry(c, color_attr);
        cursor_col++;
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    // Scroll up if we've gone past the last row
    if (cursor_row >= VGA_ROWS) {
        vga_scroll(1);
        cursor_row = VGA_ROWS - 1;
    }

    // Always sync the hardware cursor with our software position
    hw_cursor_update(cursor_row, cursor_col);
}

void vga_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

void vga_draw_hline(int row, char c, uint8_t color) {
    for (int col = 0; col < VGA_COLS; col++) {
        vga_put_at(c, color, row, col);
    }
}

void vga_print_centered(const char* str, uint8_t color, int row) {
    // Calculate string length manually (no strlen — no stdlib)
    int len = 0;
    while (str[len] != '\0') len++;

    int col = (VGA_COLS - len) / 2;
    if (col < 0) col = 0;
    vga_print_at(str, color, row, col);
}

// =============================================================================
// kprintf — Formatted kernel print
//
// Implements a subset of printf format specifiers.
// Uses the vga_putchar / vga_print primitives above.
// =============================================================================

// Helper: print a null-terminated string (for %s in kprintf)
static void kprintf_puts(const char* s) {
    if (s == NULL) {
        vga_print("(null)");
        return;
    }
    vga_print(s);
}

// Helper: print an unsigned integer in decimal (for %u, %d in kprintf)
static void kprintf_print_uint(uint32_t value) {
    if (value == 0) {
        vga_putchar('0');
        return;
    }
    // Build digits right-to-left into a buffer
    char buf[12];
    int  i = 11;
    buf[11] = '\0';
    while (value > 0 && i > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }
    vga_print(&buf[i]);
}

// Helper: print a signed integer in decimal (for %d in kprintf)
static void kprintf_print_int(int32_t value) {
    if (value < 0) {
        vga_putchar('-');
        // Cast to uint32_t carefully — INT32_MIN negation would overflow
        kprintf_print_uint(static_cast<uint32_t>(-(value + 1)) + 1);
    } else {
        kprintf_print_uint(static_cast<uint32_t>(value));
    }
}

// Helper: print a uint32_t as hexadecimal (for %x, %X in kprintf)
static void kprintf_print_hex(uint32_t value, bool uppercase) {
    const char* digits_lo = "0123456789abcdef";
    const char* digits_hi = "0123456789ABCDEF";
    const char* digits = uppercase ? digits_hi : digits_lo;

    char buf[9];                // 8 hex digits + null
    buf[8] = '\0';

    // Extract nibbles from most-significant to least-significant
    for (int i = 7; i >= 0; i--) {
        buf[i] = digits[value & 0xF];
        value >>= 4;
    }

    // Skip leading zeros (but always print at least one digit)
    int start = 0;
    while (start < 7 && buf[start] == '0') start++;
    vga_print(&buf[start]);
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);        // Point args to the first variadic argument

    for (int i = 0; fmt[i] != '\0'; i++) {

        if (fmt[i] != '%') {
            // Not a format specifier — print the character directly
            vga_putchar(fmt[i]);
            continue;
        }

        // We hit a '%' — look at the next character for the specifier
        i++;
        switch (fmt[i]) {
            case 'c': {
                // %c — single character
                // va_arg promotes char to int, so retrieve as int
                char c = static_cast<char>(va_arg(args, int));
                vga_putchar(c);
                break;
            }
            case 's': {
                // %s — null-terminated string
                const char* s = va_arg(args, const char*);
                kprintf_puts(s);
                break;
            }
            case 'd': {
                // %d — signed decimal integer
                int32_t n = va_arg(args, int32_t);
                kprintf_print_int(n);
                break;
            }
            case 'u': {
                // %u — unsigned decimal integer
                uint32_t n = va_arg(args, uint32_t);
                kprintf_print_uint(n);
                break;
            }
            case 'x': {
                // %x — unsigned hex, lowercase
                uint32_t n = va_arg(args, uint32_t);
                kprintf_print_hex(n, false);
                break;
            }
            case 'X': {
                // %X — unsigned hex, uppercase
                uint32_t n = va_arg(args, uint32_t);
                kprintf_print_hex(n, true);
                break;
            }
            case '%': {
                // %% — literal percent sign
                vga_putchar('%');
                break;
            }
            case '\0': {
                // Trailing '%' at end of string — don't go past end
                goto done;
            }
            default: {
                // Unknown specifier — print it literally
                vga_putchar('%');
                vga_putchar(fmt[i]);
                break;
            }
        }
    }

done:
    va_end(args);               // Clean up the va_list
}
