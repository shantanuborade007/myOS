#include "rtc.h"

static inline void outb(uint16_t port, uint8_t val) { asm volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t r; asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }

static int get_update_in_progress_flag() {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

static uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

void rtc_init() {}

Time rtc_get_time() {
    Time t;
    while (get_update_in_progress_flag());
    t.second = get_rtc_register(0x00);
    t.minute = get_rtc_register(0x02);
    t.hour = get_rtc_register(0x04);
    t.day = get_rtc_register(0x07);
    t.month = get_rtc_register(0x08);
    t.year = get_rtc_register(0x09);
    
    uint8_t registerB = get_rtc_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        t.second = (t.second & 0x0F) + ((t.second / 16) * 10);
        t.minute = (t.minute & 0x0F) + ((t.minute / 16) * 10);
        t.hour = ( (t.hour & 0x0F) + (((t.hour & 0x70) / 16) * 10) ) | (t.hour & 0x80);
        t.day = (t.day & 0x0F) + ((t.day / 16) * 10);
        t.month = (t.month & 0x0F) + ((t.month / 16) * 10);
        t.year = (t.year & 0x0F) + ((t.year / 16) * 10);
    }
    
    // Convert 12 hour clock to 24 hour clock
    if (!(registerB & 0x02) && (t.hour & 0x80)) {
        t.hour = ((t.hour & 0x7F) + 12) % 24;
    }

    t.year += (t.year / 100 == 20) ? 0 : 2000;
    return t;
}
