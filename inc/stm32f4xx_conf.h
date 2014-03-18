//Apparently this file gets included by all the std peripheral drivers, so  we've gotta have it

#ifndef __STM32F4xx_CONF_H
#define __STM32F4xx_CONF_H

#if defined  (HSE_VALUE)
/* Redefine the HSE value; it's equal to 8 MHz on the STM32F4-DISCOVERY Kit */
 #undef HSE_VALUE
 #define HSE_VALUE    ((uint32_t)8000000) 
#endif /* HSE_VALUE */

/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT

/**
  * @brief  The assert_param macro is used for function's parameters check.
  * @param  expr: If expr is false, it calls assert_failed function
  *   which reports the name of the source file and the source
  *   line number of the call that failed. 
  *   If expr is true, it returns no value.
  * @retval None
  */
  #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions ------------------------------------------------------- */
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0)
#endif /* USE_FULL_ASSERT */

#endif
