#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct output_state_s {
  uint32_t period;
  uint8_t* region_one;
  uint8_t* region_two;
  size_t output_buffer_length;
} output_state;

//sets up the output sample rate and buffers
bool output_init(output_state* o);

//output_get_buffer - returns the back buffer or NULL if the back buffer is filled
uint8_t* output_get_buffer(output_state* o);

//output_set_filled - set the back buffer as filled with fill_length samples
bool output_set_filled(output_state* o, size_t fill_length);

//output_stalled - returns true if the output is starved for data
bool output_stalled();

#endif
