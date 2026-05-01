// =============================================================================
// keyboard.cpp — PS/2 Keyboard Driver Implementation
// =============================================================================

#include "keyboard.h"
#include "vga.h"

// =============================================================================
// Port I/O
// =============================================================================
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

// =============================================================================
// Hardware ports
// =============================================================================
static const uint16_t KB_DATA_PORT    = 0x60;  // Read scancode here
static const uint16_t KB_STATUS_PORT  = 0x64;  // Status / command port
static const uint16_t PIC_MASTER_CMD  = 0x20;  // PIC End-Of-Interrupt port
static const uint8_t  PIC_EOI         = 0x20;  // EOI command byte

// =============================================================================
// Scancode Set 1 — Normal (no Shift) translation table
// Index = scancode make code (0x00–0x58)
// Value = ASCII character, or 0 for non-printable / special
// =============================================================================
static const uint8_t scancode_normal[89] = {
//  0      1      2      3      4      5      6      7
    0,     27,   '1',  '2',  '3',  '4',  '5',  '6',   // 0x00–0x07
//  8      9      A      B      C      D      E      F
   '7',  '8',  '9',  '0',  '-',  '=',  '\b',  '\t',  // 0x08–0x0F
//  10     11     12     13     14     15     16     17
   'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   // 0x10–0x17
//  18     19     1A     1B     1C     1D     1E     1F
   'o',  'p',  '[',  ']',  '\n',   0,   'a',  's',   // 0x18–0x1F
//  20     21     22     23     24     25     26     27
   'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   // 0x20–0x27
//  28     29     2A     2B     2C     2D     2E     2F
   '\'', '`',    0,   '\\', 'z',  'x',  'c',  'v',  // 0x28–0x2F
//  30     31     32     33     34     35     36     37
   'b',  'n',  'm',  ',',  '.',  '/',   0,   '*',   // 0x30–0x37
//  38     39     3A     3B     3C     3D     3E     3F
    0,   ' ',    0,     0,     0,     0,     0,     0,  // 0x38–0x3F
//  40     41     42     43     44     45     46     47
    0,     0,     0,     0,     0,     0,     0,    '7', // 0x40–0x47
//  48     49     4A     4B     4C     4D     4E     4F
   '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   // 0x48–0x4F
//  50     51     52     53     54     55     56
   '2',  '3',  '0',  '.',   0,     0,     0           // 0x50–0x56
};

// =============================================================================
// Scancode Set 1 — Shifted translation table
// Same indices, but with Shift held or Caps Lock active
// =============================================================================
static const uint8_t scancode_shifted[89] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',   // 0x00–0x07
   '&',  '*',  '(',  ')',  '_',  '+',  '\b',  '\t',  // 0x08–0x0F
   'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',   // 0x10–0x17
   'O',  'P',  '{',  '}',  '\n',   0,  'A',  'S',   // 0x18–0x1F
   'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',   // 0x20–0x27
   '"',  '~',    0,   '|',  'Z',  'X',  'C',  'V',  // 0x28–0x2F
   'B',  'N',  'M',  '<',  '>',  '?',   0,   '*',   // 0x30–0x37
    0,   ' ',    0,     0,     0,     0,     0,     0, // 0x38–0x3F
    0,     0,     0,     0,     0,     0,     0,   '7', // 0x40–0x47
   '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   // 0x48–0x4F
   '2',  '3',  '0',  '.',   0,     0,     0           // 0x50–0x56
};

// =============================================================================
// Modifier key state — updated by interrupt handler on press/release
// volatile: these are written by the interrupt handler and read by other code
// =============================================================================
static volatile bool shift_held  = false;
static volatile bool ctrl_held   = false;
static volatile bool alt_held    = false;
static volatile bool caps_active = false;

// =============================================================================
// Ring buffer — written by IRQ handler, read by keyboard_read()
//
// volatile: both writer (interrupt) and reader (kernel main loop) access these.
// The compiler must not cache them in registers across function calls.
// =============================================================================
static volatile uint8_t  kb_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t kb_write = 0;  // Next position to write to
static volatile uint32_t kb_read  = 0;  // Next position to read from

// =============================================================================
// Ring buffer helpers
// =============================================================================
static inline bool buffer_full() {
    return ((kb_write + 1) % KEYBOARD_BUFFER_SIZE) == kb_read;
}

static inline bool buffer_empty() {
    return kb_write == kb_read;
}

static inline void buffer_push(uint8_t c) {
    if (!buffer_full()) {
        kb_buffer[kb_write] = c;
        // Advance write pointer — modulo wraps around the ring
        kb_write = (kb_write + 1) % KEYBOARD_BUFFER_SIZE;
    }
    // If full, silently drop the character.
    // A real OS might handle this differently (e.g., overwrite oldest).
}

static inline uint8_t buffer_pop() {
    if (buffer_empty()) return 0;
    uint8_t c = kb_buffer[kb_read];
    kb_read = (kb_read + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

// =============================================================================
// keyboard_handler — called from the IRQ1 ASM stub
//
// This function runs in interrupt context:
//   - Interrupts are disabled (IF=0) because we use an Interrupt Gate
//   - Must be fast — no blocking, no heavy computation
//   - Must send EOI before returning, or no more keyboard IRQs will fire
// =============================================================================
extern "C" void keyboard_handler() {

    // ── Read the scancode from the keyboard data port ─────────────────────
    uint8_t scancode = inb(KB_DATA_PORT);

    // ── Determine if this is a key press or key release ───────────────────
    // Bit 7 set = break (release) code
    // Bit 7 clear = make (press) code
    bool released  = (scancode & 0x80) != 0;
    uint8_t make   = scancode & 0x7F;  // Strip bit 7 to get the make code

    // ── Handle modifier keys ──────────────────────────────────────────────
    // Scancodes for modifiers (make codes):
    static const uint8_t SC_LSHIFT   = 0x2A;
    static const uint8_t SC_RSHIFT   = 0x36;
    static const uint8_t SC_LCTRL    = 0x1D;
    static const uint8_t SC_LALT     = 0x38;
    static const uint8_t SC_CAPS     = 0x3A;

    if (make == SC_LSHIFT || make == SC_RSHIFT) {
        shift_held = !released;
        goto send_eoi;
    }
    if (make == SC_LCTRL) {
        ctrl_held = !released;
        goto send_eoi;
    }
    if (make == SC_LALT) {
        alt_held = !released;
        goto send_eoi;
    }
    if (make == SC_CAPS && !released) {
        // Caps Lock toggles on press only (not on release)
        caps_active = !caps_active;
        goto send_eoi;
    }

    // ── Only process key presses (not releases) for printable chars ───────
    if (released) goto send_eoi;

    // ── Translate scancode to character ───────────────────────────────────
    if (make < 89) {
        uint8_t ch = 0;

        // Determine if we should use the shifted table.
        // For letters: Shift XOR CapsLock gives uppercase.
        // For non-letters: only Shift matters (Caps doesn't affect '1'→'!')
        bool use_shift = shift_held;

        // Check if the normal-table character is a letter (a-z)
        uint8_t normal_ch = scancode_normal[make];
        bool is_letter = (normal_ch >= 'a' && normal_ch <= 'z');

        if (is_letter) {
            // For letters: Shift XOR CapsLock
            use_shift = shift_held ^ caps_active;
        }

        ch = use_shift ? scancode_shifted[make] : scancode_normal[make];

        if (ch != 0) {
            buffer_push(ch);
        }
    }

send_eoi:
    // ── Send End-Of-Interrupt to the master PIC ───────────────────────────
    // This MUST happen or the PIC will never send another IRQ1 interrupt.
    // The keyboard is on the master PIC (IRQ1), so we only need master EOI.
    outb(PIC_MASTER_CMD, PIC_EOI);
}

// =============================================================================
// Public API implementation
// =============================================================================

void keyboard_init() {
    // ── Unmask IRQ1 on the master PIC ─────────────────────────────────────
    // The PIC mask register uses 1=masked, 0=unmasked.
    // We read the current mask, clear bit 1 (IRQ1), and write it back.
    uint8_t mask = inb(0x21);       // Read current master PIC mask
    mask &= ~(1 << 1);              // Clear bit 1 = unmask IRQ1
    outb(0x21, mask);               // Write back

    // Reset buffer state
    kb_write = 0;
    kb_read  = 0;

    // Reset modifiers
    shift_held  = false;
    ctrl_held   = false;
    alt_held    = false;
    caps_active = false;
}

uint8_t keyboard_read() {
    return buffer_pop();
}

bool keyboard_has_input() {
    return !buffer_empty();
}

uint8_t keyboard_getchar() {
    // Spin-wait until a character is available
    // The CPU executes 'hlt' between interrupts to save power
    // When an IRQ1 fires, the CPU wakes up, the handler pushes to buffer,
    // and we loop back to check again
    while (buffer_empty()) {
        asm volatile("hlt");    // Sleep until next interrupt
    }
    return buffer_pop();
}

bool keyboard_shift_held()  { return shift_held;  }
bool keyboard_caps_active() { return caps_active; }
bool keyboard_ctrl_held()   { return ctrl_held;   }
