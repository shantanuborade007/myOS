#include "mouse.h"
#include "../cpu/idt.h"

int mouse_x = 512;
int mouse_y = 384;
bool mouse_left_click = false;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}
static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) if ((inb(0x64) & 1) == 1) return;
    } else {
        while (timeout--) if ((inb(0x64) & 2) == 0) return;
    }
}
static void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}
static uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    mouse_wait(1); outb(0x64, 0xA8); 
    mouse_wait(1); outb(0x64, 0x20); 
    mouse_wait(0); uint8_t status = (inb(0x60) | 2); 
    mouse_wait(1); outb(0x64, 0x60); 
    mouse_wait(1); outb(0x60, status);
    
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();
    
    uint8_t mask = inb(0xA1);
    mask &= ~(1 << 4);
    outb(0xA1, mask);

    // Unmask IRQ2 on the master PIC (Cascade for Slave PIC)
    // If IRQ2 is masked, none of the slave IRQs (8-15) will reach the CPU!
    uint8_t master_mask = inb(0x21);
    master_mask &= ~(1 << 2);
    outb(0x21, master_mask);
}

extern "C" void mouse_handler() {
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) {
        outb(0xA0, 0x20); outb(0x20, 0x20);
        return;
    }
    
    mouse_byte[mouse_cycle++] = inb(0x60);
    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        if (!((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40))) {
            mouse_left_click = mouse_byte[0] & 0x01;
            int rel_x = mouse_byte[1];
            int rel_y = mouse_byte[2];
            if (mouse_byte[0] & 0x10) rel_x -= 256;
            if (mouse_byte[0] & 0x20) rel_y -= 256;
            
            mouse_x += rel_x;
            mouse_y -= rel_y;
            
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > 1023) mouse_x = 1023;
            if (mouse_y > 767) mouse_y = 767;
        }
    }
    outb(0xA0, 0x20); outb(0x20, 0x20);
}
