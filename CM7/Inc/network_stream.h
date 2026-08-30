/**
  ******************************************************************************
  * @file    network_stream.h
  * @brief   Receives a live JPEG frame stream sent as RTP (RFC 3550) carrying
  *          RFC 2435 "RTP Payload Format for JPEG-compressed Video" over raw
  *          UDP, reassembles each frame into a standalone JPEG file, and
  *          feeds it into the existing decode/display pipeline (see main.c).
  ******************************************************************************
  */

#ifndef __NETWORK_STREAM_H
#define __NETWORK_STREAM_H

#include <stdint.h>

/* UDP port the board listens on. The PC-side sender script sends RTP
   packets to this port at the board's static IP (see MX_LWIP_Init() in
   lwip.c). */
#define NETWORK_STREAM_PORT 5001

/**
  * @brief  Starts listening for incoming RTP/JPEG packets. Must be called
  *         after MX_LWIP_Init(). Non-blocking: registers an lwIP UDP
  *         callback and returns immediately.
  * @retval None
  */
void Network_Stream_Init(void);

#endif /* __NETWORK_STREAM_H */
