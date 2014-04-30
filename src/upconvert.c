#include <math.h>

#include "upconvert.h"

//no malloc
sample_t carrier_storage[UPCONVERT_MAX_N];

bool upconvert_init(upconvert_state* s) {
  if (s->N < 2 || s->N > UPCONVERT_MAX_N) {
    return false;
  }
  s->phase = 0;
  s->carrier = carrier_storage;
  for (size_t i = 0; i < s->N; i++) {
    s->carrier[i] = float_to_sample(cosf((i*2.0f*M_PI) / s->N));
  }
  return true;
}

size_t upconvert(upconvert_state* s,
    const sample_t* restrict envelope,
    size_t envelope_len,
    uint16_t* restrict signal,
    size_t signal_s) {
  if (signal_s < s->M * envelope_len) {
    return 0;
  }

  size_t phase = s->phase; //just making sure it gets stored

  for (size_t i = 0; i < envelope_len; i++) {
    //each sample in the envelope M times
    for (size_t k = 0; k < s->M; k++) {
      if (phase >= s->N) phase = 0;
      signal[i * s->M + k] = sample_to_12bit(s_multiply(s->carrier[phase], envelope[i]));
      phase++;
    }
  }

  s->phase = phase;
  return s->M * envelope_len;
}


