; =============================================================================
; idt_flush.asm
; Contains:
;   1. idt_flush   — runs lidt, called from C++ idt_init()
;   2. isr0–isr31  — one tiny stub per CPU exception vector
;   3. isr_common  — shared stub that saves all registers and calls C++
; =============================================================================

BITS 32
section .text

; =============================================================================
; idt_flush — load the IDT register
; Called from C++ as: idt_flush(uint32_t idtr_address)
; =============================================================================
[global idt_flush]
idt_flush:
    mov eax, [esp+4]        ; Get address of IDTRegister struct
    lidt [eax]              ; Load IDT: reads 6 bytes (limit + base) from [eax]
    ret

; =============================================================================
; MACRO: ISR stub without an error code
;
; For exceptions that do NOT push an error code, we push a dummy 0
; so the stack layout is identical for all handlers.
; This lets isr_common and InterruptFrame work uniformly.
;
; Stack after this macro executes (before isr_common):
;   [esp+0] = int_no
;   [esp+4] = err_code (0 — dummy)
;   [esp+8] = eip      (pushed by CPU)
;   ...
; =============================================================================
%macro ISR_NO_ERR 1
[global isr%1]
isr%1:
    cli                     ; Disable interrupts (belt-and-suspenders)
    push dword 0            ; Push fake error code = 0
    push dword %1           ; Push exception vector number
    jmp isr_common          ; Jump to shared handler
%endmacro

; =============================================================================
; MACRO: ISR stub WITH an error code
;
; The CPU already pushed the error code before jumping here.
; We just push the vector number and jump to the common handler.
;
; Stack after this macro executes (before isr_common):
;   [esp+0] = int_no
;   [esp+4] = err_code     (pushed by CPU — real error info)
;   [esp+8] = eip          (pushed by CPU)
;   ...
; =============================================================================
%macro ISR_ERR 1
[global isr%1]
isr%1:
    cli                     ; Disable interrupts
    push dword %1           ; Push exception vector number
                            ; (error code already on stack from CPU)
    jmp isr_common          ; Jump to shared handler
%endmacro

; =============================================================================
; Instantiate all 32 exception stubs using the macros above.
; Which ones have error codes: 8, 10, 11, 12, 13, 14, 17
; Everything else: no error code.
; =============================================================================
ISR_NO_ERR 0    ; #DE  Divide Error
ISR_NO_ERR 1    ; #DB  Debug
ISR_NO_ERR 2    ;      NMI
ISR_NO_ERR 3    ; #BP  Breakpoint
ISR_NO_ERR 4    ; #OF  Overflow
ISR_NO_ERR 5    ; #BR  BOUND Range Exceeded
ISR_NO_ERR 6    ; #UD  Invalid Opcode
ISR_NO_ERR 7    ; #NM  Device Not Available
ISR_ERR    8    ; #DF  Double Fault          (error code always 0)
ISR_NO_ERR 9    ;      Coprocessor Overrun
ISR_ERR    10   ; #TS  Invalid TSS
ISR_ERR    11   ; #NP  Segment Not Present
ISR_ERR    12   ; #SS  Stack-Segment Fault
ISR_ERR    13   ; #GP  General Protection Fault
ISR_ERR    14   ; #PF  Page Fault
ISR_NO_ERR 15   ;      Reserved
ISR_NO_ERR 16   ; #MF  x87 FP Error
ISR_ERR    17   ; #AC  Alignment Check
ISR_NO_ERR 18   ; #MC  Machine Check
ISR_NO_ERR 19   ; #XM  SIMD FP Exception
ISR_NO_ERR 20   ; #VE  Virtualization Exception
ISR_NO_ERR 21   ;      Reserved
ISR_NO_ERR 22   ;      Reserved
ISR_NO_ERR 23   ;      Reserved
ISR_NO_ERR 24   ;      Reserved
ISR_NO_ERR 25   ;      Reserved
ISR_NO_ERR 26   ;      Reserved
ISR_NO_ERR 27   ;      Reserved
ISR_NO_ERR 28   ;      Reserved
ISR_NO_ERR 29   ;      Reserved
ISR_NO_ERR 30   ;      Reserved
ISR_NO_ERR 31   ;      Reserved


; =============================================================================
; isr_common — shared handler stub, called by all isr0–isr31
;
; At entry, the stack looks like this (from bottom/high to top/low):
;
;   [CPU pushed automatically]
;   EFLAGS
;   CS
;   EIP            ← address of faulting instruction
;   [err_code]     ← pushed by CPU (exceptions 8,10-14,17) or our dummy 0
;
;   [our stub pushed]
;   int_no         ← which exception (0-31)
;
;   [isr_common pushes]
;   pusha results  ← EAX ECX EDX EBX ESP EBP ESI EDI
;
; We then push DS for segment context, call isr_handler(frame*),
; restore everything, and return from the interrupt with iret.
; =============================================================================
[extern isr_handler]        ; C++ function defined in isr.cpp

isr_common:
    pusha                   ; Push EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI (8 × 4 = 32 bytes)

    ; Save and reload data segment
    ; The handler runs in kernel context, so DS must be kernel data selector
    mov ax, ds
    push eax                ; Save current DS (for later restore)
    mov ax, 0x10            ; Kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call the C++ handler
    ; Pass a pointer to the top of the stack as the InterruptFrame* argument
    ; At this point ESP points to the bottom of our saved register block
    push esp                ; Argument: pointer to InterruptFrame on stack
    call isr_handler        ; Call C++ handler
    add esp, 4              ; Clean up the argument (cdecl caller cleans up)

    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                    ; Restore EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX

    ; Clean up the int_no and err_code we pushed before isr_common
    add esp, 8              ; Skip past err_code and int_no

    ; Return from interrupt
    ; iret pops: EIP, CS, EFLAGS (and ESP, SS if privilege change occurred)
    ; This restores execution to exactly where the exception occurred
    iret

; =============================================================================
; IRQ handlers — hardware interrupt stubs
;
; Unlike CPU exception stubs, IRQ handlers do NOT need to push an error code
; (hardware never pushes one). We also don't call isr_handler — we call the
; specific C++ handler directly (keyboard_handler, timer_handler, etc.)
;
; We DO need to save/restore registers because we're interrupting arbitrary
; kernel code mid-execution.
; =============================================================================

; =============================================================================
; irq1_handler — PS/2 Keyboard (IRQ1 → vector 33)
; =============================================================================
[extern keyboard_handler]       ; Defined in drivers/keyboard.cpp
[global irq1_stub]

irq1_stub:
    cli                         ; Disable interrupts during handler
    pusha                       ; Save all general-purpose registers

    ; Reload data segment registers to kernel data selector
    ; (same reason as isr_common — ensure we're using kernel segment)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call keyboard_handler       ; Call C++ keyboard handler
                                ; keyboard_handler sends EOI before returning

    popa                        ; Restore all registers
    sti                         ; Re-enable interrupts
    iret                        ; Return from interrupt (restores EIP, CS, EFLAGS)

