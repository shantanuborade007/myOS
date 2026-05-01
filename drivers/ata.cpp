#include "ata.h"

static inline void outb(uint16_t port, uint8_t val) { asm volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t r; asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static inline void outw(uint16_t port, uint16_t val) { asm volatile("outw %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint16_t inw(uint16_t port) { uint16_t r; asm volatile("inw %1, %0" : "=a"(r) : "Nd"(port)); return r; }

static void ata_wait_bsy() {
    while (inb(0x1F7) & 0x80);
}
static void ata_wait_drq() {
    while (!(inb(0x1F7) & 0x08));
}

void ata_init() {}

void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20); // READ
    
    ata_wait_bsy();
    ata_wait_drq();
    
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) ptr[i] = inw(0x1F0);
}

void ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_bsy();
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // WRITE
    
    ata_wait_bsy();
    ata_wait_drq();
    
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) outw(0x1F0, ptr[i]);
    
    outb(0x1F7, 0xE7); // flush
    ata_wait_bsy();
}
