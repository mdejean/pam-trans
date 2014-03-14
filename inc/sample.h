#ifndef SAMPLE_H
#define SAMPLE_H

#include <stddef.h>
#include <stdint.h>

#define SAMPLE_MAX INT32_MAX
#define SAMPLE_MIN INT32_MIN

typedef int32_t sample_t; //32 bit signed fixed point i.e. divided by 2^31

#ifndef SAMPLE_NO_INLINE
inline sample_t s_multiply(sample_t a, sample_t b) {
  return (sample_t)( ( (int64_t)a * b ) >> 32);
}

inline sample_t float_to_sample(float a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample_t)( a * SAMPLE_MAX );
}

inline sample_t double_to_sample(double a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample_t)( a * SAMPLE_MAX );
}

inline uint8_t sample_to_uint8(sample_t a) {
  return (uint8_t)(((int64_t)SAMPLE_MAX + 1 + a) >> 24);
}
#endif

#endif
