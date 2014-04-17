#ifndef UTILITY_H
#define UTILITY_H

#include <stddef.h>
#include <stdint.h>

//Writes a float as a NON-TERMINATED string
//returns the number of characters written
#define SIGNIFICANT_FIGURES 4
size_t float_to_string(char* s, size_t len, float f);

size_t uint32_to_string(char* s, size_t len, uint32_t f);

#endif
