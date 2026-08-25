"""
send_webcam_stream.py

Captures frames from the PC's webcam and streams them to the
STM32H747I-EVAL board as real RTP (RFC 3550) carrying RFC 2435 "RTP Payload
Format for JPEG-compressed Video" - the same wire format used by ffmpeg,
gstreamer, and IP cameras, not a custom protocol.

Neither quantization nor Huffman tables are sent in the packets. Both are
skipped legitimately, per RFC 2435 itself:

  - Quantization tables: RFC 2435 lets a Q byte of 1-99 stand in for the
    tables. Both this sender and the board derive the exact same tables
    from Q using the standard IJG/libjpeg scaling formula (see
    CM7/Src/network_stream.c on the board side).
  - Huffman tables: OpenCV's JPEG encoder (libjpeg-turbo), used without the
    IMWRITE_JPEG_OPTIMIZE flag, already encodes with the fixed JPEG-standard
    default Huffman tables (ITU-T T.81 Annex K) - the same ones the board
    reconstructs on its end. Never set IMWRITE_JPEG_OPTIMIZE=1 or this
    breaks.

Requirements:
    pip install opencv-python

Usage:
    python send_webcam_stream.py
    (edit BOARD_IP below if the board's static IP is different)
"""

import cv2
import random
import socket
import struct
import time

BOARD_IP = "192.168.1.20"
BOARD_PORT = 5001

# The exact size sent to the board - it doesn't scale anything itself, so
# every frame must already be precisely this size.
TARGET_WIDTH = 800
TARGET_HEIGHT = 480

# Initial capture request - the camera may ignore this and give something
# else entirely; compute_crop() below is what actually determines what gets
# sent, based on whatever size the camera actually produces.
CAPTURE_WIDTH = 800
CAPTURE_HEIGHT = 480

# RFC 2435 Q byte: must stay 1-99 so both ends derive quantization tables
# from this number instead of transmitting them. Also the actual JPEG
# encode quality, must stay identical to the Q byte for both sides to
# agree on what was used.
JPEG_QUALITY = 80

# RFC 2435 Type byte: 0 = 4:2:2, 1 = 4:2:0. libjpeg-turbo's default for a
# 3-channel color image is 4:2:0 - this must match what the encoder
# actually produced or the board decodes the chroma planes wrong.
JPEG_TYPE = 1

RTP_PAYLOAD_TYPE = 26           # RFC 3551 static payload type for JPEG
RTP_CLOCK_HZ = 90000            # RFC 2435 mandates a 90kHz RTP clock
MAX_FRAGMENT_PAYLOAD = 1400     # stays under the 1500-byte Ethernet MTU

WINDOW_NAME = "Sending to board (press q or close the window to quit)"


def compute_crop(width, height):
    """Region to cut out of a width x height source frame so its aspect
    ratio matches TARGET_WIDTH:TARGET_HEIGHT exactly (centered), before
    resizing to that exact size. Fills the whole screen with no squishing
    and no letterboxing - the tradeoff is that whatever doesn't fit the
    target aspect ratio (extra top/bottom or left/right) gets cut off."""
    target_ratio = TARGET_WIDTH / TARGET_HEIGHT
    src_ratio = width / height

    if src_ratio > target_ratio:
        # Source is relatively wider than target - crop the sides
        crop_w = round(height * target_ratio)
        crop_h = height
    else:
        # Source is relatively taller than target - crop top/bottom
        crop_w = width
        crop_h = round(width / target_ratio)

    x0 = (width - crop_w) // 2
    y0 = (height - crop_h) // 2
    return x0, y0, crop_w, crop_h


def find_scan_data(jpeg_bytes):
    """Strips a JPEG down to just its entropy-coded scan data - the only
    part RFC 2435 actually transmits. Everything else (DQT/SOF/DHT/the SOS
    header itself) is reconstructed on the board from the Type/Q/Width/
    Height fields instead.

    Walks marker segments one at a time (using each segment's own length
    field to skip it) rather than searching for the raw byte pattern
    FF DA - unlike entropy-coded scan data, header segment payloads (DQT
    tables, Huffman tables, ...) are ordinary bytes with no byte-stuffing
    rule protecting them, so FF DA can appear there by pure coincidence
    and a naive search would cut the frame at the wrong point."""
    if jpeg_bytes[0:2] != b"\xff\xd8":
        return None

    pos = 2
    n = len(jpeg_bytes)

    while pos < n - 1:
        if jpeg_bytes[pos] != 0xFF:
            return None  # desynced - not a marker where one was expected

        marker = jpeg_bytes[pos + 1]
        pos += 2

        if marker == 0xDA:  # SOS - scan data starts right after this header
            seg_len = (jpeg_bytes[pos] << 8) | jpeg_bytes[pos + 1]
            scan_start = pos + seg_len

            # A well-formed JPEG always ends with EOI (FF D9) as its last
            # 2 bytes.
            if jpeg_bytes[-2:] != b"\xff\xd9":
                return None

            return jpeg_bytes[scan_start:-2]

        if marker == 0x01 or (0xD0 <= marker <= 0xD7):
            continue  # markers with no length field/payload

        seg_len = (jpeg_bytes[pos] << 8) | jpeg_bytes[pos + 1]
        pos += seg_len

    return None


def build_rtp_header(seq, timestamp, ssrc, marker):
    b0 = 0x80  # version=2, padding=0, extension=0, CSRC count=0
    b1 = (0x80 if marker else 0x00) | RTP_PAYLOAD_TYPE
    return struct.pack(">BBHII", b0, b1, seq & 0xFFFF, timestamp & 0xFFFFFFFF, ssrc)


def build_jpeg_header(fragment_offset, width, height):
    return struct.pack(
        ">BBBBBBBB",
        0,                               # type-specific
        (fragment_offset >> 16) & 0xFF,
        (fragment_offset >> 8) & 0xFF,
        fragment_offset & 0xFF,
        JPEG_TYPE,
        JPEG_QUALITY,
        width // 8,
        height // 8,
    )


def send_frame_as_rtp(sock, scan_data, width, height, seq, timestamp, ssrc):
    total = len(scan_data)
    offset = 0

    while offset < total:
        chunk = scan_data[offset:offset + MAX_FRAGMENT_PAYLOAD]
        is_last = (offset + len(chunk)) >= total

        packet = (build_rtp_header(seq, timestamp, ssrc, is_last)
                  + build_jpeg_header(offset, width, height)
                  + chunk)
        sock.sendto(packet, (BOARD_IP, BOARD_PORT))

        seq = (seq + 1) & 0xFFFF
        offset += len(chunk)

    return seq


def main():
    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAPTURE_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAPTURE_HEIGHT)

    if not cap.isOpened():
        print("Could not open webcam")
        return

    ok, probe_frame = cap.read()
    if not ok:
        print("Could not read a frame from the webcam")
        return

    cam_h, cam_w = probe_frame.shape[:2]
    crop_x, crop_y, crop_w, crop_h = compute_crop(cam_w, cam_h)
    print(f"Camera actually gives {cam_w}x{cam_h} frames "
          f"(requested {CAPTURE_WIDTH}x{CAPTURE_HEIGHT}) - "
          f"cropping to {crop_w}x{crop_h} at ({crop_x},{crop_y}) then "
          f"resizing to {TARGET_WIDTH}x{TARGET_HEIGHT} (fills the screen, "
          f"no squishing).")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ssrc = random.getrandbits(32)
    seq = random.getrandbits(16)
    timestamp = random.getrandbits(32)
    last_frame_time = time.monotonic()

    print(f"Streaming RTP/JPEG to {BOARD_IP}:{BOARD_PORT} (SSRC={ssrc:#010x})")

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("Frame capture failed")
                continue

            # Webcams give the raw (unmirrored) feed, which looks backwards
            # to the person looking at it - flip horizontally so it reads
            # like a normal mirror/selfie preview instead.
            frame = cv2.flip(frame, 1)

            # Cut out the centered region that matches the LCD's aspect
            # ratio, then resize it to fill the screen exactly - no
            # letterboxing, no squishing.
            frame = frame[crop_y:crop_y + crop_h, crop_x:crop_x + crop_w]
            frame = cv2.resize(frame, (TARGET_WIDTH, TARGET_HEIGHT))

            ok, encoded = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, JPEG_QUALITY])
            if not ok:
                continue

            scan_data = find_scan_data(encoded.tobytes())
            if scan_data:
                now = time.monotonic()
                timestamp = (timestamp + round((now - last_frame_time) * RTP_CLOCK_HZ)) & 0xFFFFFFFF
                last_frame_time = now

                seq = send_frame_as_rtp(sock, scan_data, TARGET_WIDTH, TARGET_HEIGHT, seq, timestamp, ssrc)

            cv2.imshow(WINDOW_NAME, frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
            # getWindowProperty drops below 1 the moment the user closes the
            # window with its X button - waitKey alone doesn't catch that.
            if cv2.getWindowProperty(WINDOW_NAME, cv2.WND_PROP_VISIBLE) < 1:
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()
        sock.close()


if __name__ == "__main__":
    main()
