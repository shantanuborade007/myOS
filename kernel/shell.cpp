#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../memory/pmm.h"
#include "../include/string.h"

static const int MAX_CMD_LEN = 256;
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_len = 0;

static void shell_prompt() {
    vga_set_colors(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("root@myos");
    vga_set_colors(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print(" # ");
}

static void cmd_help() {
    vga_set_colors(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("Available commands:\n");
    vga_print("  help    - Show this help message\n");
    vga_print("  clear   - Clear the screen\n");
    vga_print("  echo    - Print text back to the screen\n");
    vga_print("  mem     - Show physical memory statistics\n");
    vga_print("  panic   - Trigger a deliberate kernel panic (divide by zero)\n");
}

static void cmd_echo(const char* args) {
    vga_print(args);
    vga_print("\n");
}

static void cmd_panic() {
    volatile int a = 1;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}

static void shell_execute() {
    vga_print("\n");

    // Skip leading spaces
    int start = 0;
    while (cmd_buffer[start] == ' ') start++;

    if (cmd_len == 0 || cmd_buffer[start] == '\0') {
        return;
    }

    char* cmd = &cmd_buffer[start];

    // Find the first space to split command from arguments
    char* args = cmd;
    while (*args != ' ' && *args != '\0') args++;
    if (*args == ' ') {
        *args = '\0'; // Null terminate the command part
        args++;       // Point to arguments
        while (*args == ' ') args++; // Skip extra spaces
    }

    vga_set_colors(VGA_COLOR_LIGHT_GRAY, VGA_COLOR_BLACK);

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd, "mem") == 0) {
        pmm_print_info();
    } else if (strcmp(cmd, "panic") == 0) {
        cmd_panic();
    } else {
        vga_set_colors(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("Unknown command: '%s'. Type 'help' for a list of commands.\n", cmd);
    }
}

void shell_start() {
    vga_set_colors(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("\nWelcome to MyOS Shell!\n");
    vga_print("Type 'help' to see available commands.\n\n");
    
    shell_prompt();

    while (true) {
        uint8_t ch = keyboard_getchar();

        if (ch == '\n') {
            cmd_buffer[cmd_len] = '\0';
            shell_execute();
            cmd_len = 0;
            shell_prompt();
        } else if (ch == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                vga_putchar('\b'); // VGA driver handles backspace display
            }
        } else if (ch >= 32 && ch < 127) { // Printable characters
            if (cmd_len < MAX_CMD_LEN - 1) {
                cmd_buffer[cmd_len++] = ch;
                vga_putchar(ch);
            }
        }
    }
}
