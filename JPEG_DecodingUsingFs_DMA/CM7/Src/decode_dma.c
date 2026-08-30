/**
  ******************************************************************************
  * @file    JPEG/JPEG_DecodingUsingFs_DMA/CM7/Src/decode_dma.c
  * @author  MCD Application Team
  * @brief   This file provides routines for JPEG decoding with DMA method,
  *          feeding the decoder directly from a RAM buffer (no SD access
  *          during decode).
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
#include "decode_dma.h"
/** @addtogroup STM32H7xx_HAL_Examples
  * @{
  */

/** @addtogroup JPEG_DecodingUsingFs_DMA
  * @{
  */

/* Private define ------------------------------------------------------------*/

#define CHUNK_SIZE_IN  ((uint32_t)(4096))
#define CHUNK_SIZE_OUT ((uint32_t)(64 * 1024))

/* Private variables ---------------------------------------------------------*/

static uint8_t  *RamSrcPtr = NULL;   /* raw (compressed) JPEG bytes, already in SDRAM */
static uint32_t RamSrcSize = 0;       /* total size of that raw JPEG data */
static uint32_t RamSrcOffset = 0;     /* bytes already handed off to the decoder */
static uint32_t RamSrcChunkSize = 0;  /* size of the chunk currently configured as decoder input */

uint32_t Jpeg_HWDecodingEnd = 0;
uint32_t FrameBufferAddress;

/* Implemented in main.c - lets the application chain the next pipeline
   step (DMA2D fill+copy, then next image's decode) straight from this
   interrupt callback, with no busy-wait loop anywhere */
extern void JPEG_Pipeline_OnDecodeComplete(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Starts JPEG decoding directly from a RAM buffer holding the raw
  *         (still-compressed) JPEG bytes. No SD access happens here or in
  *         any of the callbacks below.
  * @param hjpeg: JPEG handle pointer
  * @param pJpegBuffer: pointer to raw JPEG data already loaded in RAM
  * @param JpegBufferSize: size in bytes of that raw JPEG data
  * @param DestAddress : ARGB destination Frame Buffer Address.
  * @retval None
  */
uint32_t JPEG_Decode_DMA(JPEG_HandleTypeDef *hjpeg, uint8_t *pJpegBuffer, uint32_t JpegBufferSize, uint32_t DestAddress)
{
  /* Reset decoder state so this function can be safely called again for a new image */
  Jpeg_HWDecodingEnd = 0;

  RamSrcPtr       = pJpegBuffer;
  RamSrcSize      = JpegBufferSize;
  RamSrcChunkSize = (RamSrcSize < CHUNK_SIZE_IN) ? RamSrcSize : CHUNK_SIZE_IN;
  RamSrcOffset    = RamSrcChunkSize;

  FrameBufferAddress = DestAddress;

  /* Start JPEG decoding with DMA method, first chunk comes straight from RAM */
  HAL_JPEG_Decode_DMA(hjpeg, RamSrcPtr, RamSrcChunkSize, (uint8_t *)FrameBufferAddress, CHUNK_SIZE_OUT);

  return 0;
}

/**
  * @brief  JPEG Info ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pInfo: JPEG Info Struct pointer
  * @retval None
  */
void HAL_JPEG_InfoReadyCallback(JPEG_HandleTypeDef *hjpeg, JPEG_ConfTypeDef *pInfo)
{
}

/**
  * @brief  JPEG Get Data callback: decoder consumed its current input chunk
  *         and wants more compressed bytes. Since the whole file is already
  *         in RAM, we just slide the pointer forward - no f_read()/SD access.
  * @param hjpeg: JPEG handle pointer
  * @param NbDecodedData: Number of decoded (consumed) bytes from input buffer
  * @retval None
  */
void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
{
  if(NbDecodedData == RamSrcChunkSize)
  {
    uint32_t remaining = RamSrcSize - RamSrcOffset;

    if(remaining > 0)
    {
      RamSrcChunkSize = (remaining < CHUNK_SIZE_IN) ? remaining : CHUNK_SIZE_IN;
      HAL_JPEG_ConfigInputBuffer(hjpeg, RamSrcPtr + RamSrcOffset, RamSrcChunkSize);
      RamSrcOffset += RamSrcChunkSize;
    }
    /* else: entire RAM buffer already handed off, nothing left to feed */
  }
  else
  {
    HAL_JPEG_ConfigInputBuffer(hjpeg, RamSrcPtr + RamSrcOffset - RamSrcChunkSize + NbDecodedData,
                                RamSrcChunkSize - NbDecodedData);
  }
}

/**
  * @brief  JPEG Data Ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pDataOut: pointer to the output data buffer
  * @param OutDataLength: length of output buffer in bytes
  * @retval None
  */
void HAL_JPEG_DataReadyCallback (JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength)
{
  /* Update JPEG encoder output buffer address*/
  FrameBufferAddress += OutDataLength;

  HAL_JPEG_ConfigOutputBuffer(hjpeg, (uint8_t *)FrameBufferAddress, CHUNK_SIZE_OUT);
}

/**
  * @brief  JPEG Error callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_ErrorCallback(JPEG_HandleTypeDef *hjpeg)
{
  Error_Handler();
}

/**
  * @brief  JPEG Decode complete callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
{
  Jpeg_HWDecodingEnd = 1;
  JPEG_Pipeline_OnDecodeComplete();
}

/**
  * @}
  */

/**
  * @}
  */
