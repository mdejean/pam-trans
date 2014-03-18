#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_tim.h"

#include "upconvert.h"
#include "convolve.h"
#include "encode.h"
#include "sample.h"


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

#define MAX_DATA_LENGTH MAX_MESSAGE_LENGTH + MAX_MESSAGE_LENGTH/MIN_FRAME_LENGTH * MAX_HEADER_LENGTH
uint8_t data[MAX_DATA_LENGTH];

char header[MAX_HEADER_LENGTH] = "IntroDigitalCommunications:eecs354:mxb11:profbuchner";

#define MAX_SYMBOLS_LENGTH (MAX_DATA_LENGTH * 8/BITS_PER_SYMBOL)
//overlap extra samples at the end get filled with the beginning for repeat transmission
sample_t symbols[MAX_SYMBOLS_LENGTH + SRRC_OVERLAP];

sample_t envelope[OUTPUT_BUFFER_LENGTH];
size_t envelope_samples_used;

uint32_t output_sample_rate = 1000000; //Hz = 1MHz

//set up buffers in three memory regions so DMA can take them
// without stalling the processor
//this has a side effect of making us unable to use most of mem_a
#define USE_SECTION(a) __attribute__ ((section ((a))))
uint8_t region_one[OUTPUT_BUFFER_LENGTH] USE_SECTION("MEM_A");
uint8_t region_two[OUTPUT_BUFFER_LENGTH] USE_SECTION("MEM_B");
//pointers to the three buffers by current use
uint8_t* being_filled = region_one;
//how much of the buffer to transmit
size_t fill_length;

/*at startup we wait for the buffer to be filled before first transmitting 
 *  this is the same as being stalled */
bool stalled = true;

uint8_t* being_transmitted = region_two;


DMA_InitTypeDef  dma_config;
void set_dma_buffer(uint8_t* buffer, size_t length) {
  DMA_Cmd(DMA_STREAM, DISABLE); //stop DMA so that we can adjust it

  //probably unnecessary wait
  while (DMA_GetCmdStatus(GPIO_DMA_STREAM) != DISABLE);
  
  //the remainder of this structure is set up in output_init()
  dma_config.DMA_Memory0BaseAddr = (uint32_t)buffer;
  dma_config.DMA_BufferSize = length;
  DMA_Init(GPIO_DMA_STREAM, &dma_config);
  /* DMA Stream enable */
  DMA_Cmd(DMA_STREAM, ENABLE);

  /* Enable DAC Channel2 */
  DAC_Cmd(DAC_Channel_2, ENABLE);

  /* Enable DMA for DAC Channel2 */
  DAC_DMACmd(DAC_Channel_2, ENABLE);
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
    set_dma_buffer(being_transmitted, fill_length);
    fill_length = 0;
    stalled = false;
  }
}

void update_output_sample_rate() {
  static TIM_TimeBaseInitTypeDef timebase;
  /* Time base configuration */
  timebase.TIM_Period = (SystemCoreClock / 2) / output_sample_rate; //e.g. 168 for 1MHz
  timebase.TIM_Prescaler = 0;
  timebase.TIM_ClockDivision = 0;
  timebase.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM6, &timebase);
  //set TIM1 to generate Update events
  TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);
  /* TIM6 enable counter */
  TIM_Cmd(TIM6, ENABLE);
}

void output_init() {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);

  update_output_sample_rate(); //sets up TIM6 counting

  /* DAC channel2 Configuration */
  dac_config.DAC_Trigger = DAC_Trigger_T6_TRGO;
  dac_config.DAC_WaveGeneration = DAC_WaveGeneration_None;
  dac_config.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
  DAC_Init(DAC_Channel_2, &dac_config);
  
  //don't start DAC until we have actual data in the buffers
  
  /* Enable DMA clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

  /* Reset DMA Stream registers */
  DMA_DeInit(DMA1_Stream5);

  /* wait for DMA stream*/
  while (DMA_GetCmdStatus(GPIO_DMA_STREAM) != DISABLE)
  {
  }

  dma_config.DMA_Channel = DMA_Channel_7;
  dma_config.DMA_PeripheralBaseAddr = (uint32_t)DAC_DHR8R2_ADDRESS; //8 bit samples TODO: switch to 12-bit
  //dma_config.DMA_Memory0BaseAddr = (uint32_t)being_transmitted; //GPIOE 8-15: 2nd byte
  dma_config.DMA_DIR = DMA_DIR_MemoryToPeripheral;
  //dma_config.DMA_BufferSize = 0; filled by set_dma_buffer
  dma_config.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  dma_config.DMA_MemoryInc = DMA_MemoryInc_Enable;
  dma_config.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  dma_config.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
  dma_config.DMA_Mode = DMA_Mode_Normal;
  dma_config.DMA_Priority = DMA_Priority_High;
  dma_config.DMA_FIFOMode = DMA_FIFOMode_Disable;
  dma_config.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
  dma_config.DMA_MemoryBurst = DMA_MemoryBurst_Single;
  dma_config.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
  //DMA_Init(DMA1_Stream5, &dma_config); don't init until struct is fully filled out
  
  //DMA_Cmd(DMA1_Stream5, ENABLE);
}

void DMA1_Stream5_IRQHandler(void) {
    swap_buffers();
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

  output_init();

  for (;;) {
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
                 (symbols_length - symbols_position < symbols_per_buffer)
                    ? symbols_length - symbols_position : symbols_per_buffer,
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
}

void NMI_Handler(void)
{
}

void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
}

