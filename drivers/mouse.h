#pragma once
#include "../include/types.h"
extern int mouse_x;
extern int mouse_y;
extern bool mouse_left_click;
void mouse_init();
extern "C" void mouse_handler();
