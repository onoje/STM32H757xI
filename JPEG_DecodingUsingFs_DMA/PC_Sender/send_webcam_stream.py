"""
send_webcam_stream.py

Captures frames from the PC's webcam and streams them, as JPEG images, over
a raw TCP socket to the STM32H747I-EVAL board running the
JPEG_DecodingUsingFs_DMA + Ethernet firmware.

Wire format: each frame is [4-byte big-endian length][that many JPEG bytes].
The board (see CM7/Src/network_stream.c) parses exactly this format.

Requirements:
    pip install opencv-python

Usage:
    python send_webcam_stream.py
    (edit BOARD_IP below if the board's static IP is different)
"""

import cv2
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

JPEG_QUALITY = 80  # 0-100, higher = better quality but bigger frames

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


def connect():
    while True:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((BOARD_IP, BOARD_PORT))
            print(f"Connected to board at {BOARD_IP}:{BOARD_PORT}")
            return s
        except OSError as e:
            print(f"Connection failed ({e}), retrying in 2s...")
            time.sleep(2)


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

    sock = connect()

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

            payload = encoded.tobytes()
            header = struct.pack(">I", len(payload))

            try:
                sock.sendall(header + payload)
            except OSError as e:
                print(f"Send failed ({e}), reconnecting...")
                sock.close()
                sock = connect()

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
