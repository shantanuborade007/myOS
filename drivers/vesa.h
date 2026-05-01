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
