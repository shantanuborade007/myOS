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
