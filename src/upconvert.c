#include <math.h>
#ifndef M_PI
#define M_PI (4.0*atan(1))
#endif

#include "upconvert.h"


//no malloc
sample_t carrier_storage[UPCONVERT_MAX_N];

bool upconvert_init(upconvert_state* s, size_t M, size_t N) {
  if (N < 2 || N > UPCONVERT_MAX_N) {
    return false;
  }
  s->M = M;
  s->N = N;
  s->phase = 0;
  s->carrier = carrier_storage;
  for (size_t i = 0; i < N; i++) {
    s->carrier[i] = double_to_sample(cos((i*2.0*M_PI) / N));
  }
  return true;
}

size_t upconvert(upconvert_state* s, 
    const sample_t* envelope, 
    size_t envelope_len, 
    sample_t* signal,
    size_t signal_s) {
  if (signal_s < s->M * envelope_len) {
    return 0;
  }
  
  size_t phase = s->phase; //just making sure it gets stored
  
  for (size_t i = 0; i < envelope_len; i++) {
    //each sample in the envelope M times
    for (size_t k = 0; k < s->M; k++) {
      if (phase >= s->N) phase = 0;
      signal[i * s->M + k] = s_multiply(s->carrier[phase], envelope[i]);
      phase++;
    }
  }
  
  s->phase = phase;
  return s->M * envelope_len;
}
  
  
