/**
  ******************************************************************************
  * @file    network_stream.h
  * @brief   Receives a live JPEG frame stream over a raw TCP socket and feeds
  *          it into the existing decode/display pipeline (see main.c).
  ******************************************************************************
  */

#ifndef __NETWORK_STREAM_H
#define __NETWORK_STREAM_H

#include <stdint.h>

/* TCP port the board listens on. The PC-side sender script must connect to
   this port at the board's static IP (see MX_LWIP_Init() in lwip.c). */
#define NETWORK_STREAM_PORT 5001

/**
  * @brief  Starts listening for an incoming frame-stream TCP connection.
  *         Must be called after MX_LWIP_Init(). Non-blocking: registers
  *         lwIP callbacks and returns immediately.
  * @retval None
  */
void Network_Stream_Init(void);

#endif /* __NETWORK_STREAM_H */
