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

#include "defaults.h"

#define OUTPUT_BUFFER_LENGTH 2048 //2KB

#define SRRC_OVERLAP 4

#define MAX_MESSAGE_LENGTH 1000
#define BITS_PER_SYMBOL 2
char message[MAX_MESSAGE_LENGTH] = 
"It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness, "
"it was the epoch of belief, it was the epoch of incredulity, it was the season of Light, it was the season of "
"Darkness, it was the spring of hope, it was the winter of despair, we had everything before us, we had "
"nothing before us, we were all going direct to Heaven, we were all going direct the other way-in short, "
"the period was so far like the present period, that some of its noisiest authorities insisted on its being "
"received, for good or for evil, in the superlative degree of comparison only.";

#define MIN_FRAME_LENGTH 40
#define MAX_HEADER_LENGTH 100

#define MAX_DATA_LENGTH 2000
uint8_t data[MAX_DATA_LENGTH]; //2K

char header[MAX_HEADER_LENGTH] = "IntroDigitalCommunications:eecs354:mxb11:profbuchner";

#define MAX_SYMBOLS_LENGTH (MAX_DATA_LENGTH * 8/BITS_PER_SYMBOL)

//overlap extra samples at the end get filled with the beginning for repeat transmission
sample_t symbols[MAX_SYMBOLS_LENGTH + SRRC_OVERLAP]; //(2K * 4) * 4 = 32K

sample_t envelope[OUTPUT_BUFFER_LENGTH]; //16K
size_t envelope_samples_used;

//set up buffers in two memory regions so DMA can take them
// without stalling the processor
//this has a side effect of making us unable to use most of SRAM1 (most of the processor's memory)
#define USE_SECTION(a) __attribute__ ((section ((a))))
uint8_t region_one[OUTPUT_BUFFER_LENGTH] USE_SECTION("SRAM1");
uint8_t region_two[OUTPUT_BUFFER_LENGTH] USE_SECTION("SRAM2");


encode_state encoder;
convolve_state convolver;
upconvert_state upconverter;
output_state output = {
  .sample_rate = DEFAULT_SAMPLE_RATE,
  .region_one = region_one,
  .region_two = region_two,
  .output_buffer_length = OUTPUT_BUFFER_LENGTH
};

const ui_entry ui[] = {
  {.type = UI_ENTRY_STRING, .name = "Message:", .value = &message[0], .callback = ui_callback_none},
  {.type = UI_ENTRY_STRING, .name = "Framing:", .value = &header[0], .callback = ui_callback_none},
  {.type = UI_ENTRY_UINT32, .name = "Output sample rate:", .value = &output.sample_rate, .callback = ui_callback_none},
};


int main(void) {

  size_t message_length = 0;
  size_t symbols_length = 0;

  size_t symbols_position = 0;

  size_t symbols_per_buffer = 0;
  bool new_message = true;

  encode_init(&encoder, 100, (const uint8_t*)header, strlen(header), NULL, 0);
  convolve_init_srrc(&convolver, 0.2, 4, 100); //baudrate = 10kHz
  upconvert_init(&upconverter, 1, 10); //carrier = 100kHz if Fs = 1MHz
  
  ui_init(ui, sizeof(ui)/sizeof(ui[0]));
  
  output_init(&output);

  for (;;) {
    
    //TASKS
    
    //1. Encode the message
    if (new_message == true) {
      message_length = strlen(message);
      size_t data_used =
        frame_message(&encoder,
                      (const uint8_t*)message,
                      message_length,
                      data,
                      MAX_DATA_LENGTH);
      //TODO: transmit partial message if message is too large to be framed (change in frame_message)
      if (!data_used) {
        //set_fault_light();
      }
      symbols_length = encode_data(&encoder, data, data_used, symbols, MAX_SYMBOLS_LENGTH);
      memcpy(&symbols[symbols_length], &symbols[0], SRRC_OVERLAP * sizeof(sample_t));
      symbols_length += SRRC_OVERLAP;
      symbols_position = 0;

      symbols_per_buffer = OUTPUT_BUFFER_LENGTH/convolver.M;
      
      new_message = false;
    }
    
    //2. Make the envelope
    if (envelope_samples_used == 0) {
      memset(envelope, 0, OUTPUT_BUFFER_LENGTH * sizeof(envelope[0]));
      size_t symbols_consumed =
        convolve(&convolver,
                 &symbols[symbols_position],
                 (symbols_length - symbols_position < symbols_per_buffer)
                    ? symbols_length - symbols_position : symbols_per_buffer,
                 envelope,
                 OUTPUT_BUFFER_LENGTH);
      symbols_position += symbols_consumed;
      if (symbols_length - symbols_position <= SRRC_OVERLAP) {
        symbols_position = 0;
      }
      envelope_samples_used = convolver.M * symbols_consumed;
    }
    
    //3. Upconvert the envelope
    //Note that there will always be an envelope waiting for us at this point because we clear envelope_samples_used and the previous block runs first
    if (output_get_buffer(&output)) {
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

