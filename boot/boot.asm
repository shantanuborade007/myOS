; =============================================================================
; boot.asm — Stage 9 Bootloader
; Queries BIOS E820 memory map before entering Protected Mode,
; stores results at 0x500 for the kernel to read.
; =============================================================================

BITS 16
ORG 0x7C00

; =============================================================================
; Memory map storage layout at 0x500:
;   0x500        : uint32_t  — number of E820 entries found
;   0x504        : E820Entry — first entry (24 bytes each)
;   0x504+24     : E820Entry — second entry
;   ... up to 32 entries max (32 × 24 = 768 bytes, fits well below 0x7C00)
;
; E820Entry layout (24 bytes):
;   +0  : uint64_t base
;   +8  : uint64_t length
;   +16 : uint32_t type
;   +20 : uint32_t acpi_attrs
; =============================================================================
E820_COUNT_ADDR equ 0x500       ; Where we store entry count
E820_MAP_ADDR   equ 0x504       ; Where entries start
E820_ENTRY_SIZE equ 24          ; Bytes per entry
E820_MAX        equ 32          ; Max entries we'll store

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_start
    call print_rm

    ; -------------------------------------------------------------------------
    ; Query E820 memory map
    ; Must happen in Real Mode before Protected Mode switch
    ; -------------------------------------------------------------------------
    call query_e820

    ; -------------------------------------------------------------------------
    ; Load kernel from disk into 0x10000
    ; -------------------------------------------------------------------------
    mov si, msg_loading
    call print_rm

    mov ax, 0x1000
    mov es, ax
    xor bx, bx

    mov ah, 0x02
    mov al, 50
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_loaded
    call print_rm

    ; -------------------------------------------------------------------------
    ; Enter Protected Mode
    ; -------------------------------------------------------------------------
    cli
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al

    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp CODE_SEG:pm_entry


; =============================================================================
; query_e820 — call BIOS INT 0x15, EAX=0xE820 to get memory map
;
; Stores results starting at E820_MAP_ADDR (0x504)
; Stores entry count at E820_COUNT_ADDR (0x500)
;
; Registers used (all saved/restored):
;   EAX, EBX, ECX, EDX, EDI, ES
; =============================================================================
query_e820:
    pusha

    ; ES:DI = destination buffer for entries
    ; We write to 0x0000:0x0504 = physical 0x504
    mov ax, 0x0000
    mov es, ax
    mov di, E820_MAP_ADDR       ; EDI = 0x504

    xor ebx, ebx                ; EBX = 0 = "start of list" for BIOS
    xor bp, bp                  ; BP = entry counter (we use BP since it's safe)

    mov dword [E820_COUNT_ADDR], 0  ; Initialize count to 0

.loop:
    ; Set up registers for INT 0x15, EAX=0xE820
    mov eax, 0xE820             ; Function: query system address map
    mov ecx, E820_ENTRY_SIZE    ; Ask for 24 bytes per entry
    mov edx, 0x534D4150         ; Magic: ASCII 'SMAP' — BIOS checks this
    int 0x15                    ; Call BIOS

    ; Check for errors:
    ;   Carry flag set = function not supported or error
    ;   EAX should equal 'SMAP' (0x534D4150) on success
    jc .done                    ; Carry set = done or error
    cmp eax, 0x534D4150         ; EAX must equal 'SMAP'
    jne .done                   ; Wrong magic = BIOS doesn't support E820

    ; Check entry length — some BIOS return fewer than 20 bytes
    test ecx, ecx
    jz .skip                    ; Skip zero-length entries

    ; Valid entry — count it
    inc bp
    add di, E820_ENTRY_SIZE     ; Advance destination pointer

    ; Check if EBX = 0 — BIOS signals "no more entries" by setting EBX to 0
    test ebx, ebx
    jz .done

    ; Check if we've hit our maximum entry limit
    cmp bp, E820_MAX
    jge .done

.skip:
    jmp .loop

.done:
    ; Store the final count
    mov [E820_COUNT_ADDR], bp

    ; Print how many entries we found
    mov si, msg_e820
    call print_rm

    popa
    ret


; =============================================================================
; Disk error handler
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
msg_e820    db '[RM] E820 memory map queried', 0x0D, 0x0A, 0
msg_loading db '[RM] Loading kernel...', 0x0D, 0x0A, 0
msg_loaded  db '[RM] Kernel loaded at 0x10000', 0x0D, 0x0A, 0
msg_diskerr db '[RM] DISK READ ERROR!', 0x0D, 0x0A, 0


; =============================================================================
; GDT
; =============================================================================
gdt_start:
gdt_null:   dd 0x0,       0x0
gdt_code:   dw 0xFFFF,    0x0000
            db 0x00,      10011010b, 11001111b, 0x00
gdt_data:   dw 0xFFFF,    0x0000
            db 0x00,      10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start


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
    mov esp, 0x90000
    jmp 0x10000


; =============================================================================
; Boot sector padding
; =============================================================================
BITS 16
times 510 - ($ - $$) db 0
dw 0xAA55
