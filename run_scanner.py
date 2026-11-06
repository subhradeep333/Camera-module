#!/usr/bin/env python3
"""
Root entry point for the ESP32 + OV7670 Camera Monitor & Spatial Detector.
"""

import sys
import os
import argparse

current_dir = os.path.dirname(os.path.abspath(__file__))
host_dir = os.path.join(current_dir, "host_software")
if host_dir not in sys.path:
    sys.path.insert(0, host_dir)

def launch_gui():
    import tkinter as tk
    from gui import ScannerGUI
    root = tk.Tk()
    app = ScannerGUI(root)
    root.mainloop()

def launch_cli(ip: str):
    import cv2
    from config import DEFAULT_CONFIG
    from scanner_client import ScannerClient
    from mapping_engine import MappingEngine

    cfg = DEFAULT_CONFIG
    cfg.esp32_url = f"http://{ip.replace('http://', '').rstrip('/')}"
    client = ScannerClient(cfg)
    engine = MappingEngine(cfg)

    print(f"\n[CLI] Connecting to ESP32 Camera at {cfg.esp32_url}...")
    ok, msg = client.check_connection()
    if not ok:
        print(f"[ERROR] Could not connect: {msg}")
        return

    print(f"[OK] {msg}")
    print("[CLI] Capturing frame...")
    frame = client.capture_frame()
    if frame is None:
        print("[!] Capture failed.")
        return

    analyzed, dist = engine.analyze_frame_obstacles(frame)
    radar = engine.generate_fov_radar_map(dist)
    print(f"[OK] Detected Obstacle Distance: {dist:.2f} meters")

    os.makedirs(cfg.output_dir, exist_ok=True)
    frame_path = os.path.join(cfg.output_dir, "cli_frame.png")
    radar_path = os.path.join(cfg.output_dir, "cli_radar.png")

    cv2.imwrite(frame_path, analyzed)
    cv2.imwrite(radar_path, radar)
    print(f"[OK] Saved camera frame: {frame_path}")
    print(f"[OK] Saved spatial radar: {radar_path}")

def main():
    parser = argparse.ArgumentParser(description="ESP32 OV7670 Camera Monitor & Spatial Detector")
    parser.add_argument("--cli", action="store_true", help="Capture single frame via terminal without GUI")
    parser.add_argument("--ip", type=str, default="192.168.1.50", help="ESP32 IP address (e.g. 192.168.1.50)")
    args = parser.parse_args()

    if args.cli:
        launch_cli(args.ip)
    else:
        launch_gui()

if __name__ == "__main__":
    main()
