#ifndef UPCONVERT_H
#define UPCONVERT_H

#include <stdbool.h>
#include <stddef.h>

#include "sample.h"

#define UPCONVERT_MAX_N 2000

typedef struct upconvert_state_ {
  size_t M; //carrier samples per envelope sample
  size_t N; //carrier length in samples
  //carrier frequency = baud rate * envelope upsampling factor * M/N
  sample_t* carrier;
  size_t phase; //current position in the carrier
} upconvert_state;

bool upconvert_init(upconvert_state* s, size_t M, size_t N);
//TODO: add alternate init functions for carrier frequencies other than M/N


/*upconvert - convert amplitude envelope to modulated carrier,
  also decimate to 8bit, also optionally upsample
envelope - envelope to mix
envelope_len - number of envelope samples to mix
signal - buffer in which to put the IF signal
signal_s - size of that buffer
*/
size_t upconvert(upconvert_state* s,
  const sample_t* envelope,
  size_t envelope_len,
  uint8_t* signal,
  size_t signal_s);

#endif
