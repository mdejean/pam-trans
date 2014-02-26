#ifndef SAMPLE_H
#define SAMPLE_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t sample; //32 bit signed fixed point i.e. divided by 2^31

inline sample s_multiply(sample a, sample b) {
  return (sample)( ( (int64_t)a * b ) >> 32);
}

sample float_to_sample(float a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample)( a * ((1 >> 31) - 1) );
}

#endif