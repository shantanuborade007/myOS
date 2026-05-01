#include "kheap.h"
static uint32_t placement_addr = 0x1000000; // 16MB
void* kmalloc(uint32_t size) {
    uint32_t tmp = placement_addr;
    placement_addr += size;
    return (void*)tmp;
}
