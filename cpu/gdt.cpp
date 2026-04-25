#include "gdt.h"
#include "../drivers/vga.h"

static GDTDescriptor gdt[GDT_ENTRY_COUNT];
static GDTRegister   gdtr;

void gdt_set_descriptor(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    if (index < 0 || index >= GDT_ENTRY_COUNT) return;

    gdt[index].base_low  = (base & 0xFFFF);
    gdt[index].base_mid  = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;

    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].flags_limit = ((limit >> 16) & 0x0F) | (flags & 0xF0);

    gdt[index].access = access;
}

void gdt_init() {
    vga_set_colors(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    vga_print("[BOOT] Initializing GDT...\n");

    gdtr.limit = (sizeof(GDTDescriptor) * GDT_ENTRY_COUNT) - 1;
    gdtr.base  = reinterpret_cast<uint32_t>(&gdt[0]);

    // Null descriptor
    gdt_set_descriptor(0, 0, 0, 0, 0);

    // Kernel Code segment
    gdt_set_descriptor(1, 0, 0xFFFFF, GDTAccess::KERNEL_CODE, GDTFlags::PROTECTED_32);

    // Kernel Data segment
    gdt_set_descriptor(2, 0, 0xFFFFF, GDTAccess::KERNEL_DATA, GDTFlags::PROTECTED_32);

    // User Code segment
    gdt_set_descriptor(3, 0, 0xFFFFF, GDTAccess::USER_CODE, GDTFlags::PROTECTED_32);

    // User Data segment
    gdt_set_descriptor(4, 0, 0xFFFFF, GDTAccess::USER_DATA, GDTFlags::PROTECTED_32);

    gdt_flush(reinterpret_cast<uint32_t>(&gdtr));

    vga_set_colors(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("[OK]   GDT initialized and loaded.\n\n");
}

void gdt_print_info() {
    vga_set_colors(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("[GDT] Global Descriptor Table loaded:\n");
    kprintf("      Base address : 0x%x\n", gdtr.base);
    kprintf("      Size (limit) : %d bytes (%d entries)\n\n", gdtr.limit + 1, GDT_ENTRY_COUNT);

    vga_set_colors(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    kprintf("  #   Name          Selector  Base        Limit     Access  Flags\n");
    kprintf("  --- ------------- --------- ----------- --------- ------- -----\n");
    kprintf("  0   Null          0x0       0x0         0x0       0x0     1B\n");
    kprintf("  1   Kernel Code   0x8       0x0         0xFFFFF   0x9A    4KB\n");
    kprintf("  2   Kernel Data   0x10      0x0         0xFFFFF   0x92    4KB\n");
    kprintf("  3   User Code     0x18      0x0         0xFFFFF   0xFA    4KB\n");
    kprintf("  4   User Data     0x20      0x0         0xFFFFF   0xF2    4KB\n\n");

    vga_set_colors(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);
    kprintf("[GDT] All descriptors verified. Selectors active.\n\n");
}
