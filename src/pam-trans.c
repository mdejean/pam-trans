#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_tim.h"

#include "upconvert.h"
#include "convolve.h"
#include "encode.h"
#include "sample.h"


#define OUTPUT_BUFFER_LENGTH 2048 //8KB

//set up buffers in three memory regions so DMA can take them
// without stalling the processor
//this has a side effect of making us unable to use
#define USE_SECTION(a) __attribute__ ((section ((a))))
sample_t region_one[OUTPUT_BUFFER_LENGTH] USE_SECTION("MEM_A");
sample_t region_two[OUTPUT_BUFFER_LENGTH] USE_SECTION("MEM_B");
//pointers to the three buffers by current use
sample_t* being_filled = region_one;
//how much of the buffer to transmit
size_t fill_length;

sample_t* being_transmitted = region_two;

#define SRRC_OVERLAP 4

#define MAX_MESSAGE_LENGTH 1000
#define BITS_PER_SYMBOL 2
char message[MAX_MESSAGE_LENGTH];

#define MIN_FRAME_LENGTH 40
#define MAX_HEADER_LENGTH 100

#define MAX_DATA_LENGTH MAX_MESSAGE_LENGTH + MAX_MESSAGE_LENGTH/MIN_FRAME_LENGTH * MAX_HEADER_LENGTH
uint8_t data[MAX_DATA_LENGTH];

char header[MAX_HEADER_LENGTH] = "IntroDigitalCommunications:eecs354:mxb11:profbuchner";

#define MAX_SYMBOLS_LENGTH (MAX_DATA_LENGTH * 8/BITS_PER_SYMBOL)
//overlap extra samples at the end get filled with the beginning for repeat transmission
sample_t symbols[MAX_SYMBOLS_LENGTH + SRRC_OVERLAP];

sample_t envelope[OUTPUT_BUFFER_LENGTH];
size_t envelope_samples_used;


//called on dma completion
void dma_interrupt() {
  if (fill_length == 0) { //we're not running fast enough!
    //set_fault_light()
    ((void(*)())0)();
    return;
  } else {
      sample_t* temp = being_filled;
      being_filled = being_transmitted;
      being_transmitted = temp;
  }
  do_dma(being_transmitted, fill_length);
  fill_length = 0;
}

int main(void) {

  size_t message_length;
  size_t symbols_length;

  size_t symbols_position;

  while (1) {
    if (new_message == true) {
      message_length = strlen(message);
      size_t data_used =
        frame_message(&encoder,
                      (const uint8_t*)message,
                      message_length,
                      data,
                      MAX_DATA_LENGTH);

      symbols_length = encode_data(&encoder, data, data_used, symbols, MAX_SYMBOLS_LENGTH);
      memcpy(&symbols[symbols_length], &symbols[0], SRRC_OVERLAP * sizeof(sample_t));
      symbols_length += SRRC_OVERLAP;
      symbols_position = 0;
    }
    if (envelope_samples_used == 0) {
      symbols_position +=
        convolve(&convolver,
                 &symbols[symbols_position],
                 (symbols_length - symbols_position < OUTPUT_BUFFER_LENGTH/convolver.M) //FIXME 20 is a magic number
                    ? symbols_length - symbols_position : OUTPUT_BUFFER_LENGTH/convolver.M,
                 envelope,
                 OUTPUT_BUFFER_LENGTH);
      if (symbols_length - symbols_position <= SRRC_OVERLAP) {
        symbols_position = 0;
      }
    }
    if (fill_length == 0) {
      //dma is doing its thing, do the next block
      fill_length = upconvert(&upconverter,
                              envelope,
                              envelope_samples_used,
                              being_filled,
                              OUTPUT_BUFFER_LENGTH);
      envelope_samples_used = 0;
    }
  }
  for (;;); //no return
}
