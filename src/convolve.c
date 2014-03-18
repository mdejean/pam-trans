#include <string.h> //memset memcpy
#include <stdbool.h>
#include <string.h>

#include "bad-math.h"
#include "sample.h"
#include "convolve.h"

//no malloc
sample_t pulse_storage[MAX_PULSE_LENGTH];
sample_t edge_symbols_storage[MAX_PULSE_SYMBOLS]; //no need to be skimpy

bool convolve_init_srrc(
    convolve_state* state,
    float beta,
    size_t overlap,
    size_t M) {
  if (overlap * 2 * M > MAX_PULSE_LENGTH) return false;
  if (overlap > MAX_PULSE_SYMBOLS) return false;
  state->beta = beta;
  state->overlap = overlap;
  state->M = M;
  state->pulse_shape_len = 2 * overlap * M;
  state->amplitude_corr = (1+M_PI)/M_PI * 4.0f *beta/f_sqrt((float)M) / 0.9f;
  //singleton :(
  state->edge_symbols = edge_symbols_storage;
  memset(state->edge_symbols, 0, overlap * sizeof(sample_t));
  state->pulse_shape = pulse_storage;

  float k;
  for (size_t i = 0; i < state->pulse_shape_len; i++) {
    k = (float)i - state->pulse_shape_len/2 + 1e-5f; //should actually use sinc instead of this 1e-5 silliness
    // 0.9 is a fudge factor, should actually be the 1/the sum of 2*overlap samples at the sample point
    state->pulse_shape[i] = float_to_sample(M_PI/(1+M_PI) * 0.9f *

            ( f_cos( (1+beta) * M_PI * k/M) +   f_sin((1-beta) * M_PI * k/M)
//          (                                ----------------------------  )
                                                 /(4*beta*k/M) )
//          -------------------------------------------------------------
                 / ( M_PI * (1 - 16 * (beta*k/M)*(beta*k/M) ))
        );
  }
  return true;
}

//Make the envelope from the symbols (upsampling and then convolution w/ pulse shape)

size_t convolve(
    convolve_state* state,
    const sample_t* symbols,
    size_t num_symbols,
    sample_t* envelope,
    size_t envelope_s) {//TODO: envelope_s should be used, or at least asserted!
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
  return num_symbols - state->overlap;
}

