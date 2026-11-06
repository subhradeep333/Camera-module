#!/usr/bin/env python3
"""
Simple desktop viewer for testing the ESP32 + OV7670 camera feed.
Usage:
    ./.venv/bin/python test_camera.py --ip 192.168.1.50
"""

import sys
import time
import argparse
import requests
import numpy as np
import cv2

def main():
    parser = argparse.ArgumentParser(description="OV7670 Live Feed Viewer")
    parser.add_argument("--ip", type=str, default="192.168.4.1", help="ESP32 IP address (default 192.168.4.1)")
    args = parser.parse_args()

    clean_ip = args.ip.replace("http://", "").rstrip("/")
    url = f"http://{clean_ip}/capture"

    print(f"\n[+] Connecting to OV7670 camera stream at {url}...")
    print("[+] Press 'q' or 'ESC' in the window to quit.\n")

    fps_count = 0
    fps_start = time.time()
    current_fps = 0.0

    while True:
        try:
            t0 = time.time()
            resp = requests.get(url, timeout=2.0)
            if resp.status_code == 200:
                img_bytes = np.frombuffer(resp.content, dtype=np.uint8)
                frame = cv2.imdecode(img_bytes, cv2.IMREAD_GRAYSCALE)
                
                if frame is not None:
                    # FPS calculation
                    fps_count += 1
                    if time.time() - fps_start >= 1.0:
                        current_fps = fps_count / (time.time() - fps_start)
                        fps_count = 0
                        fps_start = time.time()

                    # Scale up 3x for clear viewing on Mac display (160x120 -> 480x360)
                    display = cv2.resize(frame, (480, 360), interpolation=cv2.INTER_NEAREST)
                    color_disp = cv2.cvtColor(display, cv2.COLOR_GRAY2BGR)

                    # Overlay FPS
                    cv2.putText(
                        color_disp,
                        f"OV7670 Live | FPS: {current_fps:.1f}",
                        (15, 30),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.7,
                        (0, 255, 120),
                        2,
                        cv2.LINE_AA
                    )

                    cv2.imshow("OV7670 Camera Test", color_disp)
            else:
                print(f"[!] Server returned HTTP {resp.status_code}")

        except requests.exceptions.RequestException as e:
            print(f"[!] Connection error: {e.__class__.__name__} (retrying...)")
            time.sleep(0.5)

        # Check for exit key
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q') or key == 27:
            break

    cv2.destroyAllWindows()
    print("[+] Viewer closed.")

if __name__ == "__main__":
    main()

