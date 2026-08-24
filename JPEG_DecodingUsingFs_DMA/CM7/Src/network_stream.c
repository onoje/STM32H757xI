/**
  ******************************************************************************
  * @file    network_stream.c
  * @brief   Raw-TCP JPEG frame receiver. A PC-side script connects to this
  *          board (NETWORK_STREAM_PORT) and pushes one JPEG frame after
  *          another, each preceded by a 4-byte big-endian length. Completed
  *          frames are handed to the existing decode/display pipeline in
  *          main.c through JPEG_Pipeline_OnFrameReceived().
  *
  *          Buffer ownership: ImageRawAddr[NB_IMAGES] is shared with main.c.
  *          A slot is safe for this file to write into exactly when
  *          FrameReady[slot] == 0 - main.c sets it back to 0 once the
  *          pipeline is done reading that slot (see DMA2D_XferCpltCallback).
  *          If no slot is free when a new frame starts, that frame is read
  *          and discarded (live view: skip, don't queue stale frames).
  ******************************************************************************
  */

#include "network_stream.h"
#include "main.h"
#include "lwip/tcp.h"

/* Owned by main.c - the raw JPEG buffers, how many bytes are currently
   valid in each, and whether each is ready/in-use for the pipeline. */
extern const uint32_t ImageRawAddr[NB_IMAGES];
extern volatile uint32_t ImageRawSize[NB_IMAGES];
extern volatile uint8_t FrameReady[NB_IMAGES];

/* Implemented in main.c: kicks the decode pipeline if it was idle waiting
   specifically on this slot. */
extern void JPEG_Pipeline_OnFrameReceived(uint32_t idx);

/* Frame parser state - one connection, one frame in flight at a time */
static uint8_t  HeaderBuf[4];
static uint32_t HeaderBytesGot;
static uint32_t ExpectedLen;
static uint32_t PayloadBytesGot;
static uint32_t CurrentWriteIdx;   /* NB_IMAGES == "no slot acquired yet" */
static uint8_t  DropCurrentFrame;  /* no free slot: read but don't store */

static void Network_ResetParser(void)
{
  HeaderBytesGot   = 0;
  PayloadBytesGot  = 0;
  ExpectedLen      = 0;
  CurrentWriteIdx  = NB_IMAGES;
  DropCurrentFrame = 0;
}

static err_t Network_TcpRecvCallback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  struct pbuf *q;
  uint8_t *data;
  uint16_t i;

  (void)arg;

  if (p == NULL)
  {
    /* Remote side closed the connection */
    tcp_close(tpcb);
    Network_ResetParser();
    return ERR_OK;
  }

  if (err != ERR_OK)
  {
    pbuf_free(p);
    return err;
  }

  for (q = p; q != NULL; q = q->next)
  {
    data = (uint8_t *)q->payload;

    for (i = 0; i < q->len; i++)
    {
      if (HeaderBytesGot < 4)
      {
        /* Collecting the 4-byte big-endian frame length */
        HeaderBuf[HeaderBytesGot++] = data[i];

        if (HeaderBytesGot == 4)
        {
          uint32_t k;

          ExpectedLen = ((uint32_t)HeaderBuf[0] << 24) | ((uint32_t)HeaderBuf[1] << 16) |
                        ((uint32_t)HeaderBuf[2] << 8)  |  (uint32_t)HeaderBuf[3];
          PayloadBytesGot = 0;

          if (ExpectedLen == 0 || ExpectedLen > JPEG_RAW_BUFFER_MAX_SIZE)
          {
            /* Malformed length - the byte stream can no longer be trusted
               to be in sync, so drop the connection rather than guess */
            tcp_abort(tpcb);
            pbuf_free(p);
            Network_ResetParser();
            return ERR_ABRT;
          }

          CurrentWriteIdx = NB_IMAGES;
          for (k = 0; k < NB_IMAGES; k++)
          {
            if (FrameReady[k] == 0)
            {
              CurrentWriteIdx = k;
              break;
            }
          }
          DropCurrentFrame = (CurrentWriteIdx == NB_IMAGES) ? 1 : 0;
        }
      }
      else
      {
        /* Collecting the JPEG payload itself */
        if (!DropCurrentFrame)
        {
          ((uint8_t *)ImageRawAddr[CurrentWriteIdx])[PayloadBytesGot] = data[i];
        }
        PayloadBytesGot++;

        if (PayloadBytesGot == ExpectedLen)
        {
          if (!DropCurrentFrame)
          {
            ImageRawSize[CurrentWriteIdx] = PayloadBytesGot;
            FrameReady[CurrentWriteIdx] = 1;
            JPEG_Pipeline_OnFrameReceived(CurrentWriteIdx);
          }

          /* Ready for the next frame's 4-byte header */
          HeaderBytesGot   = 0;
          CurrentWriteIdx  = NB_IMAGES;
          DropCurrentFrame = 0;
        }
      }
    }
  }

  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);

  return ERR_OK;
}

static err_t Network_TcpAcceptCallback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  (void)arg;
  (void)err;

  tcp_setprio(newpcb, TCP_PRIO_MIN);
  tcp_arg(newpcb, NULL);
  tcp_recv(newpcb, Network_TcpRecvCallback);

  Network_ResetParser();

  return ERR_OK;
}

void Network_Stream_Init(void)
{
  struct tcp_pcb *pcb;

  Network_ResetParser();

  pcb = tcp_new();
  tcp_bind(pcb, IP_ADDR_ANY, NETWORK_STREAM_PORT);
  pcb = tcp_listen(pcb);
  tcp_accept(pcb, Network_TcpAcceptCallback);
}
