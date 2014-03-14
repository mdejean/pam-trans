#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_tim.h"

#include "upconvert.h"
#include "convolve.h"
#include "encode.h"
#include "sample.h"


#define OUTPUT_BUFFER_LENGTH 2048 //8KB

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

uint32_t output_sample_rate = 1000000; //Hz = 1MHz

#define GPIO_DMA_STREAM DMA2_Stream5 //channel 6 stream 5: TIM1 update
#define GPIO_DMA_CHANNEL DMA_Channel_6

//set up buffers in three memory regions so DMA can take them
// without stalling the processor
//this has a side effect of making us unable to use
#define USE_SECTION(a) __attribute__ ((section ((a))))
uint8_t region_one[OUTPUT_BUFFER_LENGTH] USE_SECTION("MEM_A");
uint8_t region_two[OUTPUT_BUFFER_LENGTH] USE_SECTION("MEM_B");
//pointers to the three buffers by current use
uint8_t* being_filled = region_one;
//how much of the buffer to transmit
size_t fill_length;
bool stalled = false;

uint8_t* being_transmitted = region_two;


void do_dma(uint8_t* buffer, size_t length) {
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)buffer;
  DMA_InitStructure.DMA_BufferSize = (uint32_t)length;
  DMA_Init(GPIO_DMA_STREAM, &DMA_InitStructure);


  /* DMA Stream enable */
  DMA_Cmd(DMA_STREAM, ENABLE);
}


//called on dma completion
void swap_buffers() {
  if (fill_length == 0) { //we're not running fast enough!
    //set_fault_light()
    stalled = true;
    return;
  } else {
    uint8_t* temp = being_filled;
    being_filled = being_transmitted;
    being_transmitted = temp;
    do_dma(being_transmitted, fill_length);
    fill_length = 0;
    stalled = false;
  }
}

void update_output_sample_rate() {
  static TIM_TimeBaseInitTypeDef timebase;
  /* Time base configuration */
  timebase.TIM_Period = (SystemCoreClock / 2) / output_sample_rate; //e.g. 168 for 1MHz
  timebase.TIM_Prescaler = 1;
  timebase.TIM_ClockDivision = 0;
  timebase.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM1, &timebase);
}

DMA_InitTypeDef  DMA_InitStructure;
void gpio_dma_init() {
  //set up gpio
  GPIO_InitTypeDef  GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Pin = 0xFF00;//PE8-15
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //push-pull
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOE, &GPIO_InitStructure);


  /* Set up TIM1 to generate a DMA request on overflow */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM1, ENABLE);

  update_output_sample_rate();
  TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

  //set up DMA2 to access SRAM1/2

  /* Enable DMA clock */
  RCC_AHB1PeriphClockCmd(DMA_STREAM_CLOCK, ENABLE);

  /* Reset DMA Stream registers (for debug purpose) */
  DMA_DeInit(GPIO_DMA_STREAM);

  /* wait for DMA stream*/
  while (DMA_GetCmdStatus(GPIO_DMA_STREAM) != DISABLE)
  {
  }

  DMA_InitStructure.DMA_Channel = DMA_CHANNEL;
  DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)being_transmitted;
  DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&(GPIOE->ODR) + 1; //GPIOE 8-15: 2nd byte
  DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToMemory;
  DMA_InitStructure.DMA_BufferSize = 1; //send only 1 byte per request
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Enable;
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
  DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
  DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
  DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
  /* Enable DMA Stream Transfer Complete interrupt */
  DMA_ITConfig(DMA_STREAM, DMA_IT_TCIF0, ENABLE);

}

void DMA2_Stream0_IRQHandler(void) {
  /* Test on DMA Stream Transfer Complete interrupt */
  if(DMA_GetITStatus(GPIO_DMA_STREAM, DMA_IT_TCIF)) {
    /* Clear DMA Stream Transfer Complete interrupt pending bit */
    DMA_ClearITPendingBit(GPIO_DMA_STREAM, DMA_IT_TCIF);
    swap_buffers();
  }
}

int main(void) {
  encode_state encoder;
  convolve_state convolver;
  upconvert_state upconverter;

  size_t message_length = 0;
  size_t symbols_length = 0;

  size_t symbols_position = 0;

  size_t symbols_per_buffer = 0;
  bool new_message = true;

  encode_init(&encoder, 100, (const uint8_t*)header, strlen(header), NULL, 0);
  convolve_init_srrc(&convolver, 0.2, 4, 100); //baudrate = 10kHz
  upconvert_init(&upconverter, 1, 10); //carrier = 100kHz if Fs = 1MHz

  gpio_dma_init();

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

      symbols_per_buffer = OUTPUT_BUFFER_LENGTH/convolver.M;
    }
    if (envelope_samples_used == 0) {
      symbols_position +=
        convolve(&convolver,
                 &symbols[symbols_position],
                 (symbols_length - symbols_position < )
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
      if (stalled) {
        swap_buffers();
      }
      envelope_samples_used = 0;
    }
  }
  for (;;); //no return
}
