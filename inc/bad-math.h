#ifndef BAD_MATH_H
#define BAD_MATH_H
//TODO: This header should not exist. Make libmath work!
#define M_PI 3.1415926f

#ifndef BAD_MATH_INLINE
#define BAD_MATH_INLINE inline
#endif

BAD_MATH_INLINE float f_sin(float r) {
  if (r > M_PI || r < -M_PI) {
    int a = (r/M_PI);
    r -= M_PI * a;
  }
  if (r > M_PI/2.0f) {
    r = M_PI - r;
  }
  if (r < -M_PI/2.0f) {
    r = -M_PI - r;
  }
    
  return r*(1.0f-r*r*(1.0f-r*r/20.0f)/6.0f); //shitty approximation
}

BAD_MATH_INLINE float f_cos(float r) {
  return f_sin(M_PI/2.0f + r);
}

BAD_MATH_INLINE float f_sqrt(float a) {
  float result;
  asm("vsqrt.f32 %0, %1" :"=w" (result) : "w" (a));
  return result;
}


#endif
