#ifndef CONVOLVE_H
#define CONVOLVE_H

#include <stddef.h>
#include <stdbool.h>
#include "sample.h"

#define MAX_PULSE_LENGTH 1000
#define MAX_PULSE_SYMBOLS 32

typedef struct convolve_state_ {
  float beta; //square root raised cosine parameter
  size_t overlap; //number of symbols that each pulse overlaps (1/2 width in symbols)
  size_t M; //oversampling factor
  float amplitude_corr; //factor by which the pulse would need to be multiplied to have a power of 1
  sample_t* pulse_shape;
  size_t pulse_shape_len; //2 * overlap * M
  sample_t* edge_symbols; //overlap symbols from the previous symbol block
} convolve_state;

//Generate the pulse shape
bool convolve_init_srrc(
    convolve_state* state, 
    float beta, 
    size_t overlap,
    size_t M);

/*convolve - convert symbols (at baudrate) to the PAM envelope (at Fs)
 state - convolver settings and state
 symbols - symbols to convert
 num_symbols - the maximum number symbols to convert
 envelope - buffer in which to put the envelope - must be zeroed
 envelope_s - size of that buffer in samples
RETURNS: the actual number of symbols converted
  this will be num_symbols - overlap, since overlap symbols would be 
  affected by the symbols in the next block

*/
size_t convolve(
    convolve_state* state, 
    const sample_t* symbols, 
    size_t num_symbols, 
    sample_t* envelope,
    size_t envelope_s);
    
#endif
