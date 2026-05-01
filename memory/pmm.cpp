// =============================================================================
// pmm.cpp — Physical Memory Manager Implementation
// =============================================================================

#include "pmm.h"
#include "../drivers/vga.h"

// =============================================================================
// The bitmap lives at physical address 0x100000 (1MB mark).
// We access it via a pointer cast — no allocation needed, it's a fixed address.
//
// Each bit represents one 4KB page:
//   0 = page is FREE
//   1 = page is USED
//
// The bitmap is stored as an array of uint32_t words (32 pages per word).
// This lets us scan 32 pages at once for efficient allocation.
// =============================================================================
static uint32_t* bitmap = reinterpret_cast<uint32_t*>(BITMAP_ADDR);

// Total number of pages the bitmap tracks.
// Set during pmm_init() based on the highest RAM address found in E820.
static uint32_t total_pages  = 0;
static uint32_t usable_pages = 0;

// Number of words in the bitmap array = ceil(total_pages / 32)
static uint32_t bitmap_words = 0;

// =============================================================================
// Bitmap primitive operations
// =============================================================================

// Set bit N (mark page N as USED)
static inline void bitmap_set(uint32_t page) {
    bitmap[page / 32] |= (1u << (page % 32));
}

// Clear bit N (mark page N as FREE)
static inline void bitmap_clear(uint32_t page) {
    bitmap[page / 32] &= ~(1u << (page % 32));
}

// Test bit N (returns non-zero if page N is USED)
static inline uint32_t bitmap_test(uint32_t page) {
    return bitmap[page / 32] & (1u << (page % 32));
}

// =============================================================================
// pmm_mark_used — mark every page in [base, base+length) as used
//
// We align base DOWN to page boundary and length UP to ensure we
// never leave a partial page marked as free.
// =============================================================================
void pmm_mark_used(uint32_t base, uint32_t length) {
    // Align base down to page boundary
    uint32_t page_start = base / PAGE_SIZE;

    // Align end up to page boundary
    uint32_t page_end   = (base + length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t p = page_start; p < page_end && p < total_pages; p++) {
        bitmap_set(p);
    }
}

// =============================================================================
// pmm_mark_free — mark every page in [base, base+length) as free
// =============================================================================
void pmm_mark_free(uint32_t base, uint32_t length) {
    uint32_t page_start = base / PAGE_SIZE;
    uint32_t page_end   = (base + length) / PAGE_SIZE;  // floor — don't free partial

    for (uint32_t p = page_start; p < page_end && p < total_pages; p++) {
        bitmap_clear(p);
    }
}

// =============================================================================
// pmm_init — initialize the physical memory manager
//
// Steps:
//  1. Read E820 map from 0x500 (written by bootloader)
//  2. Find highest usable address → determines bitmap size
//  3. Mark ALL pages as used (conservative start)
//  4. Mark usable E820 regions as free
//  5. Re-mark reserved regions as used (kernel, bitmap, VGA, etc.)
// =============================================================================
void pmm_init() {

    // ── Step 1: Read E820 entry count ────────────────────────────────────
    uint32_t  entry_count = *reinterpret_cast<uint32_t*>(E820_COUNT_ADDR);
    E820Entry* entries    =  reinterpret_cast<E820Entry*>(E820_MAP_ADDR);

    // ── Step 2: Find highest usable address ──────────────────────────────
    // We need this to know how large the bitmap should be.
    uint32_t highest_addr = 0;
    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].type == E820Type::USABLE) {
            // Only consider low 32 bits (we're in 32-bit mode)
            uint32_t end = static_cast<uint32_t>(
                entries[i].base + entries[i].length);
            if (end > highest_addr) highest_addr = end;
        }
    }

    // If E820 failed or returned nothing, assume 32MB as a safe default
    if (highest_addr == 0) {
        highest_addr = 32 * 1024 * 1024;
    }

    // ── Step 3: Calculate bitmap dimensions ──────────────────────────────
    total_pages  = highest_addr / PAGE_SIZE;
    bitmap_words = (total_pages + 31) / 32;  // ceil(total_pages / 32)

    // ── Step 4: Mark ALL pages as used ───────────────────────────────────
    // Safe starting state — nothing is free until we explicitly free it.
    // This ensures any region not covered by E820 stays marked as used.
    for (uint32_t w = 0; w < bitmap_words; w++) {
        bitmap[w] = 0xFFFFFFFF;             // All bits set = all used
    }

    // ── Step 5: Mark usable E820 regions as free ─────────────────────────
    usable_pages = 0;
    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].type == E820Type::USABLE) {
            uint32_t base   = static_cast<uint32_t>(entries[i].base);
            uint32_t length = static_cast<uint32_t>(entries[i].length);
            pmm_mark_free(base, length);
            usable_pages += length / PAGE_SIZE;
        }
    }

    // ── Step 6: Re-mark reserved regions as used ─────────────────────────
    // These regions must NEVER be allocated, even if E820 says they're usable.

    // Page 0 — real mode IVT, BIOS data, null pointer protection
    pmm_mark_used(0x000000, 0x001000);

    // Bootloader at 0x7C00 (one page covers it)
    pmm_mark_used(0x007000, 0x002000);

    // E820 map storage at 0x500 (one page)
    pmm_mark_used(0x000000, 0x001000);  // same page as IVT, already marked

    // Stack at 0x90000
    pmm_mark_used(0x090000, 0x001000);

    // VGA buffer at 0xB8000
    pmm_mark_used(0x0B8000, 0x008000);

    // BIOS ROM at 0xC0000–0xFFFFF
    pmm_mark_used(0x0C0000, 0x040000);

    // Kernel: from 0x10000 to some end address.
    // We use 0x10000–0x80000 as a conservative estimate
    // (the linker exposes kernel_end but it's complex to use here)
    pmm_mark_used(0x010000, 0x070000);

    // The bitmap itself — mark it as used so we don't allocate over it
    uint32_t bitmap_size_bytes = bitmap_words * 4;
    pmm_mark_used(BITMAP_ADDR, bitmap_size_bytes);
}

// =============================================================================
// pmm_alloc_page — find and allocate one free page
//
// Algorithm:
//   Scan bitmap words (32 pages each) for any word != 0xFFFFFFFF
//   Within that word, find the first 0 bit
//   Set that bit and return the corresponding physical address
//
// Returns 0 on failure (out of memory)
// =============================================================================
uint32_t pmm_alloc_page() {
    for (uint32_t w = 0; w < bitmap_words; w++) {
        // Skip fully-used words — fast path handles most words
        if (bitmap[w] == 0xFFFFFFFF) continue;

        // This word has at least one free page — find which bit
        for (uint32_t b = 0; b < 32; b++) {
            uint32_t page = w * 32 + b;
            if (page >= total_pages) return 0;  // Out of pages

            if (!bitmap_test(page)) {
                bitmap_set(page);               // Mark as used
                return page * PAGE_SIZE;        // Return physical address
            }
        }
    }
    return 0;  // No free pages
}

// =============================================================================
// pmm_alloc_pages — allocate 'count' contiguous pages
//
// For small counts (1 page), delegates to pmm_alloc_page.
// For larger counts, scans for a run of 'count' consecutive free pages.
//
// Returns physical address of first page, or 0 on failure.
// =============================================================================
uint32_t pmm_alloc_pages(uint32_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    // Scan for 'count' consecutive free pages
    uint32_t run_start = 0;
    uint32_t run_len   = 0;

    for (uint32_t page = 0; page < total_pages; page++) {
        if (!bitmap_test(page)) {
            if (run_len == 0) run_start = page;
            run_len++;
            if (run_len == count) {
                // Found a run — mark all pages as used
                for (uint32_t p = run_start; p < run_start + count; p++) {
                    bitmap_set(p);
                }
                return run_start * PAGE_SIZE;
            }
        } else {
            run_len = 0;    // Reset — this page is used, break the run
        }
    }
    return 0;   // No contiguous region found
}

// =============================================================================
// pmm_free_page — return a page to the free pool
// =============================================================================
void pmm_free_page(uint32_t addr) {
    if (addr == 0) return;                  // Never free page 0
    if (addr % PAGE_SIZE != 0) return;      // Must be page-aligned
    uint32_t page = addr / PAGE_SIZE;
    if (page >= total_pages) return;        // Out of range
    bitmap_clear(page);
}

// =============================================================================
// pmm_free_pages — free 'count' contiguous pages
// =============================================================================
void pmm_free_pages(uint32_t addr, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        pmm_free_page(addr + i * PAGE_SIZE);
    }
}

// =============================================================================
// pmm_get_stats — collect current memory statistics
// =============================================================================
PMMStats pmm_get_stats() {
    PMMStats stats;

    // Count free pages by scanning the bitmap
    uint32_t free_count = 0;
    for (uint32_t w = 0; w < bitmap_words; w++) {
        if (bitmap[w] == 0) {
            free_count += 32;   // All 32 pages free — fast path
        } else if (bitmap[w] != 0xFFFFFFFF) {
            // Count zero bits in this word using bit tricks
            uint32_t word = ~bitmap[w];  // Invert: 1s are now the free pages
            // Brian Kernighan's bit counting method
            while (word) {
                word &= (word - 1);
                free_count++;
            }
        }
    }

    stats.total_pages  = total_pages;
    stats.usable_pages = usable_pages;
    stats.used_pages   = total_pages - free_count;
    stats.free_pages   = free_count;
    stats.total_bytes  = total_pages  * PAGE_SIZE;
    stats.usable_bytes = usable_pages * PAGE_SIZE;

    return stats;
}

// =============================================================================
// pmm_print_info — display memory map and stats on VGA screen
// =============================================================================
void pmm_print_info() {
    uint8_t hdr_color  = vga_make_color(VGA_COLOR_YELLOW,      VGA_COLOR_BLACK);
    uint8_t ok_color   = vga_make_color(VGA_COLOR_LIGHT_GREEN,  VGA_COLOR_BLACK);
    uint8_t rsv_color  = vga_make_color(VGA_COLOR_LIGHT_RED,    VGA_COLOR_BLACK);
    uint8_t info_color = vga_make_color(VGA_COLOR_LIGHT_CYAN,   VGA_COLOR_BLACK);
    uint8_t dim_color  = vga_make_color(VGA_COLOR_LIGHT_GRAY,   VGA_COLOR_BLACK);

    // ── E820 Map ──────────────────────────────────────────────────────────
    uint32_t  entry_count = *reinterpret_cast<uint32_t*>(E820_COUNT_ADDR);
    E820Entry* entries    =  reinterpret_cast<E820Entry*>(E820_MAP_ADDR);

    vga_set_color(hdr_color);
    kprintf("[MEM] E820 Memory Map (%u entries):\n", entry_count);

    vga_set_color(dim_color);
    vga_print("  Base               Length             Type\n");
    vga_print("  ------------------ ------------------ ----------\n");

    const char* type_names[] = {
        "Unknown  ",
        "Usable   ",   // 1
        "Reserved ",   // 2
        "ACPI Recl",   // 3
        "ACPI NVS ",   // 4
        "Bad RAM  "    // 5
    };

    for (uint32_t i = 0; i < entry_count; i++) {
        bool usable = (entries[i].type == E820Type::USABLE);
        vga_set_color(usable ? ok_color : rsv_color);

        uint32_t base_lo   = static_cast<uint32_t>(entries[i].base);
        uint32_t base_hi   = static_cast<uint32_t>(entries[i].base >> 32);
        uint32_t len_lo    = static_cast<uint32_t>(entries[i].length);
        uint32_t len_hi    = static_cast<uint32_t>(entries[i].length >> 32);
        uint32_t type      = entries[i].type;
        const char* tname  = (type <= 5) ? type_names[type] : type_names[0];

        if (base_hi || len_hi) {
            // 64-bit address — print both halves
            kprintf("  0x%X%X  0x%X%X  %s\n",
                    base_hi, base_lo, len_hi, len_lo, tname);
        } else {
            kprintf("  0x%X         0x%X         %s\n",
                    base_lo, len_lo, tname);
        }
    }

    // ── Statistics ────────────────────────────────────────────────────────
    PMMStats s = pmm_get_stats();

    vga_print("\n");
    vga_set_color(hdr_color);
    vga_print("[MEM] Physical Memory Statistics:\n");

    vga_set_color(info_color);
    kprintf("  Total pages  : %u  (%u MB)\n",
            s.total_pages,  s.total_bytes  / (1024*1024));
    kprintf("  Usable pages : %u  (%u MB)\n",
            s.usable_pages, s.usable_bytes / (1024*1024));

    vga_set_color(ok_color);
    kprintf("  Free pages   : %u  (%u KB free)\n",
            s.free_pages, s.free_pages * 4);

    vga_set_color(rsv_color);
    kprintf("  Used pages   : %u  (%u KB used)\n",
            s.used_pages, s.used_pages * 4);

    // ── Visual usage bar ──────────────────────────────────────────────────
    vga_set_color(info_color);
    vga_print("\n  Usage: [");

    uint32_t bar_width = 40;
    uint32_t used_bars = 0;
    if (s.total_pages > 0) {
        used_bars = (s.used_pages * bar_width) / s.total_pages;
    }

    vga_set_color(rsv_color);
    for (uint32_t i = 0; i < used_bars; i++)        vga_putchar('#');
    vga_set_color(ok_color);
    for (uint32_t i = used_bars; i < bar_width; i++) vga_putchar('.');
    vga_set_color(info_color);

    uint32_t pct = s.total_pages > 0
                 ? (s.used_pages * 100) / s.total_pages : 0;
    kprintf("] %u%%\n", pct);
}
