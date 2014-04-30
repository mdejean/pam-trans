#ifndef SAMPLE_H
#define SAMPLE_H

#include <stddef.h>
#include <stdint.h>

#define SAMPLE_MAX INT32_MAX
#define SAMPLE_MIN INT32_MIN

#ifndef M_PI
#define M_PI 3.1415926f
#endif //FIXME: this doesn't really belong in this file

typedef int32_t sample_t; //32 bit signed fixed point i.e. divided by 2^31

#ifndef SAMPLE_INLINE
  #define SAMPLE_INLINE inline
#endif // SAMPLE_INLINE

SAMPLE_INLINE sample_t s_multiply(sample_t a, sample_t b) {
  return (sample_t)( ( (int64_t)a * b ) >> 32);
}

SAMPLE_INLINE sample_t float_to_sample(float a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample_t)( a * SAMPLE_MAX );
}

SAMPLE_INLINE uint16_t sample_to_12bit(sample_t a) {
  return (uint16_t)(2048 + (a / (SAMPLE_MAX/4096)));
}

#endif
