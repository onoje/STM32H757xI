/**
  ******************************************************************************
  * @file    network_stream.c
  * @brief   RTP/JPEG (RFC 2435) receiver over raw UDP. A PC-side script
  *          sends one JPEG frame per RTP timestamp, split across as many UDP
  *          packets as needed (RFC 2435 fragmentation). Neither quantization
  *          nor Huffman tables are transmitted - see the JFIF header builder
  *          below for why that's standards-compliant, not a shortcut.
  *          Completed frames are handed to the existing decode/display
  *          pipeline in main.c through JPEG_Pipeline_OnFrameReceived(),
  *          exactly as the previous raw-TCP version did.
  *
  *          Buffer ownership is unchanged from the TCP version:
  *          ImageRawAddr[NB_IMAGES] is shared with main.c, and a slot is
  *          safe to write into exactly when FrameReady[slot] == 0.
  ******************************************************************************
  */

#include "network_stream.h"
#include "main.h"
#include "lwip/udp.h"
#include <string.h>

/* Owned by main.c - the raw JPEG buffers, how many bytes are currently
   valid in each, and whether each is ready/in-use for the pipeline. */
extern const uint32_t ImageRawAddr[NB_IMAGES];
extern volatile uint32_t ImageRawSize[NB_IMAGES];
extern volatile uint8_t FrameReady[NB_IMAGES];
extern volatile uint32_t FrameSeqNum[NB_IMAGES];

/* Implemented in main.c: kicks the decode pipeline if it was idle waiting
   specifically on this slot. */
extern void JPEG_Pipeline_OnFrameReceived(uint32_t idx);

/* ---------------------------------------------------------------------
   RFC 2435 JFIF header reconstruction. Neither quantization nor Huffman
   tables travel over the network - both are the fixed values defined by
   the JPEG standard itself (ITU-T T.81 Annex K), reproduced here exactly
   as RFC 2435's own reference implementation does:

   - Quantization tables: RFC 2435 lets a Q byte of 1-99 stand in for the
     tables. The board derives the same tables from Q that the sender's
     libjpeg-turbo encoder derives internally, using the identical IJG
     scaling formula - both sides land on the same numbers without ever
     exchanging them.
   - Huffman tables: the sender encodes without IMWRITE_JPEG_OPTIMIZE, so
     libjpeg-turbo already uses the fixed JPEG-standard default tables -
     the same ones reproduced below.

   Kept in this file (rather than a separate translation unit) because
   this project links CM7 source files individually via .project <link>
   entries, and a second new .c file has repeatedly failed to get picked
   up by CubeIDE's build even after Clean + Refresh.
   --------------------------------------------------------------------- */

/* Base quantization tables, already in zigzag order (the order a JPEG DQT
   segment stores them in) - RFC 2435 Appendix, "jpeg_luma_quantizer" /
   "jpeg_chroma_quantizer". */
static const uint8_t jpeg_luma_quantizer[64] =
{
  16, 11, 12, 14, 12, 10, 16, 14,
  13, 14, 18, 17, 16, 19, 24, 40,
  26, 24, 22, 22, 24, 49, 35, 37,
  29, 40, 58, 51, 61, 60, 57, 51,
  56, 55, 64, 72, 92, 78, 64, 68,
  87, 69, 55, 56, 80, 109, 81, 87,
  95, 98, 103, 104, 103, 62, 77, 113,
  121, 112, 100, 120, 92, 101, 103, 99
};

static const uint8_t jpeg_chroma_quantizer[64] =
{
  17, 18, 18, 24, 21, 24, 47, 26,
  26, 47, 99, 66, 56, 66, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99
};

/* Standard Huffman tables, ITU-T T.81 Annex K.3 (Tables K.3-K.6) */
static const uint8_t dc_luma_bits[16] =
{ 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t dc_luma_values[12] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static const uint8_t dc_chroma_bits[16] =
{ 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const uint8_t dc_chroma_values[12] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static const uint8_t ac_luma_bits[16] =
{ 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const uint8_t ac_luma_values[162] =
{
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

static const uint8_t ac_chroma_bits[16] =
{ 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const uint8_t ac_chroma_values[162] =
{
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

/* Scales a base quantization table by the JPEG quality factor q, using the
   same formula the IJG/libjpeg reference encoder uses (RFC 2435 Appendix,
   MakeTables()). */
static void MakeQuantTable(const uint8_t *base, uint8_t q, uint8_t *out)
{
  uint32_t factor = (q < 1U) ? 1U : ((q > 99U) ? 99U : q);
  uint32_t scale = (factor < 50U) ? (5000U / factor) : (200U - factor * 2U);
  uint32_t i;

  for (i = 0; i < 64U; i++)
  {
    uint32_t v = ((uint32_t)base[i] * scale + 50U) / 100U;
    if (v < 1U)   { v = 1U; }
    if (v > 255U) { v = 255U; }
    out[i] = (uint8_t)v;
  }
}

static uint32_t WriteDQT(uint8_t *dst, uint8_t tableId, const uint8_t *table)
{
  dst[0] = 0xFF; dst[1] = 0xDB;
  dst[2] = 0x00; dst[3] = 67;   /* length: 2 (itself) + 1 (Pq/Tq) + 64 (table) */
  dst[4] = tableId;              /* Pq=0 (8-bit precision), Tq=tableId */
  memcpy(&dst[5], table, 64);
  return 69;                     /* 2 (marker) + 67 (length field value) */
}

static uint32_t WriteDHT(uint8_t *dst, uint8_t classAndId,
                          const uint8_t *bits, const uint8_t *values, uint32_t numValues)
{
  uint32_t len = 2U + 1U + 16U + numValues; /* length field value */

  dst[0] = 0xFF; dst[1] = 0xC4;
  dst[2] = (uint8_t)(len >> 8);
  dst[3] = (uint8_t)(len & 0xFFU);
  dst[4] = classAndId;
  memcpy(&dst[5], bits, 16);
  memcpy(&dst[21], values, numValues);
  return 2U + len;                /* 2 (marker) + length field value */
}

/**
  * @brief  Writes a complete JFIF header into dst: SOI, DQT (luma+chroma,
  *         scaled from q), SOF0 (width/height/subsampling), DHT (the four
  *         standard Huffman tables), SOS. The caller appends the raw JPEG
  *         entropy-coded scan data (as received over RTP) right after the
  *         returned length, then an EOI marker.
  * @param  dst: destination buffer, needs ~610 bytes of room
  * @param  width: image width in pixels
  * @param  height: image height in pixels
  * @param  type: RFC 2435 Type byte (0 = 4:2:2, 1 = 4:2:0)
  * @param  q: RFC 2435 Q byte (1-99, JPEG quality factor)
  * @retval number of bytes written to dst
  */
static uint32_t JFIF_BuildHeader(uint8_t *dst, uint16_t width, uint16_t height, uint8_t type, uint8_t q)
{
  uint8_t lqt[64], cqt[64];
  uint32_t pos = 0;

  MakeQuantTable(jpeg_luma_quantizer, q, lqt);
  MakeQuantTable(jpeg_chroma_quantizer, q, cqt);

  /* SOI */
  dst[pos++] = 0xFF; dst[pos++] = 0xD8;

  /* DQT: luma = table 0, chroma = table 1 */
  pos += WriteDQT(&dst[pos], 0, lqt);
  pos += WriteDQT(&dst[pos], 1, cqt);

  /* SOF0 - baseline DCT, 3 components (Y, Cb, Cr) */
  {
    uint8_t ySamp = (type == 0U) ? 0x21U : 0x22U; /* 0=4:2:2 (H2V1), 1=4:2:0 (H2V2) */

    dst[pos++] = 0xFF; dst[pos++] = 0xC0;
    dst[pos++] = 0x00; dst[pos++] = 17;
    dst[pos++] = 8;                          /* sample precision */
    dst[pos++] = (uint8_t)(height >> 8);
    dst[pos++] = (uint8_t)(height & 0xFFU);
    dst[pos++] = (uint8_t)(width >> 8);
    dst[pos++] = (uint8_t)(width & 0xFFU);
    dst[pos++] = 3;                          /* number of components */

    dst[pos++] = 1; dst[pos++] = ySamp; dst[pos++] = 0; /* Y  - quant table 0 */
    dst[pos++] = 2; dst[pos++] = 0x11;  dst[pos++] = 1; /* Cb - quant table 1 */
    dst[pos++] = 3; dst[pos++] = 0x11;  dst[pos++] = 1; /* Cr - quant table 1 */
  }

  /* DHT: DC/AC, luma/chroma - the four fixed standard tables */
  pos += WriteDHT(&dst[pos], 0x00, dc_luma_bits, dc_luma_values, 12);
  pos += WriteDHT(&dst[pos], 0x10, ac_luma_bits, ac_luma_values, 162);
  pos += WriteDHT(&dst[pos], 0x01, dc_chroma_bits, dc_chroma_values, 12);
  pos += WriteDHT(&dst[pos], 0x11, ac_chroma_bits, ac_chroma_values, 162);

  /* SOS - scan header, entropy-coded data follows immediately after */
  dst[pos++] = 0xFF; dst[pos++] = 0xDA;
  dst[pos++] = 0x00; dst[pos++] = 12;
  dst[pos++] = 3;
  dst[pos++] = 1; dst[pos++] = 0x00; /* Y:  DC table 0, AC table 0 */
  dst[pos++] = 2; dst[pos++] = 0x11; /* Cb: DC table 1, AC table 1 */
  dst[pos++] = 3; dst[pos++] = 0x11; /* Cr: DC table 1, AC table 1 */
  dst[pos++] = 0;                    /* Ss */
  dst[pos++] = 63;                   /* Se */
  dst[pos++] = 0;                    /* Ah/Al */

  return pos;
}

/* ---------------------------------------------------------------------
   RTP/JPEG packet reception and reassembly
   --------------------------------------------------------------------- */

/* RTP fixed header is 12 bytes (RFC 3550), the JPEG payload header that
   follows it is 8 bytes (RFC 2435 section 3.1) - no Quantization Table
   header is ever present (see JFIF_BuildHeader() above). */
#define RTP_HEADER_LEN  12U
#define JPEG_HEADER_LEN 8U
#define MIN_PACKET_LEN  (RTP_HEADER_LEN + JPEG_HEADER_LEN)

/* Reassembly state for the one frame currently being received */
static uint32_t CurrentWriteIdx;    /* NB_IMAGES == "no slot acquired yet" */
static uint32_t HeaderLen;          /* JFIF header length for this frame */
static uint32_t ExpectedFragOffset; /* next Fragment Offset we should see */
static uint8_t  FrameCorrupt;       /* a fragment was lost - drop the rest */

/* Monotonic completion counter - see FrameSeqNum[] in main.c. Starts at 1
   so slot 0's default FrameSeqNum[]=0 never looks like "the newest frame"
   before any real frame has ever completed. */
static uint32_t NextFrameSeqNum = 1;

static void Network_ResetParser(void)
{
  CurrentWriteIdx     = NB_IMAGES;
  HeaderLen           = 0;
  ExpectedFragOffset  = 0;
  FrameCorrupt        = 1;
}

static void Network_UdpRecvCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                                     const ip_addr_t *addr, u16_t port)
{
  uint8_t  *data;
  uint16_t len;
  uint8_t  marker;
  uint32_t fragOffset, type, q;
  uint16_t width, height;
  const uint8_t *payload;
  uint16_t payloadLen;

  (void)arg;
  (void)pcb;
  (void)addr;
  (void)port;

  if (p == NULL)
  {
    return;
  }

  /* Every RTP packet we ever send fits in one Ethernet frame (see
     MAX_FRAGMENT_PAYLOAD on the PC side), so it always arrives as a single,
     unchained pbuf - p->next is not expected here. */
  data = (uint8_t *)p->payload;
  len  = p->len;

  if (len < MIN_PACKET_LEN)
  {
    pbuf_free(p);
    return;
  }

  /* --- RTP fixed header (RFC 3550) --- */
  marker = (data[1] & 0x80U) ? 1U : 0U;
  /* Sequence number/timestamp/SSRC aren't needed for correctness: UDP
     delivers each of our datagrams whole or not at all, so Fragment Offset
     (below) is what actually tells us if something in the middle of a
     frame went missing. */

  /* --- RFC 2435 JPEG payload header --- */
  fragOffset = ((uint32_t)data[13] << 16) | ((uint32_t)data[14] << 8) | data[15];
  type       = data[16];
  q          = data[17];
  width      = (uint16_t)data[18] * 8U;
  height     = (uint16_t)data[19] * 8U;

  payload    = &data[MIN_PACKET_LEN];
  payloadLen = (uint16_t)(len - MIN_PACKET_LEN);

  if (fragOffset == 0U)
  {
    /* First fragment of a new frame - grab a free buffer slot the same way
       the TCP version did, and lay down a fresh JFIF header for it. */
    uint32_t k;

    CurrentWriteIdx = NB_IMAGES;
    for (k = 0; k < NB_IMAGES; k++)
    {
      if (FrameReady[k] == 0U)
      {
        CurrentWriteIdx = k;
        break;
      }
    }

    if (CurrentWriteIdx == NB_IMAGES)
    {
      /* No free slot - the pipeline is still busy with older frames. Drop
         this one (live view: skip, don't queue stale frames). */
      FrameCorrupt = 1U;
    }
    else
    {
      HeaderLen    = JFIF_BuildHeader((uint8_t *)ImageRawAddr[CurrentWriteIdx],
                                       width, height, (uint8_t)type, (uint8_t)q);
      FrameCorrupt = 0U;
    }

    ExpectedFragOffset = 0U;
  }

  if ((FrameCorrupt == 0U) && (CurrentWriteIdx != NB_IMAGES))
  {
    if (fragOffset != ExpectedFragOffset)
    {
      /* A fragment was lost somewhere before this one - the scan data is
         no longer contiguous, so the whole frame is unusable. Wait for the
         next frame (next Fragment Offset == 0). */
      FrameCorrupt = 1U;
    }
    else
    {
      uint32_t writeOffset = HeaderLen + fragOffset;

      /* Leave 2 bytes of headroom for the EOI marker appended below */
      if ((writeOffset + payloadLen) <= (JPEG_RAW_BUFFER_MAX_SIZE - 2U))
      {
        memcpy((uint8_t *)ImageRawAddr[CurrentWriteIdx] + writeOffset, payload, payloadLen);
        ExpectedFragOffset = fragOffset + payloadLen;
      }
      else
      {
        FrameCorrupt = 1U; /* would overflow the raw buffer */
      }
    }
  }

  if (marker != 0U)
  {
    if ((FrameCorrupt == 0U) && (CurrentWriteIdx != NB_IMAGES))
    {
      uint32_t endPos = HeaderLen + ExpectedFragOffset;

      ((uint8_t *)ImageRawAddr[CurrentWriteIdx])[endPos]      = 0xFFU;
      ((uint8_t *)ImageRawAddr[CurrentWriteIdx])[endPos + 1U] = 0xD9U; /* EOI */

      ImageRawSize[CurrentWriteIdx] = endPos + 2U;
      FrameSeqNum[CurrentWriteIdx] = NextFrameSeqNum++;
      FrameReady[CurrentWriteIdx]   = 1U;
      JPEG_Pipeline_OnFrameReceived(CurrentWriteIdx);
    }

    /* Marker bit means "last packet of this frame" either way - ready for
       the next frame's Fragment Offset == 0, whether this one made it or
       was dropped as corrupt. */
    Network_ResetParser();
  }

  pbuf_free(p);
}

void Network_Stream_Init(void)
{
  struct udp_pcb *pcb;

  Network_ResetParser();

  pcb = udp_new();
  udp_bind(pcb, IP_ADDR_ANY, NETWORK_STREAM_PORT);
  udp_recv(pcb, Network_UdpRecvCallback, NULL);
}
