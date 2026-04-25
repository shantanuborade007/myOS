; =============================================================================
; gdt_flush.asm
;
; Called from C++ as: gdt_flush(uint32_t gdtr_addr)
; Loads the new GDT and reloads all segment registers.
;
; Why assembly?
;   1. lgdt is a privileged instruction with no C++ equivalent
;   2. Reloading CS requires a far jump — impossible to express in C++
;   3. The far jump must be to a known code label — needs asm precision
; =============================================================================

BITS 32

[global gdt_flush]          ; Export symbol so C++ linker can find it

gdt_flush:
    ; ── Step 1: Get the argument ──────────────────────────────────────────────
    ; C calling convention (cdecl): first argument is at [esp+4]
    ; (esp+0 is the return address pushed by the 'call' instruction)
    mov eax, [esp+4]        ; EAX = address of our GDTRegister struct

    ; ── Step 2: Load the GDT ─────────────────────────────────────────────────
    ; lgdt reads 6 bytes from [eax]:
    ;   bytes 0-1 : limit (size of GDT - 1)
    ;   bytes 2-5 : base  (linear address of GDT array)
    lgdt [eax]

    ; ── Step 3: Reload data segment registers ────────────────────────────────
    ; After lgdt, the CPU has the new GDT in GDTR but the segment registers
    ; still cache the OLD descriptors. We must reload them with new selectors.
    ; 0x10 = selector for entry 2 = kernel data segment (index 2 × 8 = 0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; ── Step 4: Reload CS via a far jump ─────────────────────────────────────
    ; CS cannot be loaded with MOV — it can only be changed by a far jump/call.
    ; A far jump loads both CS (from the segment part) and EIP (from the offset).
    ;
    ; Syntax: jmp segment_selector:offset
    ;   0x08  = selector for entry 1 = kernel code segment
    ;   .done = the label right below — we're just jumping one instruction ahead
    ;
    ; The CPU will:
    ;   1. Look up selector 0x08 in the new GDT
    ;   2. Validate the descriptor
    ;   3. Load the new CS descriptor cache
    ;   4. Set EIP to the address of .done
    ;   5. Continue executing at .done — now fully using the new GDT
    jmp 0x08:.done

.done:
    ; ── Step 5: Return to C++ ────────────────────────────────────────────────
    ; All segment registers now use the new GDT descriptors.
    ; The old bootloader GDT is no longer referenced.
    ret