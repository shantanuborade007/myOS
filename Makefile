# =============================================================================
# Makefile — MyOS Stage 9
# =============================================================================

ASM      = nasm
CXX      = i686-elf-g++
LD       = i686-elf-ld

CXXFLAGS = -ffreestanding -fno-builtin -fno-exceptions -fno-rtti \
           -nostdlib -m32 -O2 -Wall -Wextra -std=c++17 \
           -I./include -I./drivers -I./cpu -I./memory

ASMFLAGS = -f elf32
LDFLAGS  = -T linker.ld

BUILD    = build
IMG      = $(BUILD)/os.img

CPP_SRCS = kernel/kernel.cpp    \
           drivers/vga.cpp      \
           drivers/keyboard.cpp \
           cpu/gdt.cpp          \
           cpu/idt.cpp          \
           cpu/isr.cpp          \
           memory/pmm.cpp

ASM_SRCS = kernel/kernel_entry.asm \
           cpu/gdt_flush.asm       \
           cpu/idt_flush.asm

CPP_OBJS = $(patsubst %.cpp, $(BUILD)/%.o, $(CPP_SRCS))
ASM_OBJS = $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
ALL_OBJS = $(ASM_OBJS) $(CPP_OBJS)

all: $(IMG)
	@echo "Build complete -> $(IMG)"

$(BUILD):
	mkdir -p $(BUILD)/kernel $(BUILD)/drivers $(BUILD)/cpu $(BUILD)/memory

$(BUILD)/boot.bin: boot/boot.asm | $(BUILD)
	$(ASM) -f bin boot/boot.asm -o $@

$(BUILD)/%.o: %.asm | $(BUILD)
	@echo "[ASM] $<"
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD)/%.o: %.cpp | $(BUILD)
	@echo "[CXX] $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kernel.bin: $(ALL_OBJS) linker.ld
	@echo "[LD] Linking..."
	$(LD) $(LDFLAGS) $(ALL_OBJS) -o $@

$(IMG): $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $(IMG)
	truncate -s 1440k $(IMG)

run: $(IMG)
	qemu-system-i386 -drive format=raw,file=$(IMG)

run-debug: $(IMG)
	qemu-system-i386 -drive format=raw,file=$(IMG) \
		-d int,cpu_reset -D $(BUILD)/qemu.log -no-reboot

clean:
	rm -rf $(BUILD)

.PHONY: all run run-debug clean
