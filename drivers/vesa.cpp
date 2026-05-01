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
