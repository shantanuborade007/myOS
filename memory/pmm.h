// =============================================================================
// pmm.h — Physical Memory Manager
// Bitmap-based page allocator for 4KB physical pages
// =============================================================================

#pragma once
#include "../include/types.h"

// =============================================================================
// Constants
// =============================================================================
static const uint32_t PAGE_SIZE       = 4096;       // 4KB pages
static const uint32_t BITMAP_ADDR     = 0x100000;   // Bitmap at 1MB mark
static const uint32_t E820_COUNT_ADDR = 0x500;      // Where bootloader stored count
static const uint32_t E820_MAP_ADDR   = 0x504;      // Where entries start

// =============================================================================
// E820 memory map entry — matches what the bootloader stored
// __attribute__((packed)) ensures no padding between fields
// =============================================================================
struct E820Entry {
    uint64_t base;          // Region base address
    uint64_t length;        // Region length in bytes
    uint32_t type;          // 1=usable, 2=reserved, 3=ACPI, 4=NVS, 5=bad
    uint32_t acpi_attrs;    // ACPI extended attributes (we ignore this)
} __attribute__((packed));

// E820 region types
namespace E820Type {
    static const uint32_t USABLE   = 1;
    static const uint32_t RESERVED = 2;
    static const uint32_t ACPI     = 3;
    static const uint32_t NVS      = 4;
    static const uint32_t BAD      = 5;
}

// =============================================================================
// PMM statistics — for the mem command in Stage 10
// =============================================================================
struct PMMStats {
    uint32_t total_pages;       // Total physical pages detected
    uint32_t usable_pages;      // Pages in usable E820 regions
    uint32_t used_pages;        // Currently allocated pages
    uint32_t free_pages;        // Currently free pages
    uint32_t total_bytes;       // Total RAM in bytes
    uint32_t usable_bytes;      // Usable RAM in bytes
};

// =============================================================================
// Public API
// =============================================================================

// Initialize PMM: parse E820 map, build bitmap, mark reserved regions
void pmm_init();

// Allocate one physical page (4KB aligned).
// Returns the physical address of the page, or 0 on failure.
uint32_t pmm_alloc_page();

// Allocate 'count' contiguous physical pages.
// Returns physical address of first page, or 0 on failure.
uint32_t pmm_alloc_pages(uint32_t count);

// Free a previously allocated page.
// addr must be 4KB-aligned and previously returned by pmm_alloc_page.
void pmm_free_page(uint32_t addr);

// Free 'count' contiguous pages starting at addr.
void pmm_free_pages(uint32_t addr, uint32_t count);

// Get memory statistics (for shell 'mem' command)
PMMStats pmm_get_stats();

// Mark a physical region as used (used internally + exposed for kernel subsystems)
void pmm_mark_used(uint32_t base, uint32_t length);

// Mark a physical region as free
void pmm_mark_free(uint32_t base, uint32_t length);

// Print full memory map and stats to VGA (for shell 'mem' command)
void pmm_print_info();
