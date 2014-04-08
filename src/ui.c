#include <stdbool.h>

#include "stm32f4xx_gpio.h"

const ui_entry* ui_entries;
size_t num_entries;

bool ui_init(ui_entry* entries, size_t count) {
  ui_entries = entries;
  num_entries = count;
}

void ui_tick() {
  
}