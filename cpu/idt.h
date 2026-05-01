// =============================================================================
// idt.h — Interrupt Descriptor Table public API
// =============================================================================

#pragma once
#include "../include/types.h"

// =============================================================================
// IDT Gate Descriptor — 8 bytes, packed
// =============================================================================
struct IDTDescriptor {
    uint16_t offset_low;    // Handler address bits 15:0
    uint16_t selector;      // Code segment selector (0x08 = kernel code)
    uint8_t  reserved;      // Always 0
    uint8_t  type_attr;     // Gate type | DPL | Present flag
    uint16_t offset_high;   // Handler address bits 31:16
} __attribute__((packed));

// =============================================================================
// IDTR — the 6-byte structure that lidt reads (same layout as GDTR)
// =============================================================================
struct IDTRegister {
    uint16_t limit;         // Size of IDT in bytes, minus 1
    uint32_t base;          // Linear address of IDT array
} __attribute__((packed));

// =============================================================================
// Gate type_attr values
// =============================================================================
namespace IDTGate {
    // 32-bit Interrupt Gate: P=1, DPL=0, type=0xE
    // Automatically clears IF (disables interrupts) on entry
    static const uint8_t INTERRUPT_GATE_RING0 = 0x8E;

    // 32-bit Trap Gate: P=1, DPL=0, type=0xF
    // Does NOT clear IF — used for debug breakpoints
    static const uint8_t TRAP_GATE_RING0      = 0x8F;

    // 32-bit Interrupt Gate callable from Ring 3 (DPL=3)
    // Needed for INT 0x80 syscall interface later
    static const uint8_t INTERRUPT_GATE_RING3 = 0xEE;
}

// =============================================================================
// Registers pushed onto stack by our ASM stubs + CPU exception frame.
// This struct maps exactly onto the stack when the C++ handler is called.
//
// Stack layout (low address at top, pushed last = at top):
//   pushed by pusha:    edi esi ebp esp_dummy ebx edx ecx eax
//   pushed by stub:     int_no  err_code
//   pushed by CPU:      eip  cs  eflags  [esp  ss  if privilege change]
// =============================================================================
struct InterruptFrame {
    // Saved general-purpose registers (pushed by pusha in stub)
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;

    // Exception info (pushed by our stub)
    uint32_t int_no;        // Exception vector number (0-31)
    uint32_t err_code;      // Error code (0 if exception has no error code)

    // CPU exception frame (pushed automatically by CPU)
    uint32_t eip;           // Instruction that caused the exception
    uint32_t cs;            // Code segment at time of exception
    uint32_t eflags;        // CPU flags at time of exception

    // Only present on privilege-level change (Ring 3 → Ring 0):
    uint32_t user_esp;
    uint32_t user_ss;
} __attribute__((packed));

// =============================================================================
// Number of IDT entries
// x86 supports 256 vectors; we define all 256 to be safe
// =============================================================================
static const int IDT_ENTRY_COUNT = 256;

// =============================================================================
// Public API
// =============================================================================

// Initialize IDT: register all 32 exception handlers, remap PIC, load IDTR
void idt_init();

// Install one gate into the IDT
void idt_set_gate(uint8_t  vector,
                  uint32_t handler_addr,
                  uint16_t selector,
                  uint8_t  type_attr);

// The common C++ exception handler — called by all ASM stubs
// Declared extern "C" so ASM can call it without name mangling
extern "C" void isr_handler(InterruptFrame* frame);

// Declared extern "C" so C++ can call into idt_flush.asm to load IDTR
extern "C" void idt_flush(uint32_t idtr_addr);

// ASM stubs — declared here so idt.cpp can take their addresses
// Each one is defined in idt_flush.asm
extern "C" void isr0();  extern "C" void isr1();
extern "C" void isr2();  extern "C" void isr3();
extern "C" void isr4();  extern "C" void isr5();
extern "C" void isr6();  extern "C" void isr7();
extern "C" void isr8();  extern "C" void isr9();
extern "C" void isr10(); extern "C" void isr11();
extern "C" void isr12(); extern "C" void isr13();
extern "C" void isr14(); extern "C" void isr15();
extern "C" void isr16(); extern "C" void isr17();
extern "C" void isr18(); extern "C" void isr19();
extern "C" void isr20(); extern "C" void isr21();
extern "C" void isr22(); extern "C" void isr23();
extern "C" void isr24(); extern "C" void isr25();
extern "C" void isr26(); extern "C" void isr27();
extern "C" void isr28(); extern "C" void isr29();
extern "C" void isr30(); extern "C" void isr31();

// =============================================================================
// IRQ stub declarations — defined in idt_flush.asm
// =============================================================================
extern "C" void irq1_stub();    // Keyboard IRQ1 → vector 33

// Install a hardware IRQ handler into the IDT
// irq_num: 0-15 (maps to vector 32-47)
void idt_install_irq(uint8_t irq_num, uint32_t handler_addr);
