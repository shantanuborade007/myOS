// =============================================================================
// types.h — Fundamental type definitions for MyOS
//
// In freestanding mode, <stdint.h> may or may not be available depending on
// the compiler. We define our own to be safe and explicit.
// These names match the C99/C++11 standard names you're used to.
// =============================================================================

#pragma once                    // Include guard: only processed once per translation unit

// ── Fixed-width integer types ─────────────────────────────────────────────────

// Unsigned integers
typedef unsigned char       uint8_t;    // 8-bit:  0 to 255
typedef unsigned short      uint16_t;   // 16-bit: 0 to 65,535
typedef unsigned int        uint32_t;   // 32-bit: 0 to 4,294,967,295
typedef unsigned long long  uint64_t;   // 64-bit: 0 to 18,446,744,073,709,551,615

// Signed integers
typedef signed char         int8_t;     // 8-bit:  -128 to 127
typedef signed short        int16_t;    // 16-bit: -32,768 to 32,767
typedef signed int          int32_t;    // 32-bit: -2,147,483,648 to 2,147,483,647
typedef signed long long    int64_t;    // 64-bit

// Size type — used for memory sizes, array indices, pointer arithmetic
// On 32-bit x86 this is 32 bits wide (matches pointer size)
typedef uint32_t            size_t;

// Pointer-sized integer — can hold any pointer value without casting
typedef uint32_t            uintptr_t;

// Boolean
#ifndef __cplusplus
typedef uint8_t             bool;
#define true  1
#define false 0
#endif

// NULL pointer
#define NULL ((void*)0)

// ── Useful macros ─────────────────────────────────────────────────────────────

// Number of elements in a statically-sized array
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Suppress "unused variable" compiler warnings for intentionally unused params
#define UNUSED(x) ((void)(x))
