#include <string.h> //memset memcpy
#include <stdbool.h>
#include <string.h>

#include <math.h>

#include "sample.h"
#include "convolve.h"

//should actually be the 1/the sum of 2*overlap samples at the sample point
#define FUDGE_FACTOR 0.8f

//no malloc
sample_t pulse_storage[MAX_PULSE_LENGTH];
sample_t edge_symbols_storage[MAX_PULSE_SYMBOLS];

bool convolve_init_srrc(convolve_state* state) {
  if (state->overlap * 2 * state->M > MAX_PULSE_LENGTH) return false;
  if (state->overlap > MAX_PULSE_SYMBOLS) return false;
  state->pulse_shape_len = 2 * state->overlap * state->M;
  state->amplitude_corr = (1+M_PI)/M_PI * 4.0f *state->beta/sqrtf((float)state->M) / FUDGE_FACTOR;
  //singleton :(
  state->edge_symbols = edge_symbols_storage;
  memset(state->edge_symbols, 0, state->overlap * sizeof(sample_t));
  state->pulse_shape = pulse_storage;

  float k;
  for (size_t i = 0; i < state->pulse_shape_len; i++) {
    k = (float)i - state->pulse_shape_len/2 + 1e-5f; //should actually use sinc instead of this 1e-5 silliness
    state->pulse_shape[i] = float_to_sample(M_PI/(1+M_PI) * FUDGE_FACTOR *

            (cosf( (1+state->beta) * M_PI * k/state->M) +   sinf((1-state->beta) * M_PI * k/state->M)
//          (                                ----------------------------  )
                                                 /(4*state->beta*k/state->M) )
//          -------------------------------------------------------------
                 / ( M_PI * (1 - 16 * (state->beta*k/state->M)*(state->beta*k/state->M) ))
        );
  }
  return true;
}

//Make the envelope from the symbols (upsampling and then convolution w/ pulse shape)
size_t convolve(
    convolve_state* restrict state,
    const sample_t* restrict symbols,
    size_t num_symbols,
    sample_t* restrict envelope,
    size_t envelope_len,
    size_t* restrict envelope_used) {//TODO: envelope_s should be used, or at least asserted!
  //allow compiler to make more assumptions
  if (state->overlap < 2 || state->M < 2 || state->pulse_shape_len < 8) return 0;
  
  //only use as many symbols as we can fit
  if ((num_symbols - state->overlap) * state->M > envelope_len) {
    num_symbols = envelope_len / state->M + state->overlap;
  }

  //first deal with the symbols from the previous block
  for (int i = 0; i < state->overlap; i++) {
    //       how much of the pulse makes it into this block
    for (int k = state->pulse_shape_len - i * state->M;
	k < state->pulse_shape_len; k++) { //todo: make sure we're not dereferencing every cycle
      //consider: 2*overlap * M = pulse_shape_len
      //          -pulse_shape_len/2+k => that would be the first sample of this block
      envelope[i * state->M - state->pulse_shape_len + k] += s_multiply(
        state->pulse_shape[k],
        state->edge_symbols[i]);
    }
  }

  int i = 0;
  //first width symbols special case
  for (; i < state->overlap; i++) {
    for (int k = state->pulse_shape_len - (state->overlap + i) * state->M;
         k < state->pulse_shape_len; k++) {
      //consider i=0: -overlap*M + pulse_shape_len - overlap*M = 0
      envelope[(i - state->overlap) * state->M + k]
        += s_multiply(state->pulse_shape[k], symbols[i]);
    }
  }

  //main multiply-accumulate loop
  for (; i < num_symbols - 2 * state->overlap; i++) {
    for (size_t k = 0; k < state->pulse_shape_len; k++) {
      envelope[(i - state->overlap) * state->M + k]
        += s_multiply(state->pulse_shape[k], symbols[i]); //TODO: c this emits a MAC instruction
    }
  }

  //last 2*overlap symbols special case
  for (; i < num_symbols; i++) {
    //initially i=num_symbols-2*overlap => (num_symbols - i) * state->M = 2*overlap*M = pulse_shape_len
    for (size_t k = 0; k < (num_symbols - i) * state->M; k++) {
      envelope[(i - state->overlap) * state->M + k]
        += s_multiply(state->pulse_shape[k], symbols[i]);
    }
  }

  //low prio fixme: ignoring the possibilty that we converted less than syms symbols this time
  //set up edge_symbols for next time
  memcpy(state->edge_symbols, &symbols[i - state->overlap], sizeof(sample_t) * state->overlap);
  if (envelope_used != NULL) {
    *envelope_used = (num_symbols - state->overlap) * state->M; //samples
  }
  
  return num_symbols - state->overlap; //symbols
}

