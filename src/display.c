#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_gpio.h"

#include "display.h"

#define DISPLAY_PERIOD 1000

GPIO_InitTypeDef gpioe_config;
GPIO_InitTypeDef gpiob_config;

char character_to_write; 
int state;

void TIM2_IRQHandler() {
  TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
  switch (state) {
    case 0:
      //set RS and bits if we have something to do
      if (character_to_write > 0) {
        GPIO_SetBits(GPIOE, 0x80 + ((uint8_t)character_to_write) >> 8); //unsigned so we don't sign-extend
        character_to_write = 0;
        state++;
      }
      break;
    case 1:
      GPIO_SetBits(GPIOB, 1); //set E
      state++;
      break;
    case 2:
      GPIO_SetBits(GPIOB, 0); //clear E
      state = 0;
      if (character_to_write == 0) {
        //we're done
        TIM_SetAutoreload(TIM2, 0);
      break;
  }
}

void display_set(char c, uint8_t x, uint8_t y) {
  if (character_to_write == 0 && state == 0) {
    GPIO_SetBits(GPIOE, (uint8_t)(x + 0x40 * y) >> 8); //send address
    character_to_write = c;
    state = 1;
    TIM_SetAutoreload(TIM2, DISPLAY_PERIOD);
  }
}

void display_init() {
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
  gpioe_config.GPIO_Pin = 0xFF80; //pins 7-15 rs db0-7 (r/w always 0)
  gpioe_config.GPIO_Mode = GPIO_Mode_OUT;
  gpioe_config.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpioe_config.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOE, &gpioe_config);
  
  gpiob_config.GPIO_Pin = 0x0001; //b1 = E  
  gpiob_config.GPIO_Mode = GPIO_Mode_OUT;
  gpiob_config.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpiob_config.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &gpiob_config);
  
  static TIM_TimeBaseInitTypeDef tim1;
  /* Time base configuration */
  tim1.TIM_Period = DISPLAY_PERIOD; // slow idk
  tim1.TIM_Prescaler = 0;
  tim1.TIM_ClockDivision = 0;
  tim1.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM2, &timebase);
  //set TIM6 to generate Update events
  TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);
  
  TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
  /* TIM6 enable counter */
  TIM_Cmd(TIM2, ENABLE);
  GPIO_SetBits(GPIOE, 0x3C);
}

bool display_ready() {
  if (character_to_write == 0 && state == 0) {
    return true;
  }
  return false;
}