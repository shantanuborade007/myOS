#include "gui.h"
#include "../drivers/vesa.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/ata.h"
#include "../drivers/rtc.h"
#include "../include/string.h"

static char notepad_buffer[512] = "Type here... Click 'SAVE' to write to Hard Disk LBA 200";
static int notepad_len = 0;
static bool mouse_was_clicked = false;
static char status_msg[100] = "";

// Window state
static int win_x = 100;
static int win_y = 100;
static bool is_dragging = false;
static int drag_off_x = 0;
static int drag_off_y = 0;

static bool is_inside(int x, int y, int rx, int ry, int rw, int rh) {
    return (x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

void gui_start() {
    notepad_len = strlen(notepad_buffer);
    
    while (true) {
        // 1. Process Keyboard Input
        while (keyboard_has_input()) {
            uint8_t ch = keyboard_read();
            if (ch == '\b') {
                if (notepad_len > 0) notepad_len--;
                notepad_buffer[notepad_len] = '\0';
            } else if (ch >= 32 && ch < 127) {
                if (notepad_len < 511) {
                    notepad_buffer[notepad_len++] = ch;
                    notepad_buffer[notepad_len] = '\0';
                }
            }
        }

        // 2. Process Mouse Clicks & Dragging
        bool click = mouse_left_click;
        bool clicked_now = (click && !mouse_was_clicked);
        mouse_was_clicked = click;

        // Title Bar Dragging (win_x, win_y, 600, 20)
        if (clicked_now && is_inside(mouse_x, mouse_y, win_x, win_y, 600, 20)) {
            is_dragging = true;
            drag_off_x = mouse_x - win_x;
            drag_off_y = mouse_y - win_y;
        }
        
        if (is_dragging) {
            if (click) {
                win_x = mouse_x - drag_off_x;
                win_y = mouse_y - drag_off_y;
            } else {
                is_dragging = false;
            }
        }

        // Save Button (relative to window)
        int save_x = win_x + 20;
        int save_y = win_y + 250;
        if (clicked_now && is_inside(mouse_x, mouse_y, save_x, save_y, 80, 30)) {
            uint8_t sector[512];
            memset(sector, 0, 512);
            memcpy(sector, notepad_buffer, notepad_len);
            ata_write_sector(200, sector);
            memcpy(status_msg, "Saved to Disk LBA 200!", 23);
        }

        // Load Button (relative to window)
        int load_x = win_x + 120;
        int load_y = win_y + 250;
        if (clicked_now && is_inside(mouse_x, mouse_y, load_x, load_y, 80, 30)) {
            uint8_t sector[512];
            ata_read_sector(200, sector);
            sector[511] = '\0';
            memcpy(notepad_buffer, sector, 512);
            notepad_len = strlen(notepad_buffer);
            memcpy(status_msg, "Loaded from Disk LBA 200!", 26);
        }

        // 3. Render Desktop
        vesa_clear(0x008080); // Teal desktop
        
        // Render Window
        vesa_draw_rect(win_x, win_y, 600, 300, 0xC0C0C0); 
        vesa_draw_rect(win_x, win_y, 600, 20, 0x000080);  
        vesa_print("MyOS Notepad - Draggable Window!", win_x + 5, win_y + 6, 0xFFFFFF, 0x000080);
        
        vesa_draw_rect(win_x + 10, win_y + 30, 580, 200, 0xFFFFFF); 
        
        int tx = win_x + 15;
        int ty = win_y + 35;
        for (int i = 0; i < notepad_len; i++) {
            vesa_draw_char(notepad_buffer[i], tx, ty, 0x000000, 0xFFFFFF);
            tx += 8;
            if (tx > win_x + 580) { tx = win_x + 15; ty += 16; }
        }
        
        vesa_draw_rect(save_x, save_y, 80, 30, 0x808080);
        vesa_print("SAVE", save_x + 25, save_y + 10, 0xFFFFFF, 0x808080);
        
        vesa_draw_rect(load_x, load_y, 80, 30, 0x808080);
        vesa_print("LOAD", load_x + 25, load_y + 10, 0xFFFFFF, 0x808080);
        
        vesa_print(status_msg, win_x + 220, win_y + 260, 0x0000FF, 0xC0C0C0);
        
        // 4. Render Taskbar
        vesa_draw_rect(0, 768 - 30, 1024, 30, 0xC0C0C0); // Gray Taskbar
        vesa_draw_rect(5, 768 - 25, 60, 20, 0x808080);   // Start Button
        vesa_print("START", 15, 768 - 19, 0xFFFFFF, 0x808080);
        
        // Get Time and Render Clock
        Time t = rtc_get_time();
        char time_str[9];
        time_str[0] = '0' + (t.hour / 10);
        time_str[1] = '0' + (t.hour % 10);
        time_str[2] = ':';
        time_str[3] = '0' + (t.minute / 10);
        time_str[4] = '0' + (t.minute % 10);
        time_str[5] = ':';
        time_str[6] = '0' + (t.second / 10);
        time_str[7] = '0' + (t.second % 10);
        time_str[8] = '\0';
        
        vesa_print(time_str, 1024 - 80, 768 - 20, 0x000000, 0xC0C0C0);

        // 5. Mouse Cursor
        vesa_draw_rect(mouse_x, mouse_y, 6, 6, 0xFFFFFF);
        vesa_draw_rect(mouse_x+1, mouse_y+1, 4, 4, 0x000000);
        
        vesa_swap_buffers();
    }
}
