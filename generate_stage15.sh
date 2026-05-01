#!/bin/bash
set -e

mkdir -p memory drivers ui

# 1. Paging
cat << 'H_EOF' > memory/paging.h
#pragma once
void paging_init();
H_EOF

cat << 'C_EOF' > memory/paging.cpp
#include "paging.h"
#include "../include/types.h"

// 1024 entries * 4 bytes = 4KB page directory
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

void paging_init() {
    // Identity map the first 4GB using 4MB pages
    // PSE (Page Size Extension) must be enabled in CR4
    for (int i = 0; i < 1024; i++) {
        // bit 7 = PS (4MB page), bit 1 = RW, bit 0 = Present
        page_directory[i] = (i * 0x400000) | 0x83; 
    }
    
    // Enable PSE
    asm volatile("mov %%cr4, %%eax; or $0x10, %%eax; mov %%eax, %%cr4" ::: "eax");
    // Load PD
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));
    // Enable Paging
    asm volatile("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0" ::: "eax");
}
C_EOF

# 2. Kernel Heap
cat << 'H_EOF' > memory/kheap.h
#pragma once
#include "../include/types.h"
void* kmalloc(uint32_t size);
H_EOF

cat << 'C_EOF' > memory/kheap.cpp
#include "kheap.h"
static uint32_t placement_addr = 0x1000000; // 16MB
void* kmalloc(uint32_t size) {
    uint32_t tmp = placement_addr;
    placement_addr += size;
    return (void*)tmp;
}
C_EOF

# 3. Mouse Driver
cat << 'H_EOF' > drivers/mouse.h
#pragma once
#include "../include/types.h"
extern int mouse_x;
extern int mouse_y;
extern bool mouse_left_click;
void mouse_init();
extern "C" void mouse_handler();
H_EOF

cat << 'C_EOF' > drivers/mouse.cpp
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
C_EOF

# 4. VESA Graphics
cat << 'H_EOF' > drivers/vesa.h
#pragma once
#include "../include/types.h"
struct VesaInfo {
    uint32_t framebuffer;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
};
void vesa_init();
void vesa_put_pixel(uint16_t x, uint16_t y, uint32_t color);
void vesa_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void vesa_draw_char(char c, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg);
void vesa_print(const char* str, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg);
void vesa_clear(uint32_t color);
void vesa_swap_buffers();
H_EOF

cat << 'C_EOF' > drivers/vesa.cpp
#include "vesa.h"
#include "../memory/kheap.h"
#include "../include/string.h"

static VesaInfo vesa_info;
static uint32_t* backbuffer;
static uint8_t* bios_font = (uint8_t*)0xFFA6E;

void vesa_init() {
    uint8_t* mode_info = (uint8_t*)0x600;
    vesa_info.pitch = *(uint16_t*)(mode_info + 16);
    vesa_info.width = *(uint16_t*)(mode_info + 18);
    vesa_info.height = *(uint16_t*)(mode_info + 20);
    vesa_info.bpp = *(uint8_t*)(mode_info + 25);
    vesa_info.framebuffer = *(uint32_t*)(mode_info + 40);
    backbuffer = (uint32_t*)kmalloc(vesa_info.width * vesa_info.height * 4);
}

void vesa_put_pixel(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= vesa_info.width || y >= vesa_info.height) return;
    backbuffer[y * vesa_info.width + x] = color;
}

void vesa_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    for (uint16_t cy = y; cy < y + h; cy++) {
        for (uint16_t cx = x; cx < x + w; cx++) {
            vesa_put_pixel(cx, cy, color);
        }
    }
}

void vesa_draw_char(char c, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
    uint8_t* glyph = bios_font + c * 8;
    for (int cy = 0; cy < 8; cy++) {
        for (int cx = 0; cx < 8; cx++) {
            if (glyph[cy] & (0x80 >> cx)) {
                vesa_put_pixel(x + cx, y + cy, fg);
            } else {
                vesa_put_pixel(x + cx, y + cy, bg); // background pixel
            }
        }
    }
}

void vesa_print(const char* str, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*str) {
        vesa_draw_char(*str, cx, y, fg, bg);
        cx += 8;
        str++;
    }
}

void vesa_clear(uint32_t color) {
    for (int i = 0; i < vesa_info.width * vesa_info.height; i++) {
        backbuffer[i] = color;
    }
}

void vesa_swap_buffers() {
    uint32_t* fb = (uint32_t*)vesa_info.framebuffer;
    memcpy(fb, backbuffer, vesa_info.width * vesa_info.height * 4);
}
C_EOF

# 5. Window Manager
cat << 'H_EOF' > ui/gui.h
#pragma once
void gui_start();
H_EOF

cat << 'C_EOF' > ui/gui.cpp
#include "gui.h"
#include "../drivers/vesa.h"
#include "../drivers/mouse.h"

void gui_start() {
    while (true) {
        vesa_clear(0x008080); // Teal desktop
        
        // Window
        vesa_draw_rect(100, 100, 400, 300, 0xC0C0C0); 
        vesa_draw_rect(100, 100, 400, 20, 0x000080);  
        vesa_print("MyOS Graphical Interface", 105, 106, 0xFFFFFF, 0x000080);
        
        vesa_draw_rect(110, 130, 380, 260, 0xFFFFFF); 
        vesa_print("Welcome to Stage 15 GUI!", 115, 135, 0x000000, 0xFFFFFF);
        vesa_print("You successfully built a Window Manager!", 115, 150, 0x000000, 0xFFFFFF);
        
        if (mouse_left_click) {
            vesa_print("Mouse is clicked!", 115, 170, 0xFF0000, 0xFFFFFF);
        }
        
        // Mouse cursor
        vesa_draw_rect(mouse_x, mouse_y, 6, 6, 0xFFFFFF);
        vesa_draw_rect(mouse_x+1, mouse_y+1, 4, 4, 0x000000);
        
        vesa_swap_buffers();
    }
}
C_EOF

