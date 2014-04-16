#include <stdbool.h>
#include <string.h>
#include <math.h>


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
  NEW_ENTRY,
  UPDATED_VALUE,
} ui_state;

char value_str_storage[UI_MAX_VALUE_LENGTH]; //string of current value
const char* value_str;
size_t value_pos; //position in current value string

size_t name_pos; //position in name string

#define UI_PIN_MASK 0xff00
#define UI_PIN_SHIFT 8
GPIO_InitTypeDef gpiod_config;

#define STATUS_PIN_MASK 0x0F
GPIO_InitTypeDef gpioc_config;

#define SIGNIFICANT_FIGURES 4
size_t float_to_string(char* s, size_t len, float f) {
  if (len < SIGNIFICANT_FIGURES + 7) return 0;//7 = +.e+99\0
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
    if (i == 3) s[--i] = '.'; //3 = +x.3 0123
    n /= 10;
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
  s[SIGNIFICANT_FIGURES+6] = '\0';
  return i;
}

size_t uint32_t_to_string(char* s, size_t len, uint32_t n) {
  if (len < 11) return 0;
  //i apologize sincerely for the following
  int i=0;
  #define J(K) if (n>K) { s[i++] = '0'+(n / K); n -= n / K; }
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
  
  display_init();
  return true;
}

void update_value() {
  if (ui_entries[current_entry].type == UI_ENTRY_UINT32) {
    uint32_t_to_string(value_str_storage, 
      sizeof(value_str_storage), 
      *(uint32_t*)ui_entries[current_entry].value);
    value_str = value_str_storage;
  } else if (ui_entries[current_entry].type == UI_ENTRY_FLOAT) {
    float_to_string(value_str_storage, 
      sizeof(value_str_storage), 
      *(float*)ui_entries[current_entry].value);
    value_str = value_str_storage;
  } else if (ui_entries[current_entry].type == UI_ENTRY_STRING) {
    value_str = (const char*)ui_entries[current_entry].value;
  }
  
  value_pos = 0;
  ui_state = UPDATED_VALUE;
}

void ui_update(uint16_t button) {
  if (button & UI_BUTTON_UP) {
    current_entry = (current_entry + 1);
    if (current_entry >= num_entries) current_entry = 0;
    ui_state = NEW_ENTRY;
  }
  if (button & UI_BUTTON_DOWN) {
    if (current_entry == 0) current_entry = num_entries;
    current_entry = (current_entry - 1);
    ui_state = NEW_ENTRY;
  }
}

void ui_tick() {
  if (ui_state == IDLE) {
    if (TIM_GetFlagStatus(TIM3, TIM_FLAG_Update) == SET) {
      TIM_ClearFlag(TIM3, TIM_FLAG_Update);
      static uint16_t prev_input;
      uint16_t new_input = (GPIO_ReadInputData(GPIOD) & UI_PIN_MASK) >> UI_PIN_SHIFT;
      uint16_t input_change = prev_input & ~new_input; //buttons pressed - new_input will be low (active low), prev_input high
      prev_input = new_input;
      //call the callback
      if (ui_entries[current_entry].callback(&ui_entries[current_entry], input_change)) {
        //assume we need to update value
        update_value();
      } else {
        //button press not consumed - do default behaviors
        ui_update(input_change);
      }
    }
  }
  
  if (ui_state == UPDATED_VALUE) {
    if (display_ready()) {
      if (value_str[value_pos] && value_pos < UI_MAX_VALUE_LENGTH) {
        display_set(value_str[value_pos], 20+value_pos, 0);
        value_pos++;
      }
    }
  }
  
  if (ui_state == NEW_ENTRY) {
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

void ui_set_status(uint8_t o) {
  GPIO_Write(GPIOC, o & STATUS_PIN_MASK);  
}

uint8_t ui_get_status() {
  return GPIO_ReadOutputData(GPIOC) & STATUS_PIN_MASK;
}

bool ui_callback_none(const ui_entry* entry, ui_button button) {
  return false;
}
