#include <string.h>

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
uint8_t region_one[OUTPUT_BUFFER_LENGTH] USE_SECTION("SRAM1");
uint8_t region_two[OUTPUT_BUFFER_LENGTH] USE_SECTION("SRAM2");


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

bool editing;
void display_output_sample_rate(char ui[UI_MAX_LENGTH], const ui_entry* entry, uint32_t time) {
  if (editing || time & 0x10) {
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
    
bool change_output_sample_rate(const ui_entry* entry, ui_button button, uint32_t time) {
  if (button & UI_BUTTON_ENTER) {
    editing = !editing;
  }
  
  if (editing) {
    uint32_t* target = (uint32_t*)entry->user_data;
    if (button & UI_BUTTON_UP) {
      *target -= 1;
      if (*target < 1) {
        *target = 1;
      } else {
        if (!output_init(&output)) {
          *target += 1;
          output_init(&output);
        }
      }
      return true;
    }
    if (button & UI_BUTTON_DOWN) {
      *target += 1;
      if (!output_init(&output)) {
        *target -= 1;
        output_init(&output);
      }
      return true;
    }
  }
  return false;
}


size_t position;
//string_editor assumes user_data is maxlength

const ui_entry ui[] = {
  {.name = "Message:", .value = &message[0], .callback = ui_callback_none, .display = ui_display_name_only},
  {.name = "Framing:", .value = &header[0], .callback = ui_callback_none, .display = ui_display_name_only},
  {.name = "Output sample rate:", .value = &output.period, .callback = ui_callback_none, .display = ui_display_name_only},
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

  for (;;) {
    
    //TASKS
    
    //0. Check for new message 
    if (new_message == true) {
      message_used = strlen(message);
      message_position = 0;
      data_used = 0;
      symbols_used = 0;
      envelope_samples_used = 0;
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
      }
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
        //TODO:
        symbols_used = 0;
      }
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
    
    //4. Update the UI
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

