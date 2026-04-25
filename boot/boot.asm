; =============================================================================
; boot.asm — Stage 4 Bootloader
; Loads kernel from disk into 0x10000, enters Protected Mode, jumps to kernel
; =============================================================================

BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Save drive number — BIOS puts it in DL before jumping to us
    ; We need it later for disk reads
    mov [boot_drive], dl

    mov si, msg_start
    call print_rm

    ; -------------------------------------------------------------------------
    ; Load the kernel from disk into memory at 0x10000
    ;
    ; We use BIOS INT 0x13, function 0x02 (Read Sectors)
    ; The kernel binary is appended right after the boot sector on disk,
    ; so it starts at Cylinder 0, Head 0, Sector 2.
    ;
    ; We read 64 sectors = 32KB. That's enough for many stages ahead.
    ; ES:BX = destination. We want 0x10000, so ES=0x1000, BX=0x0000
    ; -------------------------------------------------------------------------
    mov si, msg_loading
    call print_rm

    mov ax, 0x1000              ; ES = 0x1000
    mov es, ax                  ; ES:BX = 0x1000:0x0000 = physical 0x10000
    xor bx, bx                  ; BX = 0

    mov ah, 0x02                ; INT 0x13 function 02h = Read Sectors
    mov al, 15                  ; Read 15 sectors — fits in 1 track (max 18)
    mov ch, 0                   ; Cylinder 0
    mov cl, 2                   ; Sector 2 (sector 1 is the boot sector)
    mov dh, 0                   ; Head 0
    mov dl, [boot_drive]        ; Drive number saved from entry
    int 0x13                    ; Call BIOS disk service

    ; Check if read succeeded — BIOS sets carry flag on error
    jc disk_error

    mov si, msg_loaded
    call print_rm

    ; -------------------------------------------------------------------------
    ; Enable A20
    ; -------------------------------------------------------------------------
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al

    ; -------------------------------------------------------------------------
    ; Enter Protected Mode
    ; -------------------------------------------------------------------------
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp CODE_SEG:pm_entry       ; Far jump — flushes pipeline, loads CS


; =============================================================================
; Disk error handler — print message and hang
; =============================================================================
disk_error:
    mov si, msg_diskerr
    call print_rm
.hang:
    hlt
    jmp .hang


; =============================================================================
; 16-bit print function
; =============================================================================
print_rm:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    popa
    ret


; =============================================================================
; Data
; =============================================================================
boot_drive  db 0

msg_start   db '[RM] Bootloader started', 0x0D, 0x0A, 0
msg_loading db '[RM] Loading kernel from disk...', 0x0D, 0x0A, 0
msg_loaded  db '[RM] Kernel loaded at 0x10000', 0x0D, 0x0A, 0
msg_diskerr db '[RM] DISK READ ERROR! Halting.', 0x0D, 0x0A, 0


; =============================================================================
; GDT
; =============================================================================
gdt_start:
gdt_null:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start   ; 0x08
DATA_SEG equ gdt_data - gdt_start   ; 0x10


; =============================================================================
; 32-bit Protected Mode entry
; =============================================================================
BITS 32
pm_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000            ; Set up 32-bit stack

    ; Jump to kernel_entry in the loaded kernel binary
    ; The kernel was loaded at 0x10000.
    ; kernel_entry is the very first thing in the kernel binary,
    ; so it sits exactly at 0x10000.
    jmp 0x10000


; =============================================================================
; Boot sector padding
; =============================================================================
BITS 16
times 510 - ($ - $$) db 0
dw 0xAA55
