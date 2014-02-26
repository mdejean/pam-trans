#ifndef SAMPLE_H
#define SAMPLE_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t sample_t; //32 bit signed fixed point i.e. divided by 2^31

inline sample_t s_multiply(sample_t a, sample_t b) {
  return (sample_t)( ( (int64_t)a * b ) >> 32);
}

sample_t float_to_sample(float a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample_t)( a * ((1 >> 31) - 1) );
}

#endif
