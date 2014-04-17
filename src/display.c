#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "misc.h"

#include "display.h"

#define DISPLAY_PERIOD 1000

GPIO_InitTypeDef gpioe_config;
GPIO_InitTypeDef gpiob_config;

char character_to_write; 
int display_cycle;

enum {
  INIT1,
  INIT2,
  INIT3,
  IDLE,
  WRITING_ADDRESS,
  WRITING_CHARACTER
} display_state;

void TIM2_IRQHandler() {
  TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
  switch (display_cycle) {
    case 0:
      switch (display_state) {
        case WRITING_CHARACTER:
          //set RS and bits
          GPIO_Write(GPIOE, 0x80 + ((uint8_t)character_to_write << 8)); //unsigned so we don't sign-extend
          character_to_write = 0;
          break;
        case INIT2:
          GPIO_Write(GPIOE, 0x0C << 8);
          break;
        case INIT3:
          GPIO_Write(GPIOE, 0x06 << 8);
          break;
        default:
          break;
      }
      if (display_state != IDLE) {
        display_cycle++;
      }
      break;
    case 1:
      GPIO_Write(GPIOB, 1); //set E
      display_cycle++;
      break;
    case 2:
      GPIO_Write(GPIOB, 0); //clear E
      display_cycle = 0;
      switch (display_state) {
        case INIT1:
        case INIT2:
        case INIT3:
          display_state++; //go to next init state or idle
          break;
        case WRITING_ADDRESS:
          display_state = WRITING_CHARACTER;
          break;
        case WRITING_CHARACTER:
          display_state = IDLE;
          break;
        default:
          break;
      }
      break;
  }
}

void display_set(char c, uint8_t x, uint8_t y) {
  if (display_ready()) {
    character_to_write = c;
    GPIO_Write(GPIOE, 0x8000 | ((x + 0x40U * y) << 8)); //send address
    display_state = WRITING_ADDRESS;
  }
}

void display_init() {
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
  gpioe_config.GPIO_Pin = 0xFF80; //pins 7-15 rs db0-7 (r/w always 0)
  gpioe_config.GPIO_Mode = GPIO_Mode_OUT;
  gpioe_config.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpioe_config.GPIO_OType = GPIO_OType_OD;
  gpioe_config.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOE, &gpioe_config);
  
  gpiob_config.GPIO_Pin = 0x0001; //b1 = E  
  gpiob_config.GPIO_Mode = GPIO_Mode_OUT;
  gpiob_config.GPIO_PuPd = GPIO_PuPd_Up;
  gpiob_config.GPIO_OType = GPIO_OType_OD;
  gpiob_config.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &gpiob_config);
  
  TIM_TimeBaseInitTypeDef tim2;
  /* Time base configuration */
  tim2.TIM_Period = DISPLAY_PERIOD; // slow idk
  tim2.TIM_Prescaler = 0;
  tim2.TIM_ClockDivision = 0;
  tim2.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM2, &tim2);
  //set TIM2 to generate Update events
  TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);
  
  TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
  
  NVIC_InitTypeDef tim2_irq;
  tim2_irq.NVIC_IRQChannel = TIM2_IRQn;
  tim2_irq.NVIC_IRQChannelPreemptionPriority = 100; //FIXME: should have a file with irq priorities
  tim2_irq.NVIC_IRQChannelSubPriority = 1;
  tim2_irq.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&tim2_irq);
  
  /* TIM2 enable counter */
  TIM_Cmd(TIM2, ENABLE);
  //set 8-bit, two line mode, enable display
  GPIO_SetBits(GPIOE, 0x3F << 8);
  display_cycle = 1;
  display_state = INIT1;
}

bool display_ready() {
  if (display_state == IDLE && display_cycle == 0) {
    return true;
  }
  return false;
}
