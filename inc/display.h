#ifndef DISPLAY_H
#define DISPLAY_H

#define DISPLAY_WIDTH 80
#define DISPLAY_HEIGHT 2

void display_init();

void display_set(char c, unsigned int x, unsigned int y);

#endif