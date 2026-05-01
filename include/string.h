#pragma once
#include "types.h"

extern "C" {
    void* memset(void* dest, int val, uint32_t len);
    void* memcpy(void* dest, const void* src, uint32_t len);
    uint32_t strlen(const char* str);
    int strcmp(const char* s1, const char* s2);
    int strncmp(const char* s1, const char* s2, uint32_t n);
}
