#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_dac.h"

#include "output.h"


//these global variables are the output state set up in output_init
uint8_t* front_buffer;
uint8_t* back_buffer;
size_t back_fill_length;
/*at startup we wait for the buffer to be filled before first transmitting 
 *  this is the same as being stalled */
volatile bool stalled = true;

//peripheral configs.
DMA_InitTypeDef dma_config;
DAC_InitTypeDef dac_config;
GPIO_InitTypeDef gpioa_config;

void update_output_sample_rate(uint32_t output_sample_rate) {
  static TIM_TimeBaseInitTypeDef timebase;
  /* Time base configuration */
  timebase.TIM_Period = (SystemCoreClock / 8) / output_sample_rate; //e.g. 168 for 1MHz
  timebase.TIM_Prescaler = 0;
  timebase.TIM_ClockDivision = 0;
  timebase.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM6, &timebase);
  //set TIM6 to generate Update events
  TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);
  /* TIM6 enable counter */
  TIM_Cmd(TIM6, ENABLE);
}

void set_dma_buffer(const uint8_t* buffer, size_t length) {
  DMA_Cmd(DMA1_Stream6, DISABLE); //stop DMA so that we can adjust it

  //probably unnecessary wait
  while (DMA_GetCmdStatus(DMA1_Stream6) != DISABLE);
  
  //the remainder of this structure is set up in output_init()
  dma_config.DMA_Memory0BaseAddr = (uint32_t)buffer;
  dma_config.DMA_BufferSize = length;
  DMA_Init(DMA1_Stream6, &dma_config);
  
  /* DMA Stream enable */
  DMA_Cmd(DMA1_Stream6, ENABLE);
  
  DAC_ClearFlag(DAC_Channel_2, DAC_FLAG_DMAUDR);

  /* Enable DAC Channel2 */
  DAC_Cmd(DAC_Channel_2, ENABLE);

  /* Enable DMA for DAC Channel2 */
  DAC_DMACmd(DAC_Channel_2, ENABLE);
}

//called on dma completion
void swap_buffers() {
  if (back_fill_length == 0) { //we're not running fast enough!
    //set_fault_light()
    stalled = true;
    return;
  } else {
    set_dma_buffer(back_buffer, back_fill_length);
    uint8_t* temp = back_buffer;
    back_buffer = front_buffer;
    front_buffer = temp;
    back_fill_length = 0;
    stalled = false;
  }
}

void DMA1_Stream6_IRQHandler(void) {
  DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);  
  swap_buffers();
}

bool output_init(output_state* o) {
  static bool peripherals_initialized = false;
  if (!peripherals_initialized) {
    //turn on the peripheral clocks
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    
    //set up output pins for analog output
    gpioa_config.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    gpioa_config.GPIO_Mode = GPIO_Mode_AN;
    gpioa_config.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &gpioa_config);

    update_output_sample_rate(o->sample_rate); //sets up TIM6 counting

    /* DAC channel2 Configuration */
    dac_config.DAC_Trigger = DAC_Trigger_T6_TRGO;
    dac_config.DAC_WaveGeneration = DAC_WaveGeneration_None;
    dac_config.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_2, &dac_config);
    
    //don't start DAC until we have actual data in the buffers
    
    /* Reset DMA Stream registers */
    DMA_DeInit(DMA1_Stream6);

    /* wait for DMA stream*/
    while (DMA_GetCmdStatus(DMA1_Stream6) != DISABLE)
    {
    }

    dma_config.DMA_Channel = DMA_Channel_7;
    dma_config.DMA_PeripheralBaseAddr = (uint32_t)&(DAC->DHR8R2); //8 bit samples TODO: switch to 12-bit
    //dma_config.DMA_Memory0BaseAddr = (uint32_t)being_transmitted;
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
    //DMA_Init(DMA1_Stream6, &dma_config); don't init until struct is fully filled out
    
    //enable transfer complete interrupt
    DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);  
    DMA_ITConfig(DMA1_Stream6, DMA_IT_TC, ENABLE);
    
    //DMA_Cmd(DMA1_Stream6, ENABLE);
    
    peripherals_initialized = true;
  }
  front_buffer = o->region_one;
  back_buffer = o->region_two;
  back_fill_length = 0;
  update_output_sample_rate(o->sample_rate);
  
  return true;
}


uint8_t* output_get_buffer(output_state* o) {
  if (back_fill_length > 0) {
    return NULL;
  } else {
    return back_buffer;
  }
}

bool output_set_filled(output_state* o, size_t fill_length) {
  if (fill_length > o->output_buffer_length) {
    return false;
  }
  
  if (back_fill_length > 0) {
    return false;
  }
  
  back_fill_length = fill_length;
  
  //if we were stalled, start transmitting the back buffer now!
  if (stalled) {
    swap_buffers();
  }
  return true;
}

bool output_stalled() {
  return stalled;
}
