// =============================================================================
// gdt.h — Global Descriptor Table public API
// =============================================================================

#pragma once
#include "../include/types.h"

// =============================================================================
// GDT Descriptor — exactly 8 bytes, packed to match hardware layout
//
// __attribute__((packed)) prevents GCC from inserting alignment padding.
// Without it, GCC might add bytes between fields, corrupting the descriptor.
// =============================================================================
struct GDTDescriptor {
    uint16_t limit_low;     // Limit bits 15:0
    uint16_t base_low;      // Base  bits 15:0
    uint8_t  base_mid;      // Base  bits 23:16
    uint8_t  access;        // Access byte: P|DPL|S|E|DC|RW|A
    uint8_t  flags_limit;   // Flags[7:4] | Limit bits 19:16 [3:0]
    uint8_t  base_high;     // Base  bits 31:24
} __attribute__((packed));

// =============================================================================
// GDTR — the 6-byte structure that lgdt reads
// =============================================================================
struct GDTRegister {
    uint16_t limit;         // Size of GDT in bytes, minus 1
    uint32_t base;          // Linear address of the GDT array
} __attribute__((packed));

// =============================================================================
// Access byte bit flags — combine these with OR to build an access byte
// =============================================================================
namespace GDTAccess {
    static const uint8_t PRESENT        = (1 << 7); // Segment is valid
    static const uint8_t PRIVILEGE_0    = (0 << 5); // Ring 0 — kernel
    static const uint8_t PRIVILEGE_3    = (3 << 5); // Ring 3 — user
    static const uint8_t DESCRIPTOR     = (1 << 4); // Code/data type (S=1)
    static const uint8_t EXECUTABLE     = (1 << 3); // Code segment (E=1)
    static const uint8_t DIRECTION_UP   = (0 << 2); // Data grows up
    static const uint8_t CONFORMING_NO  = (0 << 2); // Code: non-conforming
    static const uint8_t READABLE       = (1 << 1); // Code: readable
    static const uint8_t WRITABLE       = (1 << 1); // Data: writable
    static const uint8_t ACCESSED       = (1 << 0); // CPU sets this; init to 0

    // Pre-built access bytes for common segment types
    static const uint8_t KERNEL_CODE =
        PRESENT | PRIVILEGE_0 | DESCRIPTOR | EXECUTABLE | READABLE;
    static const uint8_t KERNEL_DATA =
        PRESENT | PRIVILEGE_0 | DESCRIPTOR | WRITABLE;
    static const uint8_t USER_CODE =
        PRESENT | PRIVILEGE_3 | DESCRIPTOR | EXECUTABLE | READABLE;
    static const uint8_t USER_DATA =
        PRESENT | PRIVILEGE_3 | DESCRIPTOR | WRITABLE;
}

// =============================================================================
// Flags nibble — goes into the upper 4 bits of byte 6
// =============================================================================
namespace GDTFlags {
    static const uint8_t GRANULARITY_4K = (1 << 7); // Limit in 4KB pages
    static const uint8_t GRANULARITY_1B = (0 << 7); // Limit in bytes
    static const uint8_t SIZE_32BIT     = (1 << 6); // 32-bit protected mode
    static const uint8_t SIZE_16BIT     = (0 << 6); // 16-bit protected mode
    static const uint8_t LONG_MODE      = (1 << 5); // 64-bit (we don't use this)

    // Standard 32-bit flat segment flags
    static const uint8_t PROTECTED_32 = GRANULARITY_4K | SIZE_32BIT;
}

// =============================================================================
// GDT Segment selector values
// Each is an index into the GDT (× 8 bytes per entry)
// =============================================================================
namespace GDTSelector {
    static const uint16_t KERNEL_NULL = 0x00;
    static const uint16_t KERNEL_CODE = 0x08; // Entry 1
    static const uint16_t KERNEL_DATA = 0x10; // Entry 2
    static const uint16_t USER_CODE   = 0x18; // Entry 3 (reserved for later)
    static const uint16_t USER_DATA   = 0x20; // Entry 4 (reserved for later)
}

// =============================================================================
// Number of GDT entries we're defining
// =============================================================================
static const int GDT_ENTRY_COUNT = 5; // null, kernel code, kernel data, user code, user data

// =============================================================================
// Public functions
// =============================================================================

// Initialize and load the GDT. Call once during kernel startup.
void gdt_init();

// Build and install one descriptor entry.
// index  : which GDT slot (0=null, 1=kcode, 2=kdata, ...)
// base   : 32-bit segment base address
// limit  : 20-bit segment limit
// access : access byte (use GDTAccess:: constants)
// flags  : flags nibble (use GDTFlags:: constants)
void gdt_set_descriptor(int index,
                        uint32_t base,
                        uint32_t limit,
                        uint8_t  access,
                        uint8_t  flags);

// Print the current GDT contents to the VGA screen (for debugging)
void gdt_print_info();

// Declared extern "C" so gdt_flush.asm can call back into C++ if needed
// This is the ASM function that runs lgdt and reloads segment registers
extern "C" void gdt_flush(uint32_t gdtr_addr);
