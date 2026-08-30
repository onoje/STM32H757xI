/**
  ******************************************************************************
  * @file    JPEG/JPEG_DecodingUsingFs_DMA/CM7/Src/main.c
  * @author  MCD Application Team
  * @brief   This sample code shows how to use the HW JPEG to Decode a JPEG file with DMA method.
  *          This is the main program for Cortex-M7    
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "decode_dma.h"
#include "lwip.h"
#include "network_stream.h"

/** @addtogroup STM32H7xx_HAL_Examples
  * @{
  */

/** @addtogroup JPEG_DecodingUsingFs_DMA
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static uint32_t LCD_X_Size = 0, LCD_Y_Size= 0;

/* Raw JPEG buffers and their bookkeeping - shared with network_stream.c,
   which fills them from the incoming TCP stream (see the extern
   declarations there). Not static for that reason. */
const uint32_t ImageRawAddr[NB_IMAGES] = { JPEG_RAW_BUFFER_0, JPEG_RAW_BUFFER_1, JPEG_RAW_BUFFER_2 };
volatile uint32_t ImageRawSize[NB_IMAGES];

/* Set by network_stream.c once a full frame has landed in ImageRawAddr[idx];
   cleared by the pipeline once it's done reading that slot, freeing it for
   the network to reuse. This is the only handshake between the two files. */
volatile uint8_t FrameReady[NB_IMAGES] = { 0, 0, 0 };

/* Monotonic completion order, stamped by network_stream.c alongside
   FrameReady[idx]=1 (see FrameSeqNum there). Lets the pipeline tell which
   ready slot is the *newest* one when more than one is waiting - without
   this, the pipeline always decoded slots in strict idx order, so if it
   ever fell even slightly behind the network's delivery rate, the screen
   would keep showing genuinely stale frames (older ones still waiting
   their turn) while newer arrivals got dropped for lack of a free slot -
   bounded staleness, but very visible, and *worse* with more buffers
   (more room to fall behind before anything gets dropped). */
volatile uint32_t FrameSeqNum[NB_IMAGES] = { 0, 0, 0 };

/* Each image gets its own dedicated, already-converted ARGB8888 framebuffer */
static const uint32_t LcdFrameBufferAddr[NB_IMAGES] = { LCD_FRAME_BUFFER, LCD_FRAME_BUFFER_1, LCD_FRAME_BUFFER_2 };

/* Decode duration of each image, in ms (HAL_GetTick() / SysTick based).
   Watch these in STM32CubeIDE's Live Expressions view while running. */
volatile uint32_t DecodeTime_ms[NB_IMAGES] = {0};

/* Time spent converting/copying the decoded image into its dedicated
   framebuffer via DMA2D - now happens only once per image, not per loop */
volatile uint32_t CopyTime_ms[NB_IMAGES] = {0};

/* Time spent switching the LTDC layer address between the two pre-converted
   framebuffers - this replaces the per-loop DMA2D copy */
volatile uint32_t SwitchTime_ms[NB_IMAGES] = {0};

/* Same measurement in microseconds, via the Cortex-M7 DWT cycle counter -
   HAL_GetTick() (1ms resolution) can't measure an operation this fast */
volatile uint32_t SwitchTime_us[NB_IMAGES] = {0};

/* TEMP DIAGNOSTIC (portrait-mode stutter investigation): DecodeTime_ms/
   CopyTime_ms/SwitchTime_us only cover the pipeline's own compute steps -
   none of them measure how long PIPE_WAITING_RELOAD actually waits for
   HAL_LTDC_ReloadEventCallback() to confirm the display applied the
   switch. If that wait is normally ~1 VSYNC period but is taking much
   longer in portrait mode, the compute-step timers would look completely
   normal (as they've measured) while the real achieved frame rate is far
   lower - which would look exactly like stutter. */
volatile uint32_t ReloadWaitTime_ms[NB_IMAGES] = {0};
static uint32_t PipelineReloadWaitTickStart = 0;

JPEG_HandleTypeDef    JPEG_Handle;

/* Not static: stm32h7xx_it.c's DMA2D_IRQHandler() needs to reach this handle */
DMA2D_HandleTypeDef    DMA2D_Handle;
static JPEG_ConfTypeDef       JPEG_Info;

/* Fully interrupt-driven decode+convert+display pipeline - no busy-wait loop
   anywhere. Each stage is kicked off from the previous stage's HW callback:
   JPEG decode complete -> DMA2D fill complete -> DMA2D copy complete -> show
   this image -> next frame's decode -> ... forever. The CPU is free
   (sleeping via __WFI()) between interrupts instead of spinning. The source
   is now a live network stream: if the next frame hasn't arrived yet, the
   pipeline parks in PIPE_WAITING_FRAME until network_stream.c wakes it. */
typedef enum
{
  PIPE_WAITING_FRAME = 0,
  PIPE_DECODING,
  PIPE_COPYING,
  /* Requested the LTDC switch to this frame's buffer at the next vertical
     blanking, but the display hasn't confirmed it actually happened yet
     (HAL_LTDC_ReloadEventCallback() hasn't fired). More buffers alone
     don't guarantee tear-free display if decode+copy keeps outrunning the
     screen's own refresh rate indefinitely - only waiting for each switch
     to be confirmed before reusing a buffer does, because it's the only
     thing that actually paces the pipeline to the display's real speed
     instead of assuming N buffers is always enough headroom. */
  PIPE_WAITING_RELOAD
} PipelineState_t;

static volatile PipelineState_t PipelineState = PIPE_WAITING_FRAME;
static volatile uint32_t PipelineIdx = 0;
static uint32_t PipelineXPos = 0, PipelineYPos = 0;
static uint32_t PipelineDecodeTickStart = 0;
static uint32_t PipelineCopyTickStart = 0;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void LCD_BriefDisplay(void);
static void Pipeline_StartDecodeOfFreshest(void);
static void DMA2D_XferCpltCallback(DMA2D_HandleTypeDef *hdma2d);
static void DMA2D_StartCopy(uint32_t *pSrc, uint32_t *pDst, uint16_t x, uint16_t y, uint16_t xsize, uint16_t ysize, uint32_t ChromaSampling);
static void MPU_Config(void);
static void CPU_CACHE_Enable(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  uint8_t  lcd_status = BSP_ERROR_NONE;

  /* System Init, System clock, voltage scaling and L1-Cache configuration are done by CPU1 (Cortex-M7)
     in the meantime Domain D2 is put in STOP mode(Cortex-M4 in deep-sleep)
  */

  /* Configure the MPU attributes as Write Through for SDRAM*/
  MPU_Config();

  /* Enable the CPU Cache */
  CPU_CACHE_Enable();

  /* STM32H7xx HAL library initialization:
       - Systick timer is configured by default as source of time base, but user 
         can eventually implement his proper time base source (a general purpose 
         timer for example or other time source), keeping in mind that Time base 
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and 
         handled in milliseconds basis.
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();
  
  /* Configure the system clock to 400 MHz */
  SystemClock_Config();

  /* Enable the Cortex-M7 cycle counter (DWT) for microsecond-precision
     timing - needed for operations faster than HAL_GetTick()'s 1ms resolution */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* When system initialization is finished, Cortex-M7 could wakeup (when needed) the Cortex-M4  by means of 
     HSEM notification or by any D2 wakeup source (SEV,EXTI..)   */  
  
  /* Initialize the LED3 (Red LED , set to On when error) */
  BSP_LED_Init(LED3);

  /*##-1- JPEG Initialization ################################################*/   
   /* Init the HAL JPEG driver */
  JPEG_Handle.Instance = JPEG;
  HAL_JPEG_Init(&JPEG_Handle);

  /* Enable DMA2D interrupt - needed so HAL_DMA2D_Start_IT()'s completion
     callback (DMA2D_XferCpltCallback) fires without any CPU polling/waiting */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 0x07, 0x0F);
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);


  /*##-2- LCD Configuration ##################################################*/
  /* Initialize the LCD   */

  /* TEMP DIAGNOSTIC (diagonal-tear investigation): native PORTRAIT
     (480x800) instead of the usual LANDSCAPE (800x480) - this skips the
     MADCTL row/column exchange entirely (OTM8009A_Init() only sends that
     command for LANDSCAPE), testing whether the tear is tied to that
     remapping. BSP_LCD_Init(Instance, Orientation) can't be used for this:
     its 2-argument wrapper always passes the hardcoded LCD_DEFAULT_WIDTH/
     HEIGHT (800/480) to BSP_LCD_InitEx() regardless of Orientation, so
     Orientation alone would reach OTM8009A_Init() correctly but LTDC/DSI
     would still be configured for 800x480 - calling BSP_LCD_InitEx()
     directly with the swapped dimensions is required.
     To revert: change back to
       lcd_status = BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE); */
  lcd_status = BSP_LCD_InitEx(0, LCD_ORIENTATION_PORTRAIT, LTDC_PIXEL_FORMAT_ARGB8888, 480, 800);
  if(lcd_status != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* BSP_LCD_SetLayerAddress() defaults to an *immediate* LTDC reload -
     the new framebuffer address can take effect mid-scanout, splitting
     the visible screen between old and new frame content (diagonal/
     staircase tearing). BSP_LCD_RELOAD_NONE makes it only stage the
     address change; DMA2D_XferCpltCallback() below then explicitly
     requests HAL_LTDC_Reload(..., LTDC_RELOAD_VERTICAL_BLANKING) so the
     switch only actually happens between frames. */
  BSP_LCD_Relaod(0, BSP_LCD_RELOAD_NONE);

  UTIL_LCD_SetFuncDriver(&LCD_Driver);
  UTIL_LCD_SetLayer(0);


  /* Get the LCD Width */
  BSP_LCD_GetXSize(0, &LCD_X_Size);
  BSP_LCD_GetYSize(0, &LCD_Y_Size);
  /* Cear LCD */
  UTIL_LCD_Clear(UTIL_LCD_COLOR_BLACK);
  
  /* Display example brief   */
  LCD_BriefDisplay();

  /*##-3- Bring up the network and start listening for the frame stream #####*/
  MX_LWIP_Init();
  Network_Stream_Init();

  /* No frame has arrived yet - the pipeline starts idle and is kicked into
     PIPE_DECODING by network_stream.c the moment the first full frame lands
     in ImageRawAddr[0] (see JPEG_Pipeline_OnFrameReceived()). Every frame
     after that is chained the same way, from HW callbacks, forever. */
  PipelineIdx   = 0;
  PipelineState = PIPE_WAITING_FRAME;

  /* This project's Ethernet driver is polled, not interrupt-driven: there is
     no ETH_IRQHandler, ethernetif_input() checks the ETH DMA's RX ring
     directly. So MX_LWIP_Process() must be called regularly - here, once
     per __WFI() wakeup - and the 1ms SysTick interrupt alone guarantees that
     happens at least every millisecond even with no other activity. */
  while (1)
  {
    MX_LWIP_Process();
    __WFI();
  }
}


/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 400000000 (CM7 CPU Clock)
  *            HCLK(Hz)                       = 200000000 (CM4 CPU, AXI and AHBs Clock)
  *            AHB Prescaler                  = 2
  *            D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
  *            D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
  *            D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
  *            D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 5
  *            PLL_N                          = 160
  *            PLL_P                          = 2
  *            PLL_Q                          = 4
  *            PLL_R                          = 2
  *            VDD(V)                         = 3.3
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;

  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    Error_Handler();
  }
  
/* Select PLL as system clock source and configure  bus clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                 RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;  
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2; 
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2; 
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2; 
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
  if(ret != HAL_OK)
  {
    Error_Handler();
  }

 /*
  Note : The activation of the I/O Compensation Cell is recommended with communication  interfaces
          (GPIO, SPI, FMC, QSPI ...)  when  operating at  high frequencies(please refer to product datasheet)       
          The I/O Compensation Cell activation  procedure requires :
        - The activation of the CSI clock
        - The activation of the SYSCFG clock
        - Enabling the I/O Compensation Cell : setting bit[0] of register SYSCFG_CCCSR
 */
 
  /*activate CSI clock mondatory for I/O Compensation Cell*/  
  __HAL_RCC_CSI_ENABLE() ;
    
  /* Enable SYSCFG clock mondatory for I/O Compensation Cell */
  __HAL_RCC_SYSCFG_CLK_ENABLE() ;
  
  /* Enables the I/O Compensation Cell */    
  HAL_EnableCompensationCell();  
}

/**
  * @brief  Configure the MPU attributes as Write Through for External SDRAM.
  * @note   The Base Address is SDRAM_DEVICE_ADDR .
  *         The Configured Region Size is 32MB because same as SDRAM size.
  * @param  None
  * @retval None
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;
  
  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU as Strongly ordered for not defined regions */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU attributes as WT for SDRAM */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = SDRAM_DEVICE_ADDR;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure RAM_D1 (AXI SRAM, 0x24000000) as Normal, non-cacheable memory.
     This is where the linker script now places .data/.bss (including all of
     lwIP's own memory) plus the Ethernet DMA descriptors and RX buffer pool
     (.RxDescripSection/.TxDescripSection/.Rx_PoolSection). That memory is
     written/read directly by the Ethernet DMA controller, which does not go
     through the CPU's D-Cache - if this region were left cacheable, the CPU
     and the DMA could each see a different, stale copy of the same data.

     TypeExtField=MPU_TEX_LEVEL1 together with IsCacheable/IsBufferable=NOT
     is what actually selects "Normal, non-cacheable" per the ARMv7-M MPU
     TEX/C/B encoding. TEX_LEVEL0 with the same C/B bits instead selects
     "Strongly Ordered" memory, which is stricter: it also forbids unaligned
     accesses outright, and that's exactly what crashed here - a plain
     struct field copy in lwIP's ARP code (acd_arp_reply(), a 6-byte
     hardware-address copy) triggered a HardFault the moment it touched this
     region, because that copy isn't naturally 4-byte aligned. */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure D2 domain SRAM (0x30000000, SRAM1+SRAM2+SRAM3, 288KB) as
     Normal, non-cacheable too. lwipopts.h pins lwIP's entire packet memory
     heap here via LWIP_RAM_HEAP_POINTER (0x30004000) - a separate, fixed
     address that CubeMX generates independently of where main.c's own
     .data/.bss end up (that part lives in RAM_D1, region 2 above). Every
     outgoing packet (ARP replies included) is allocated from this heap, so
     it needs the same non-cacheable treatment as RAM_D1: without it, the
     ETH DMA reads stale/garbage bytes straight from physical SRAM while the
     CPU's freshly written packet data is still sitting in D-Cache. */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}


/**
* @brief  CPU L1-Cache enable.
* @param  None
* @retval None
*/
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

/**
  * @brief  Display Example description.
  * @param  None
  * @retval None
  */
static void LCD_BriefDisplay(void)
{
  /* Laid out for the board's current 480-wide portrait orientation (see
     LCD_ORIENTATION_PORTRAIT in main()) - the original single-line title
     was sized for the 800-wide landscape screen and got clipped/cramped
     ("...live Et") once the screen narrowed to 480px. Wrapped across two
     lines and given explicit Y positions (LCD_Y_Size=800 leaves plenty of
     headroom) instead of the LINE() macro, so nothing overlaps. */
  UTIL_LCD_Clear(UTIL_LCD_COLOR_WHITE);
  UTIL_LCD_SetBackColor(UTIL_LCD_COLOR_BLUE);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_BLUE);
  UTIL_LCD_FillRect(0, 0, LCD_X_Size, 190, UTIL_LCD_COLOR_BLUE);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
  /* UTIL_LCD's DrawProp starts with pFont == NULL (no default font) until
     the first UTIL_LCD_SetFont() call - drawing text before that call
     dereferences a NULL font pointer. Must come before the first
     DisplayStringAt below, not after it. */
  UTIL_LCD_SetFont(&Font24);
  UTIL_LCD_DisplayStringAt(0, 20,  (uint8_t *)"JPEG Decoding from a", CENTER_MODE);
  UTIL_LCD_DisplayStringAt(0, 50,  (uint8_t *)"live Ethernet stream", CENTER_MODE);
  UTIL_LCD_SetFont(&Font16);
  UTIL_LCD_DisplayStringAt(0, 100, (uint8_t *)"Waiting for RTP/JPEG packets", CENTER_MODE);
  UTIL_LCD_DisplayStringAt(0, 120, (uint8_t *)"on UDP port 5001...", CENTER_MODE);
  UTIL_LCD_DisplayStringAt(0, 150, (uint8_t *)"Board IP: 192.168.1.20", CENTER_MODE);
}

/**
  * @brief  Picks the *newest* ready slot (highest FrameSeqNum) to decode
  *         next, instead of following NB_IMAGES in strict rotation - any
  *         other slot that was also sitting ready gets discarded (freed
  *         without ever being decoded) since it's now known to be stale.
  *         Only actually starts a decode if the pipeline is genuinely idle
  *         (PIPE_WAITING_FRAME); otherwise does nothing (a decode already
  *         in flight will re-check for the freshest slot itself once it
  *         finishes, from HAL_LTDC_ReloadEventCallback() below).
  * @retval None
  */
static void Pipeline_StartDecodeOfFreshest(void)
{
  uint32_t i, freshestIdx = NB_IMAGES;

  if(PipelineState != PIPE_WAITING_FRAME)
  {
    return;
  }

  for(i = 0; i < NB_IMAGES; i++)
  {
    if(FrameReady[i] && ((freshestIdx == NB_IMAGES) || (FrameSeqNum[i] > FrameSeqNum[freshestIdx])))
    {
      freshestIdx = i;
    }
  }

  if(freshestIdx == NB_IMAGES)
  {
    return; /* nothing ready yet */
  }

  for(i = 0; i < NB_IMAGES; i++)
  {
    if((i != freshestIdx) && FrameReady[i])
    {
      /* Older frame that arrived and sat waiting while the pipeline was
         busy - discarding it (instead of decoding it in turn) is what
         keeps the display caught up to "now" instead of trailing behind
         by however many frames piled up. */
      FrameReady[i] = 0;
    }
  }

  PipelineIdx   = freshestIdx;
  PipelineState = PIPE_DECODING;
  PipelineDecodeTickStart = HAL_GetTick();

  JPEG_Decode_DMA(&JPEG_Handle, (uint8_t *)ImageRawAddr[freshestIdx], ImageRawSize[freshestIdx], JPEG_OUTPUT_DATA_BUFFER);
}

/**
  * @brief  Called from network_stream.c once a full JPEG frame has landed in
  *         ImageRawAddr[idx]. If the pipeline is idle, this may kick off a
  *         decode - not necessarily of idx itself, see
  *         Pipeline_StartDecodeOfFreshest(). If the pipeline is still busy,
  *         this frame just sits ready (FrameReady[idx] == 1) until then.
  * @param  idx: which raw buffer slot just became ready (unused - kept for
  *              interface/documentation clarity at the call site)
  * @retval None
  */
void JPEG_Pipeline_OnFrameReceived(uint32_t idx)
{
  (void)idx;
  Pipeline_StartDecodeOfFreshest();
}

/**
  * @brief  Called from decode_dma.c's HAL_JPEG_DecodeCpltCallback() - i.e. in
  *         JPEG_IRQn interrupt context. Decode just finished: record its
  *         duration, then kick off the (also non-blocking) DMA2D copy for
  *         this image. No busy-wait anywhere in this chain.
  *
  *         There used to be a DMA2D_StartFill() step here first, painting
  *         the whole destination buffer white before the copy - that only
  *         matters if the image is smaller than the LCD (letterboxing).
  *         The PC senders always crop-to-fill so ImageWidth/Height equal
  *         LCD_X_Size/Y_Size exactly; the copy below then overwrites every
  *         pixel the fill touched anyway, so the fill was pure wasted time
  *         on every single frame - removed on the mentor's call.
  * @retval None
  */
void JPEG_Pipeline_OnDecodeComplete(void)
{
  DecodeTime_ms[PipelineIdx] = HAL_GetTick() - PipelineDecodeTickStart;

  HAL_JPEG_GetInfo(&JPEG_Handle, &JPEG_Info);

  /* A frame wider/taller than the LCD would make (LCD_X_Size - ImageWidth)
     underflow (both are unsigned), wrapping into a huge offset instead of
     going negative - clamp to top-left instead of trusting the source to
     always match the LCD size exactly. */
  PipelineXPos = (JPEG_Info.ImageWidth  < LCD_X_Size) ? (LCD_X_Size - JPEG_Info.ImageWidth)  / 2 : 0;
  PipelineYPos = (JPEG_Info.ImageHeight < LCD_Y_Size) ? (LCD_Y_Size - JPEG_Info.ImageHeight) / 2 : 0;

  PipelineCopyTickStart = HAL_GetTick();
  PipelineState = PIPE_COPYING;

  DMA2D_StartCopy((uint32_t *)JPEG_OUTPUT_DATA_BUFFER, (uint32_t *)LcdFrameBufferAddr[PipelineIdx],
                   (uint16_t)PipelineXPos, (uint16_t)PipelineYPos,
                   (uint16_t)JPEG_Info.ImageWidth, (uint16_t)JPEG_Info.ImageHeight, JPEG_Info.ChromaSubsampling);
}

/**
  * @brief  DMA2D transfer-complete interrupt callback (DMA2D_IRQn context).
  *         Fires once the copy is done and chains straight to the next
  *         pipeline step.
  * @retval None
  */
static void DMA2D_XferCpltCallback(DMA2D_HandleTypeDef *hdma2d)
{
  if(PipelineState == PIPE_COPYING)
  {
    uint32_t tick_start, cyc_start;

    CopyTime_ms[PipelineIdx] = HAL_GetTick() - PipelineCopyTickStart;

    /* This frame's buffer is fully converted - request it be shown, but
       don't touch any buffer/pipeline bookkeeping yet. Advancing here
       (like this code used to) assumes N buffers is always enough
       headroom before this same slot is written again - that assumption
       breaks the moment decode+fill+copy keeps outrunning the display's
       own refresh rate, no matter how many buffers there are. Actually
       waiting for HAL_LTDC_ReloadEventCallback() below, which only fires
       once the display has genuinely applied this switch, is what
       guarantees a buffer is never overwritten while still on screen. */
    tick_start = HAL_GetTick();
    cyc_start  = DWT->CYCCNT;

    /* Stages the new address only (BSP_LCD_RELOAD_NONE set at init) - it
       does not take visual effect until the reload below actually happens,
       so this alone can never race with what LTDC is currently scanning
       out. */
    BSP_LCD_SetLayerAddress(0, 0, LcdFrameBufferAddr[PipelineIdx]);

    /* Request the switch for the *next* vertical blanking interval, not
       right now - this is what actually prevents the diagonal/staircase
       tearing: without it (or with the default immediate reload), the
       switch could land in the middle of an active scanout, so the top
       part of that refresh shows the old frame and the bottom part
       shows the new one. */
    HAL_LTDC_Reload(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING);

    SwitchTime_ms[PipelineIdx] = HAL_GetTick() - tick_start;
    SwitchTime_us[PipelineIdx] = (DWT->CYCCNT - cyc_start) / (SystemCoreClock / 1000000U);

    /* TEMP DIAGNOSTIC: mark when the wait for reload confirmation starts -
       see ReloadWaitTime_ms declaration above. */
    PipelineReloadWaitTickStart = HAL_GetTick();

    PipelineState = PIPE_WAITING_RELOAD;
  }
}

/**
  * @brief  LTDC reload-complete interrupt callback (LTDC_IRQn context,
  *         overrides the HAL's default weak no-op). Fires once the display
  *         has genuinely started scanning out the buffer requested in
  *         DMA2D_XferCpltCallback() above - only past this point is that
  *         buffer's *previous* slot guaranteed safe to overwrite again.
  * @retval None
  */
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
  if(PipelineState == PIPE_WAITING_RELOAD)
  {
    uint32_t finishedIdx = PipelineIdx;

    /* TEMP DIAGNOSTIC: how long PIPE_WAITING_RELOAD actually waited here -
       see ReloadWaitTime_ms declaration near the top of this file. */
    ReloadWaitTime_ms[finishedIdx] = HAL_GetTick() - PipelineReloadWaitTickStart;

    /* This slot's raw JPEG bytes are fully consumed - free it so
       network_stream.c can write the next incoming frame into it. */
    FrameReady[finishedIdx] = 0;

    /* Go straight into decoding the freshest frame that's ready now, if
       any (discarding any others that piled up while busy) - otherwise
       park in PIPE_WAITING_FRAME until JPEG_Pipeline_OnFrameReceived()
       wakes the pipeline back up. No longer a fixed idx+1 rotation: see
       Pipeline_StartDecodeOfFreshest(). */
    PipelineState = PIPE_WAITING_FRAME;
    Pipeline_StartDecodeOfFreshest();
  }
}

/**
  * @brief  Start a non-blocking YCbCr->ARGB8888 copy (DMA2D M2M+PFC mode).
  * @param  pSrc: Pointer to source buffer
  * @param  pDst: Pointer to destination buffer
  * @param  x: destination Horizontal offset.
  * @param  y: destination Vertical offset.
  * @param  xsize: image width
  * @param  ysize: image Height
  * @param  ChromaSampling : YCbCr Chroma sampling : 4:2:0, 4:2:2 or 4:4:4
  * @retval None
  */
static void DMA2D_StartCopy(uint32_t *pSrc, uint32_t *pDst, uint16_t x, uint16_t y, uint16_t xsize, uint16_t ysize, uint32_t ChromaSampling)
{
  uint32_t cssMode = DMA2D_CSS_420, inputLineOffset = 0;
  uint32_t destination = 0;
  uint32_t xSize = 0;

  if(ChromaSampling == JPEG_420_SUBSAMPLING)
  {
    cssMode = DMA2D_CSS_420;

    inputLineOffset = xsize % 16;
    if(inputLineOffset != 0)
    {
      inputLineOffset = 16 - inputLineOffset;
    }
  }
  else if(ChromaSampling == JPEG_444_SUBSAMPLING)
  {
    cssMode = DMA2D_NO_CSS;

    inputLineOffset = xsize % 8;
    if(inputLineOffset != 0)
    {
      inputLineOffset = 8 - inputLineOffset;
    }
  }
  else if(ChromaSampling == JPEG_422_SUBSAMPLING)
  {
    cssMode = DMA2D_CSS_422;

    inputLineOffset = xsize % 16;
    if(inputLineOffset != 0)
    {
      inputLineOffset = 16 - inputLineOffset;
    }
  }

  /*##-1- Configure the DMA2D Mode, Color Mode and output offset #############*/
  DMA2D_Handle.Init.Mode         = DMA2D_M2M_PFC;
  DMA2D_Handle.Init.ColorMode    = DMA2D_OUTPUT_ARGB8888;
  DMA2D_Handle.Init.OutputOffset = LCD_X_Size - xsize;
  DMA2D_Handle.Init.AlphaInverted = DMA2D_REGULAR_ALPHA;  /* No Output Alpha Inversion*/
  DMA2D_Handle.Init.RedBlueSwap   = DMA2D_RB_REGULAR;     /* No Output Red & Blue swap */

  /*##-2- DMA2D Callbacks Configuration ######################################*/
  DMA2D_Handle.XferCpltCallback  = DMA2D_XferCpltCallback;

  /*##-3- Foreground Configuration ###########################################*/
  DMA2D_Handle.LayerCfg[1].AlphaMode = DMA2D_REPLACE_ALPHA;
  DMA2D_Handle.LayerCfg[1].InputAlpha = 0xFF;
  DMA2D_Handle.LayerCfg[1].InputColorMode = DMA2D_INPUT_YCBCR;
  DMA2D_Handle.LayerCfg[1].ChromaSubSampling = cssMode;
  DMA2D_Handle.LayerCfg[1].InputOffset = inputLineOffset;
  DMA2D_Handle.LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR; /* No ForeGround Red/Blue swap */
  DMA2D_Handle.LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA; /* No ForeGround Alpha inversion */

  DMA2D_Handle.Instance          = DMA2D;

  /*##-4- DMA2D Initialization     ###########################################*/
  HAL_DMA2D_Init(&DMA2D_Handle);
  HAL_DMA2D_ConfigLayer(&DMA2D_Handle, 1);

  /*##-5-  copy the new decoded frame to the LCD Frame buffer, non-blocking ##*/
  BSP_LCD_GetXSize(0, &xSize);
  destination = (uint32_t)pDst + ((y * xSize) + x) * 4;

  HAL_DMA2D_Start_IT(&DMA2D_Handle, (uint32_t)pSrc, destination, xsize, ysize);
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */ 

/**
  * @brief  On Error Handler.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  BSP_LED_On(LED3);
  while(1) { ; } /* Blocking on error */
}

/**
  * @}
  */

/**
  * @}
  */
