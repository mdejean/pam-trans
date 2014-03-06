#define SAMPLE_NO_INLINE
#include "sample.h"

sample_t s_multiply(sample_t a, sample_t b) {
  return (sample_t)( ( (int64_t)a * b ) >> 32);
}

sample_t float_to_sample(float a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample_t)( a * SAMPLE_MAX );
}

sample_t double_to_sample(double a) {
  if (a >= 1) a = 1;
  if (a <= -1) a = -1;
  return (sample_t)( a * SAMPLE_MAX );
}
