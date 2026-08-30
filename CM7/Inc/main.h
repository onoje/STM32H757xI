/**
  ******************************************************************************
  * @file    JPEG/JPEG_DecodingUsingFs_DMA/CM7/Inc/main.h
  * @author  MCD Application Team
  * @brief   Header for main.c module for Cortex-M7
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

#include "stm32h747i_eval.h"
#include "stm32h747i_eval_io.h"
#include "stm32h747i_eval_lcd.h"
#include "stm32_lcd.h"
#include "stm32h747i_eval_sdram.h"

/* FatFs includes component */
#include "ff_gen_drv.h"
#include "sd_diskio.h"

/* Exported variables --------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define LCD_FRAME_BUFFER         0xD0000000
#define JPEG_OUTPUT_DATA_BUFFER  0xD0200000

/* Second and third dedicated final ARGB8888 framebuffers - each incoming
   frame gets converted into its own buffer once, then the LTDC layer
   address is switched between them instead of re-copying/re-converting on
   every display swap. A third buffer (not just two) means DMA2D is never
   writing into the buffer LTDC most recently switched away from - LTDC's
   layer address change only actually takes effect at the next VSYNC, not
   instantly, so with only two buffers DMA2D could start overwriting a
   buffer LTDC is still finishing a scan-out of, causing visible tearing.
   With three, a buffer gets a full extra pipeline cycle to be certainly
   done with before it's written again. */
#define LCD_FRAME_BUFFER_1       0xD0800000
#define LCD_FRAME_BUFFER_2       0xD0C00000

/* Raw (still-compressed) JPEG bytes received over the network, kept in
   SDRAM. Three buffers (not just two) give the network an extra frame's
   worth of slack to land in before a slot must be dropped, if a frame
   arrives while the pipeline is still mid-decode on an earlier one - see
   FrameReady[] in main.c. */
#define JPEG_RAW_BUFFER_0        0xD0400000
#define JPEG_RAW_BUFFER_1        0xD0600000
#define JPEG_RAW_BUFFER_2        0xD0A00000
#define JPEG_RAW_BUFFER_MAX_SIZE (2 * 1024 * 1024)

/* Number of rotating raw-JPEG / framebuffer slots. Shared between main.c
   (owns the pipeline) and network_stream.c (fills the raw buffers). */
#define NB_IMAGES 3

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void Error_Handler(void);

#endif /* __MAIN_H */

