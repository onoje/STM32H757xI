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

# Upper bound, not a target - frames are scaled down to fit inside this box
# (aspect ratio kept, so no squishing) but never forced to exactly this size.
# The board centers whatever size actually arrives, padding with white.
MAX_WIDTH = 800
MAX_HEIGHT = 480

# Initial capture request - the camera may ignore this and give something
# else entirely; fit_size() below is what actually determines what gets sent.
CAPTURE_WIDTH = 800
CAPTURE_HEIGHT = 480

JPEG_QUALITY = 80  # 0-100, higher = better quality but bigger frames


def fit_size(width, height):
    """Largest (w, h) that preserves the width:height ratio and fits within
    MAX_WIDTH x MAX_HEIGHT."""
    scale = min(MAX_WIDTH / width, MAX_HEIGHT / height)
    fitted_w = min(MAX_WIDTH, max(1, round(width * scale)))
    fitted_h = min(MAX_HEIGHT, max(1, round(height * scale)))
    return fitted_w, fitted_h


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
    target_w, target_h = fit_size(cam_w, cam_h)
    print(f"Camera actually gives {cam_w}x{cam_h} frames "
          f"(requested {CAPTURE_WIDTH}x{CAPTURE_HEIGHT}) - "
          f"sending {target_w}x{target_h} (aspect ratio kept, fits within "
          f"{MAX_WIDTH}x{MAX_HEIGHT}).")

    sock = connect()

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("Frame capture failed")
                continue

            # Scale to fit inside the LCD's bounds without squishing - the
            # board centers whatever size arrives and pads the rest white.
            frame = cv2.resize(frame, (target_w, target_h))

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

            cv2.imshow("Sending to board (press q to quit)", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()
        sock.close()


if __name__ == "__main__":
    main()
