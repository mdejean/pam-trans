#include <string.h>
#include <math.h>

#include "utility.h"

size_t float_to_string(char* s, size_t len, float f) {
  if (len < SIGNIFICANT_FIGURES + 5) return 0;//7 = +.e+99
  //the ok method
  if (f<0) {
    f = -f;
    s[0] = '-';
  } else {
    s[0] = '+';
  }
  int exponent_10 = floorf(log10f(f)); 
  int n = f * powf(10, -exponent_10 - 1 + SIGNIFICANT_FIGURES);
  int i;
  for (i = SIGNIFICANT_FIGURES+1; i > 0; i--) {
    s[i] = '0' + (n % 10);
    n /= 10;
    if (i == 3) s[--i] = '.'; //3 = +x.3 0123
  }
  s[SIGNIFICANT_FIGURES+2] = 'e';
  if (exponent_10 < 0) {
    exponent_10 = -exponent_10;
    s[SIGNIFICANT_FIGURES+3] = '-';
  } else {
    s[SIGNIFICANT_FIGURES+3] = '+';
  }
  s[SIGNIFICANT_FIGURES+4] = '0'+exponent_10 / 10;
  s[SIGNIFICANT_FIGURES+5] = '0'+exponent_10 % 10;
  return SIGNIFICANT_FIGURES+5;
}

size_t uint32_to_string(char* s, size_t len, uint32_t n) {
  if (len < 11) return 0;
  //i apologize sincerely for the following
  int i=0;
  #define J(K) if (n>=K || i) { s[i++] = '0'+(n / K); n = n % K; }
  J(1000000000);
  J(100000000);
  J(10000000);
  J(1000000);
  J(100000);
  J(10000);
  J(1000);
  J(100);
  J(10);
  s[i++] = '0'+n;
  return i;
}

