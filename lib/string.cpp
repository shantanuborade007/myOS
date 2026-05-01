#include "../include/string.h"

extern "C" {
    void* memset(void* dest, int val, uint32_t len) {
        uint8_t* ptr = static_cast<uint8_t*>(dest);
        while (len-- > 0) *ptr++ = val;
        return dest;
    }

    void* memcpy(void* dest, const void* src, uint32_t len) {
        uint8_t* d = static_cast<uint8_t*>(dest);
        const uint8_t* s = static_cast<const uint8_t*>(src);
        while (len-- > 0) *d++ = *s++;
        return dest;
    }

    uint32_t strlen(const char* str) {
        uint32_t len = 0;
        while (str[len]) len++;
        return len;
    }

    int strcmp(const char* s1, const char* s2) {
        while (*s1 && (*s1 == *s2)) {
            s1++; s2++;
        }
        return *reinterpret_cast<const unsigned char*>(s1) - *reinterpret_cast<const unsigned char*>(s2);
    }

    int strncmp(const char* s1, const char* s2, uint32_t n) {
        while (n && *s1 && (*s1 == *s2)) {
            ++s1; ++s2; --n;
        }
        if (n == 0) return 0;
        return *reinterpret_cast<const unsigned char*>(s1) - *reinterpret_cast<const unsigned char*>(s2);
    }
}
