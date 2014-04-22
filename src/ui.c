#include <stdbool.h>

#include "stm32f4xx_gpio.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"

#include "ui.h"
#include "display.h"
//input period in timer clock cycles
//keypresses shorter than this will be missed
// 10ms
#define INPUT_PERIOD 210000 

const ui_entry* ui_entries;
size_t num_entries;

size_t current_entry;

enum ui_state {
  INIT,
  IDLE,
  UPDATED,
} ui_state;

char display[UI_MAX_LENGTH]; //string of current value
size_t display_pos; //position in current value string

uint32_t time;

#define UI_PIN_MASK 0xff00
#define UI_PIN_SHIFT 8
GPIO_InitTypeDef gpiod_config;

#define STATUS_PIN_MASK 0x0F
GPIO_InitTypeDef gpioc_config;

bool ui_init(const ui_entry* entries, size_t count) {
  
  ui_entries = entries;
  num_entries = count;
  
  //set up buttons
  //PD8-15: up down left right enter cancel
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
  gpiod_config.GPIO_Pin = UI_PIN_MASK; 
  gpiod_config.GPIO_Mode = GPIO_Mode_IN;
  gpiod_config.GPIO_PuPd = GPIO_PuPd_UP;
  gpiod_config.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOD, &gpiod_config);
  
  //set up outputs
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
  gpioc_config.GPIO_Pin = 0x0f; 
  gpioc_config.GPIO_Mode = GPIO_Mode_OUT;
  gpioc_config.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpioc_config.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_Init(GPIOC, &gpioc_config);
  
  //set up TIM3 to count at ~100Hz for debouncing
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
  static TIM_TimeBaseInitTypeDef tim3;
  /* Time base configuration */
  tim3.TIM_Period = INPUT_PERIOD; // slow idk
  tim3.TIM_Prescaler = 0;
  tim3.TIM_ClockDivision = 0;
  tim3.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM3, &tim3);
  TIM_Cmd(TIM3, ENABLE);
  
  display_init();
  
  ui_state = INIT;
  
  return true;
}

void ui_refresh() {
  ui_entries[current_entry].display(display, &ui_entries[current_entry], time);
  ui_state = UPDATED;
}

bool ui_update(uint16_t button) {
  if (button & UI_BUTTON_UP) {
    current_entry = (current_entry + 1);
    if (current_entry >= num_entries) current_entry = 0;
    return true;
  }
  if (button & UI_BUTTON_DOWN) {
    if (current_entry == 0) current_entry = num_entries;
    current_entry = (current_entry - 1);
    return true;
  }
  return false;
}

void ui_tick() {
  if (ui_state == IDLE) {
    if (TIM_GetFlagStatus(TIM3, TIM_FLAG_Update) == SET) {
      TIM_ClearFlag(TIM3, TIM_FLAG_Update);
      static uint16_t prev_input;
      uint16_t new_input = (GPIO_ReadInputData(GPIOD) & UI_PIN_MASK) >> UI_PIN_SHIFT;
      uint16_t input_change = prev_input & ~new_input; //buttons pressed - new_input will be low (active low), prev_input high
      prev_input = new_input;
      //soapbox: this could be written as a single if statement 
      //with short-circuit evaluation, but that would be dumb
      if (input_change) {
        if (ui_entries[current_entry].callback(&ui_entries[current_entry], input_change, time)) {
          //something happened. update the display
          ui_refresh();
        } else {
          //button press not consumed - do default behaviors
          if (ui_update(input_change)) {
            ui_refresh();
          }
        }
      }
      time++;
    }
  }
  
  if (ui_state == UPDATED) {
    if (display_ready()) {
      if (display_pos < UI_MAX_LENGTH) {
        //is this the right place to be replacing nulls with spaces?
        display_set(display[display_pos] ? display[display_pos]  : ' ', display_pos, 0);
        display_pos++;
      } else {
        display_pos = 0;
        ui_state = IDLE;
      }
    }
  }
  
  if (ui_state == INIT) {
    ui_refresh();
  }
}

void ui_set_status(uint8_t o) {
  GPIO_Write(GPIOC, o & STATUS_PIN_MASK);  
}

uint8_t ui_get_status() {
  return GPIO_ReadOutputData(GPIOC) & STATUS_PIN_MASK;
}

bool ui_callback_none(const ui_entry* entry, ui_button button, uint32_t time) {
  return false;
}

void ui_display_name_only(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  int j=0;
  for (int i=0; i<UI_MAX_LENGTH; i++) {
    ui[i] = entry->name[j];
    if (entry->name[j]) {
      j++;
    }
  }
}
