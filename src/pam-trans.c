#include <string.h>
#include <math.h>

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_dac.h"

#include "upconvert.h"
#include "convolve.h"
#include "encode.h"
#include "sample.h"

#include "output.h"
#include "ui.h"

#include "utility.h"

#include "defaults.h"

#define OUTPUT_BUFFER_LENGTH 2048 //2KB

#define MAX_MESSAGE_LENGTH 1000
#define BITS_PER_SYMBOL 2
char message[MAX_MESSAGE_LENGTH] = DEFAULT_MESSAGE;

#define MIN_FRAME_LENGTH 40
#define MAX_HEADER_LENGTH 100

#define MAX_DATA_LENGTH 2000
uint8_t data[MAX_DATA_LENGTH]; //2K

char header[MAX_HEADER_LENGTH] = DEFAULT_HEADER;

#define MAX_SYMBOLS_LENGTH (MAX_DATA_LENGTH * 8/BITS_PER_SYMBOL)

//overlap extra samples at the end get filled with the beginning for repeat transmission
sample_t symbols[MAX_SYMBOLS_LENGTH + MAX_PULSE_SYMBOLS]; //(2K * 4) * 4 = 32K

sample_t envelope[OUTPUT_BUFFER_LENGTH]; //16K
size_t envelope_samples_used;

//set up buffers in two memory regions so DMA can take them
// without stalling the processor
//this has a side effect of making us unable to use most of SRAM1 (most of the processor's memory)
#define USE_SECTION(a) __attribute__ ((section ((a))))
uint16_t region_one[OUTPUT_BUFFER_LENGTH] USE_SECTION("SRAM1");
uint16_t region_two[OUTPUT_BUFFER_LENGTH] USE_SECTION("SRAM2");


bool new_message = true;

encode_state encoder = {
  .frame_len = DEFAULT_FRAME_LENGTH,
  .start_framing = (uint8_t*)&header,
  .start_framing_len = 40,
  .end_framing = NULL,
  .end_framing_len = 0
};

convolve_state convolver = {
    .beta = DEFAULT_BETA, 
    .overlap = DEFAULT_OVERLAP_SYMBOLS,
    .M = DEFAULT_CONVOLVE_OVERSAMPLING
};

upconvert_state upconverter = {
  .M = DEFAULT_UPCONVERT_OVERSAMPLING,
  .N = DEFAULT_UPCONVERT_PERIOD
};

output_state output = {
  .period = DEFAULT_OUTPUT_PERIOD,
  .region_one = region_one,
  .region_two = region_two,
  .output_buffer_length = OUTPUT_BUFFER_LENGTH
};

typedef bool(*on_update_cb)(void);

bool editing;
size_t position;
    
bool change_uint32(const ui_entry* entry, ui_button button, uint32_t time) {
  bool ret = false;
  if (button & UI_BUTTON_ENTER) {
    editing = !editing;
    ret = true;
  }
  
  if (editing) {
    uint32_t* target = (uint32_t*)entry->value;
    on_update_cb onupdate = (on_update_cb)entry->user_data;  
    if (button & UI_BUTTON_UP) {
      *target += 1;
      if (*target < 1) {
        *target = 1;
      } else {
        if (!onupdate()) {
          *target -= 1;
          onupdate();
        }
      }
      ret = true;
    }
    if (button & UI_BUTTON_DOWN) {
      *target -= 1;
      if (!onupdate()) {
        *target += 1;
        onupdate();
      }
      ret = true;
    }
  }
  return ret;
}

bool change_uint32_backwards(const ui_entry* entry, ui_button button, uint32_t time) {
  ui_button a = button;
  if (button & UI_BUTTON_UP) {
    a ^= UI_BUTTON_UP;
    a |= UI_BUTTON_DOWN;
  } 
  if (button & UI_BUTTON_DOWN) {
    a ^= UI_BUTTON_DOWN;
    a |= UI_BUTTON_UP;
  }
  return change_uint32(entry, a, time);
}

void display_uint32(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing) {
    size_t i = uint32_to_string(ui, UI_MAX_LENGTH, *(uint32_t*)entry->value);
    for (;i<UI_MAX_LENGTH;i++) ui[i] = ' ';
  } else {
    ui_display_name_only(ui, entry, time);
  }
}

void display_float(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing) {
    size_t i = float_to_string(ui, UI_MAX_LENGTH, *(float*)entry->value);
    for (;i<UI_MAX_LENGTH;i++) ui[i] = ' ';
    if (time & 0x80) ui[position] = ' '; //blink
  } else {
    ui_display_name_only(ui, entry, time);
  }
}
bool change_float(const ui_entry* entry, ui_button button, uint32_t time) {
  bool ret = false;
  float* target = (float*)entry->value;
  if (button & UI_BUTTON_ENTER) {
    editing = !editing;
    position = 0;
    ret = true;
  }
  if (editing) {
    if (button & UI_BUTTON_UP || button & UI_BUTTON_DOWN) {
      if (position == 0) { //sign
        *target = -*target;
      } else if (position == SIGNIFICANT_FIGURES+1) {
        *target /= powf(10,2*floorf(log10f(*target)));
      } else if (position > SIGNIFICANT_FIGURES+1) {
        if (button & UI_BUTTON_DOWN) {
          *target /= 10;
        } else {
          *target *= 10;
        }
      } else if (position == 2) {
        
      } else {
        float factor = powf(10,2*floorf(log10f(*target)));
        if (button & UI_BUTTON_DOWN) {
          factor = -factor;
        }
        if (position > 2) {
          factor *= powf(10, -(position-3));
        }
        *target += factor; 
      }
        
      ret = true;
    }
    if (button & UI_BUTTON_LEFT) {
      if (position > 0) {
        position--;
      }
      ret = true;
    }
    if (button & UI_BUTTON_RIGHT) {
      if (position < SIGNIFICANT_FIGURES+5) {
        position++;
      }
      ret = true;
    }
    if (time & 0x40) {
      ret = true;
    }
  }
  return ret;
}

bool default_message_callback(const ui_entry* entry, ui_button button, uint32_t time) {
  const char* new = (const char*)entry->value;
  if (button & UI_BUTTON_ENTER) {
    strncpy(message, new, MAX_MESSAGE_LENGTH);
    editing = 1;
    new_message = true;
    return true;
  }
  if (button & UI_BUTTON_UP || button & UI_BUTTON_DOWN) {
    editing = 0;
    return false;
  }
  return false;
}

void default_message_display(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing == 1) {
    strncpy(ui, "Message changed!   ", UI_MAX_LENGTH);
  } else {
    ui_display_name_only(ui, entry, time);
  }
}

typedef struct string_editor {
  size_t max_length;
  on_update_cb on_update;
} string_editor;

bool string_editor_callback(const ui_entry* entry, ui_button button, uint32_t time) {
  bool ret = false;
  string_editor* e = (string_editor*)entry->user_data;
  char* s = (char*)entry->value;
  if (button & UI_BUTTON_ENTER) {
    editing = !editing;
    position = 0;
    ret = true;
  }
  if (editing) {
    if (button & UI_BUTTON_UP) {
      if (s[position] == 0 && position < e->max_length) {
        s[position] = 'A';
        s[position+1] = '\0';
      } else {
        s[position]++;
      }
    }
    if (button & UI_BUTTON_DOWN) {
      s[position]--;
    }
    if (button & UI_BUTTON_LEFT) {
      if (position != 0) {
        position--;
      }
    }
    if (button & UI_BUTTON_RIGHT) {
      if (s[position] != '\0') {
        position++;
      }
    }
    if (time & 0x40 || button) {
      ret = true;
    }
  }
  
  return ret;
}

void string_editor_display(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing) {
    string_editor* e = (string_editor*)entry->user_data;
    char* s = (char*)entry->value;
    for (size_t i = 0; i < UI_MAX_LENGTH; i++) {
      if (position + i > e->max_length) {
        ui[i] = ' ';
      } else {
        ui[i] = s[position + i];
      }
    }
    if (time & 0x80) ui[0] = ' '; //blink
  } else {
    ui_display_name_only(ui, entry, time);
  }
}

bool on_update_message() {
  new_message = true;
  return true;
}

bool on_update_framing() {
  encoder.start_framing_len = strlen(header);
  return encode_init(&encoder);
}

bool on_update_output() {
  return output_init(&output);
}

bool on_update_upconvert() {
  return upconvert_init(&upconverter);
}

bool on_update_convolve() {
  return convolve_init_srrc(&convolver);
}

void display_output_sample_rate(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing /*|| time & 0x10*/) { //todo: time-based update
    //TODO: figure out why this factor is 8 and not 2
    uint32_t sample_rate = SystemCoreClock / 8 / output.period;
    uint32_t kHz = sample_rate / 1000;
    uint32_t hundreds = (sample_rate % 1000) / 100;
    size_t i = uint32_to_string(ui, UI_MAX_LENGTH, kHz);
    ui[i++] = '.'; ui[i++] = '0' + hundreds;
    ui[i++] = ' '; ui[i++] = 'k'; ui[i++] = 'H'; ui[i++] = 'z';
    for (;i<UI_MAX_LENGTH;i++) ui[i] = ' ';
  } else {
    ui_display_name_only(ui, entry, time);
  }
}

void display_symbol_rate(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing) {
    uint32_t symbol_rate = SystemCoreClock / 8 / output.period / *(uint32_t*)entry->value;
    size_t i = uint32_to_string(ui, UI_MAX_LENGTH, symbol_rate / 1000);
    size_t j = uint32_to_string(&ui[i], UI_MAX_LENGTH - i, 1000 + symbol_rate % 1000);
    ui[i] = '.'; //fixme: this is janky
    i += j;
    strcpy(&ui[i], " kBaud"); //fixme: buffer overflow
    i += 6;
    for (;i<UI_MAX_LENGTH;i++) ui[i] = ' ';
  } else {
    ui_display_name_only(ui, entry, time);
  }
}

void display_carrier_freq(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing) { //fixme: merge this and above into some sort of display_with_units
    uint32_t carrier_freq = SystemCoreClock / 8 / output.period / *(uint32_t*)entry->value;
    size_t i = uint32_to_string(ui, UI_MAX_LENGTH, carrier_freq / 1000);
    size_t j = uint32_to_string(&ui[i], UI_MAX_LENGTH - i, 1000 + carrier_freq % 1000);
    ui[i] = '.'; //overwrite the "1" that we added 1000 for
    i += j;
    strcpy(&ui[i], " kHz"); //fixme: buffer overflow
    i += 4;
    for (;i<UI_MAX_LENGTH;i++) ui[i] = ' ';
  } else {
    ui_display_name_only(ui, entry, time);
  }
}


bool periodic_update(const ui_entry* entry, ui_button button, uint32_t time) {
  static uint32_t next;
  if (time > next) {
    next = time + 60; //fixme
    return true;
  }
  return false;
}

uint32_t transmit_count;

void display_transmit_count(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  uint32_t n = *(uint32_t*) entry->value;
  uint32_t i = 0;
  for (;i<UI_MAX_LENGTH && entry->name[i]; i++) ui[i] = entry->name[i];
  i += uint32_to_string(&ui[i], UI_MAX_LENGTH - i, n);
  for (;i<UI_MAX_LENGTH;i++) ui[i] = ' ';
}

uint32_t stalls;

void display_stalls(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  size_t i = uint32_to_string(ui, UI_MAX_LENGTH, stalls);
  strncpy(&ui[i], " stalls", UI_MAX_LENGTH - i);
}

bool reset_stalls(const ui_entry* entry, ui_button button, uint32_t time) {
  if (button & UI_BUTTON_ENTER) {
    stalls = 0;
    return true;
  }
  
  if (!button) return true; //always update
  return false;
}

const string_editor message_editor = {
  .max_length = MAX_MESSAGE_LENGTH,
  .on_update = on_update_message
};
const string_editor framing_editor = {
  .max_length = MAX_HEADER_LENGTH,
  .on_update = on_update_framing
};

const ui_entry ui[] = {
  {.name = "PAM Transmitter", 
   .callback = ui_callback_none, 
   .display = ui_display_name_only},
  {.name = "Message", 
   .value = &message[0], 
   .callback = string_editor_callback, 
   .display = string_editor_display,
   .user_data = (void*)&message_editor},
  {.name = "Framing", 
   .value = &header[0], 
   .callback = string_editor_callback, 
   .display = string_editor_display,
   .user_data = (void*)&framing_editor},
  {.name = "Frame length", 
   .value = &encoder.frame_len, 
   .callback = change_uint32, 
   .display = display_uint32,
   .user_data = on_update_convolve},
  {.name = "Output sample rate:", 
   .value = &output.period, 
   .callback = change_uint32_backwards, 
   .display = display_output_sample_rate,
   .user_data = on_update_output},
  {.name = "Pulse overlap (syms)", 
   .value = &convolver.overlap, 
   .callback = change_uint32, 
   .display = display_uint32,
   .user_data = on_update_convolve},
  {.name = "Baud (symbol) rate", 
   .value = &convolver.M, 
   .callback = change_uint32_backwards, 
   .display = display_symbol_rate,
   .user_data = on_update_convolve},
  {.name = "Beta (pulse shape)", 
   .value = &convolver.beta, 
   .callback = change_float, 
   .display = display_float,
   .user_data = on_update_convolve},
  {.name = "IF Carrier (f0)", 
   .value = &upconverter.N, 
   .callback = change_uint32_backwards, 
   .display = display_carrier_freq,
   .user_data = on_update_upconvert},
  {.name = "Default message A", 
   .value = DEFAULT_MESSAGE, 
   .callback = default_message_callback, 
   .display = default_message_display},
  {.name = "Default message B", 
   .value = DEFAULT_MESSAGE_B, 
   .callback = default_message_callback, 
   .display = default_message_display},
  {.name = "Default message C", 
   .value = DEFAULT_MESSAGE_C, 
   .callback = default_message_callback, 
   .display = default_message_display},
  {.name = "Transmit count ", 
   .value = &transmit_count, 
   .callback = periodic_update, 
   .display = display_transmit_count},
  {.value = &stalls, 
   .callback = reset_stalls, 
   .display = display_stalls},
};


int main(void) {

  size_t message_used = 0;
  size_t message_position = 0;
  
  size_t data_used = 0;
  size_t data_position = 0;
  
  size_t symbols_used = 0;
  size_t symbols_position = 0;

  size_t envelope_samples_used = 0;

  encode_init(&encoder);
  convolve_init_srrc(&convolver); //baudrate = 10kHz
  upconvert_init(&upconverter); //carrier = 100kHz if Fs = 1MHz
  
  ui_init(ui, sizeof(ui)/sizeof(ui[0]));
  
  output_init(&output);
  
  new_message = true;

  for (;;) {
    
    //TASKS
    
    //0. Check for new message 
    if (new_message == true) {
      message_used = strlen(message);
      message_position = 0;
      data_used = 0;
      data_position = 0;
      symbols_used = 0;
      symbols_position = 0;
      envelope_samples_used = 0;
      
      transmit_count = 0;
      
      new_message = false;
    }
    
    //1. Frame the message into data
    if (data_used == 0) {
      message_position +=
        frame_message(&encoder,
                      (const uint8_t*)&message[message_position],
                      message_used - message_position,
                      data,
                      MAX_DATA_LENGTH,
                      &data_used);
      
      if (message_position >= message_used) {
        message_position = 0;
        transmit_count++;
      }
      data_position = 0;
   }
   
   //2. Encode the data into symbols
   if (symbols_used == 0) { 
      //TODO: prepend the last overlap samples from the previous block
      data_position += 
        encode_data(&encoder, 
                    &data[data_position], 
                    data_used - data_position, 
                    symbols, 
                    MAX_SYMBOLS_LENGTH - convolver.overlap,
                    &symbols_used);
                    
      if (data_position >= data_used) {
        data_used = 0;
      }
      symbols_position = 0;
    }
    
    //3. Make the envelope
    if (envelope_samples_used == 0) {
      memset(envelope, 0, OUTPUT_BUFFER_LENGTH * sizeof(envelope[0]));
      symbols_position += 
        convolve(&convolver,
                 &symbols[symbols_position],
                 symbols_used - symbols_position,
                 envelope,
                 OUTPUT_BUFFER_LENGTH,
                 &envelope_samples_used);

      if (symbols_position + convolver.overlap >= symbols_used) {
        //TODO: prepend the last overlap samples from the previous block
        symbols_used = 0;
      }
      
      if (output_stalled()) {
        stalls += 1;
      }
      //upconvert/output run in one go, no position
    }
    
    //4. Upconvert the envelope and set it up for output
    if (envelope_samples_used != 0 && output_get_buffer(&output)) {
      //dma is doing its thing, do the next block
      size_t fill_length = upconvert(&upconverter,
                              envelope,
                              envelope_samples_used,
                              output_get_buffer(&output),
                              OUTPUT_BUFFER_LENGTH);
      output_set_filled(&output, fill_length);
      
      envelope_samples_used = 0;
    }
    
    //5. Update the UI
    ui_tick();
      
    if (output_stalled()) {
      ui_set_status(ui_get_status() | 0x1); //TODO: give names to pins
    } else {
      ui_set_status(ui_get_status() & ~0x1); //TODO: give names to pins
    }
    if (symbols_position == 0) {
      ui_set_status(ui_get_status() | 0x2);
    } else {
      ui_set_status(ui_get_status() & ~0x2); //TODO: give names to pins
    }
  }
}


/*The default interrupt handler crashes the system (for unexpected interrupts)
 * For some interrupts we don't want this behavior */
void NMI_Handler(void) {
}

void SVC_Handler(void) {
}

void DebugMon_Handler(void) {
}

void PendSV_Handler(void) {
}

void SysTick_Handler(void) {
}

void WWDG_IRQHandler(void) {
}

