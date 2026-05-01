// =============================================================================
// keyboard.h — PS/2 Keyboard Driver Public API
// =============================================================================

#pragma once
#include "../include/types.h"

// =============================================================================
// Key constants — special keys that don't map to ASCII
// We use values above 127 so they don't collide with ASCII
// =============================================================================
namespace Key {
    static const uint8_t NONE      = 0;    // No key / unknown
    static const uint8_t ENTER     = '\n'; // Enter maps to newline
    static const uint8_t BACKSPACE = '\b'; // Backspace maps to \b
    static const uint8_t TAB       = '\t'; // Tab maps to \t
    static const uint8_t ESCAPE    = 27;   // ESC = ASCII 27

    // Special keys (above ASCII range)
    static const uint8_t UP_ARROW    = 128;
    static const uint8_t DOWN_ARROW  = 129;
    static const uint8_t LEFT_ARROW  = 130;
    static const uint8_t RIGHT_ARROW = 131;
    static const uint8_t DELETE_KEY  = 132;
    static const uint8_t HOME        = 133;
    static const uint8_t END         = 134;
    static const uint8_t PAGE_UP     = 135;
    static const uint8_t PAGE_DOWN   = 136;
    static const uint8_t F1          = 137;
    static const uint8_t F2          = 138;
    static const uint8_t F12         = 148;
}

// =============================================================================
// Ring buffer capacity — must be a power of 2 for efficient modulo
// =============================================================================
static const int KEYBOARD_BUFFER_SIZE = 256;

// =============================================================================
// Public API
// =============================================================================

// Initialize the keyboard driver: unmask IRQ1, set up buffer
void keyboard_init();

// Called by the IRQ1 handler — reads scancode from port 0x60,
// translates to ASCII/key code, puts in ring buffer
// Declared extern "C" so the ASM IRQ stub can call it
extern "C" void keyboard_handler();

// Read one character from the ring buffer.
// Returns 0 if the buffer is empty (non-blocking).
uint8_t keyboard_read();

// Returns true if there is at least one character waiting in the buffer
bool keyboard_has_input();

// Block until a key is pressed, then return it
uint8_t keyboard_getchar();

// Returns current state of modifier keys
bool keyboard_shift_held();
bool keyboard_caps_active();
bool keyboard_ctrl_held();
