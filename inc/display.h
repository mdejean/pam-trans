#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define DISPLAY_WIDTH 40
#define DISPLAY_HEIGHT 2

void display_init();


//returns true if the display is ready for a character
bool display_ready();

void display_set(char c, uint8_t x, uint8_t y);

#endif