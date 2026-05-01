// =============================================================================
// idt.cpp — IDT initialization and PIC remapping
// =============================================================================

#include "idt.h"
#include "../drivers/vga.h"

// =============================================================================
// Port I/O helpers (duplicated from gdt.cpp — will consolidate in a later stage)
// =============================================================================
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Small delay needed between PIC writes — writing to an unused port wastes cycles
static inline void io_wait() {
    outb(0x80, 0x00);       // Port 0x80 is used for POST codes; writing here = delay
}

// =============================================================================
// The IDT itself — 256 entries, each 8 bytes = 2KB total
// =============================================================================
static IDTDescriptor idt[IDT_ENTRY_COUNT];
static IDTRegister   idtr;

// =============================================================================
// idt_set_gate — install one descriptor into the IDT
// =============================================================================
void idt_set_gate(uint8_t  vector,
                  uint32_t handler_addr,
                  uint16_t selector,
                  uint8_t  type_attr)
{
    idt[vector].offset_low  = static_cast<uint16_t>(handler_addr & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].reserved    = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_high = static_cast<uint16_t>((handler_addr >> 16) & 0xFFFF);
}

// =============================================================================
// pic_remap — reprogram the 8259 PIC
//
// The PIC maps hardware IRQs to interrupt vectors. After reset, BIOS maps:
//   Master PIC: IRQ0-7  → vectors 8-15   (COLLIDES with CPU exceptions!)
//   Slave  PIC: IRQ8-15 → vectors 112-119
//
// We remap to:
//   Master PIC: IRQ0-7  → vectors 32-39  (safe, above exception range)
//   Slave  PIC: IRQ8-15 → vectors 40-47
//
// The PIC is programmed by sending a sequence of Initialization Command Words
// (ICW1–ICW4) to its command and data ports.
// =============================================================================
static void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    // Save existing interrupt masks (we'll restore them after remapping)
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask  = inb(0xA1);

    // ICW1: Start initialization sequence
    // 0x11 = initialize (bit 4) + expect ICW4 (bit 0)
    outb(0x20, 0x11);   io_wait();   // Master PIC command port
    outb(0xA0, 0x11);   io_wait();   // Slave  PIC command port

    // ICW2: Set vector offsets
    outb(0x21, master_offset);  io_wait();  // Master maps to vectors 32–39
    outb(0xA1, slave_offset);   io_wait();  // Slave  maps to vectors 40–47

    // ICW3: Tell master/slave how they're wired together
    outb(0x21, 0x04);   io_wait();  // Master: slave is connected to IRQ2 (bit 2)
    outb(0xA1, 0x02);   io_wait();  // Slave:  its cascade identity = IRQ2

    // ICW4: Set 8086 mode
    outb(0x21, 0x01);   io_wait();  // Master: 8086/8088 mode
    outb(0xA1, 0x01);   io_wait();  // Slave:  8086/8088 mode

    // Restore saved masks (all IRQs remain masked until explicitly enabled)
    outb(0x21, master_mask);
    outb(0xA1, slave_mask);
}

// =============================================================================
// idt_init — set up the full IDT and load it
// =============================================================================
void idt_init() {
    // ── Step 1: Remap the PIC ─────────────────────────────────────────────
    // MUST happen before enabling interrupts, otherwise a hardware IRQ
    // could fire on vector 8 and be mistaken for a Double Fault exception.
    pic_remap(0x20, 0x28);  // Master → 32–39, Slave → 40–47

    // After remapping, mask ALL IRQs — we'll unmask only keyboard in Stage 8
    outb(0x21, 0xFF);       // Mask all master PIC IRQs
    outb(0xA1, 0xFF);       // Mask all slave  PIC IRQs

    // ── Step 2: Zero out the IDT ─────────────────────────────────────────
    for (int i = 0; i < IDT_ENTRY_COUNT; i++) {
        idt[i].offset_low  = 0;
        idt[i].selector    = 0;
        idt[i].reserved    = 0;
        idt[i].type_attr   = 0;
        idt[i].offset_high = 0;
    }

    // ── Step 3: Install all 32 exception handlers ─────────────────────────
    // We cast each isr function pointer to uint32_t for idt_set_gate.
    // All handlers run in Ring 0 using the kernel code segment (0x08).
    idt_set_gate(0,  (uint32_t)isr0,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, IDTGate::TRAP_GATE_RING0);    // breakpoint
    idt_set_gate(4,  (uint32_t)isr4,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDTGate::INTERRUPT_GATE_RING0);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDTGate::INTERRUPT_GATE_RING0);

    // ── Step 4: Set up IDTR and load it ──────────────────────────────────
    idtr.limit = static_cast<uint16_t>(sizeof(IDTDescriptor) * IDT_ENTRY_COUNT - 1);
    idtr.base  = reinterpret_cast<uint32_t>(&idt);

    // idt_flush is in idt_flush.asm — runs lidt
    idt_flush(reinterpret_cast<uint32_t>(&idtr));

    // ── Step 5: Enable interrupts ─────────────────────────────────────────
    // STI sets the Interrupt Flag (IF) in EFLAGS, allowing maskable interrupts.
    // From this point on, the CPU will use our IDT for all exceptions and IRQs.
    asm volatile("sti");
}

// =============================================================================
// idt_install_irq — install a hardware IRQ handler
// irq_num 0-15 maps to IDT vectors 32-47
// =============================================================================
void idt_install_irq(uint8_t irq_num, uint32_t handler_addr) {
    uint8_t vector = 32 + irq_num;
    idt_set_gate(vector, handler_addr, 0x08, IDTGate::INTERRUPT_GATE_RING0);
}
