#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_gpio.h"

#include "display.h"

GPIO_InitTypeDef gpioe_config;

uint8_t character_to_write; 

void display_init() {
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM1, ENABLE);
  gpioe_config.GPIO_Pin = 0xFF80; //pins 7-15 rs db0-7 (r/w always 0)
  gpioe_config.GPIO_Mode = GPIO_Mode_OUT;
  gpioe_config.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpioe_config.GPIO_Speed = GPIO_Speed_2MHz;
  
  
  static TIM_TimeBaseInitTypeDef tim1;
  /* Time base configuration */
  tim1.TIM_Period = 1000; // slow idk
  tim1.TIM_Prescaler = 0;
  tim1.TIM_ClockDivision = 0;
  tim1.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM1, &timebase);
  //set TIM6 to generate Update events
  TIM_SelectOutputTrigger(TIM1, TIM_TRGOSource_Update);
  /* TIM6 enable counter */
  TIM_Cmd(TIM1, ENABLE);
}