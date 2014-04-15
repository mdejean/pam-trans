#include <stdbool.h>
#include <string.h>
#include <stdio.h> //snprintf

#include "stm32f4xx_gpio.h"

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
  NEW_ENTRY,
  UPDATED_VALUE,
};

char value_str_storage[UI_MAX_VALUE_LENGTH]; //string of current value
const char* value_str;
size_t value_pos; //position in current value string

size_t name_pos; //position in name string

GPIO_InitTypeDef gpiod_config;

#define UI_PIN_MASK 0xff00
#define UI_PIN_SHIFT 8

bool ui_init(const ui_entry* entries, size_t count) {
  for (size_t i=0;i<count;i++) {
    if (strlen(entries[i].name) > UI_MAX_NAME_LENGTH) {
      return false;
    }
  }
  
  ui_entries = entries;
  num_entries = count;
  
  //set up buttons
  //PD8-15: up down left right enter cancel
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
  gpiod_config.GPIO_Pin = UI_PIN_MASK; 
  gpiod_config.GPIO_Mode = GPIO_Mode_OUT;
  gpiod_config.GPIO_PuPd = GPIO_PuPd_UP;
  gpiod_config.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(GPIOB, &gpiob_config);
  
  //set up TIM3 to count at ~100Hz for debouncing
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
  static TIM_TimeBaseInitTypeDef tim3;
  /* Time base configuration */
  tim3.TIM_Period = INPUT_PERIOD; // slow idk
  tim3.TIM_Prescaler = 0;
  tim3.TIM_ClockDivision = 0;
  tim3.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM2, &timebase);
  
  display_init();
}

void update_value() {
  if (ui_entries[current_entry].type == UI_ENTRY_UINT32) {
    snprintf(value_str_storage, sizeof(value_str_storage), "%u", ui_entries[current_entry].value);
    value_str = value_str_storage;
  } else if (ui_entries[current_entry].type == UI_ENTRY_FLOAT) {
    snprintf(value_str_storage, sizeof(value_str_storage), "%f", ui_entries[current_entry].value);
    value_str = value_str_storage;
  } else if (ui_entries[current_entry].type == UI_ENTRY_STRING) {
    value_str = (const char*)ui_entries[current_entry].value;
  }
  
  value_pos = 0;
  state = UPDATED_VALUE;
}

void ui_update() {
  if (button & UI_BUTTON_UP) {
    current_entry = (current_entry + 1);
    if (current_entry >= num_entries) current_entry = 0;
    state = NEW_ENTRY;
  }
  if (button & UI_BUTTON_DOWN) {
    if (current_entry == 0) current_entry = num_entries;
    current_entry = (current_entry - 1);
    state = NEW_ENTRY;
  }
}

void ui_tick() {
  static enum ui_state state;
  if (state == IDLE) {
    if (TIM_GetFlagStatus(TIM3, TIM_FLAG_Update) == SET) {
      TIM_ClearFlag(TIM3, TIM_FLAG_Update);
      static uint16_t prev_input;
      uint16_t new_input = (GPIO_ReadInputData(GPIOD) & UI_PIN_MASK) >> UI_PIN_SHIFT;
      uint16_t input_change = prev_input & ~new_input; //buttons pressed - new_input will be low (active low), prev_input high
      prev_input = new_input;
      //call the callback
      if (ui_entries[current_entry].callback(ui_entries[current_entry], input_change)) {
        //assume we need to update value
        update_value();
      } else {
        //button press not consumed - do default behaviors
        ui_update(input_change);
      }
    }
  }
  
  if (state == UPDATED_VALUE) {
    if (display_ready()) {
      if (value_str[value_pos] && value_pos < UI_MAX_VALUE_LENGTH) {
        display_set(value_str[value_pos], 20+value_pos, 0);
        value_pos++;
      }
    }
  }
  
  if (state == NEW_ENTRY) {
    //print the name
    if (display_ready()) {
      if (ui_entries[current_entry].name[name_pos]) {
        display_set(ui_entries[current_entry].name[name_pos],name_pos, 0);
        name_pos++;
      } else {
        //done drawing the name, now show the value
        update_value();
      }
    }
  }
  
  //todo: update status - error paused time?
}

void ui_callback_none(const ui_entry* entry, ui_button button) {
}