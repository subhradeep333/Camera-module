import os
import time
import json
import threading
from datetime import datetime
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk
import cv2
import numpy as np

try:
    from config import ScannerConfig, DEFAULT_CONFIG
    from scanner_client import ScannerClient
    from mapping_engine import MappingEngine
except ImportError:
    from host_software.config import ScannerConfig, DEFAULT_CONFIG
    from host_software.scanner_client import ScannerClient
    from host_software.mapping_engine import MappingEngine

class ScannerGUI:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ESP32 + OV7670 Camera Monitor & Spatial Detector")
        self.root.geometry("1060x720")
        self.root.minsize(920, 640)
        self.root.configure(bg="#141419")

        self.config = DEFAULT_CONFIG
        self.client = ScannerClient(self.config)
        self.engine = MappingEngine(self.config)

        # State
        self.is_streaming = False
        self.latest_frame = None
        self.latest_analyzed = None
        self.latest_radar_img = None
        self.latest_dist = 0.0

        self._setup_style()
        self._build_ui()

    def _setup_style(self):
        self.style = ttk.Style()
        self.style.theme_use("clam")

        bg_dark = "#141419"
        card_bg = "#1e1e26"
        accent_blue = "#00a8ff"
        text_light = "#f0f0f5"

        self.style.configure(".", background=bg_dark, foreground=text_light, font=("Helvetica", 10))
        self.style.configure("Card.TFrame", background=card_bg, relief="flat")
        self.style.configure("Header.TLabel", font=("Helvetica", 12, "bold"), foreground=accent_blue, background=card_bg)
        self.style.configure("Status.TLabel", font=("Helvetica", 9), foreground="#a0a0b0", background=card_bg)
        
        self.style.configure("Primary.TButton", font=("Helvetica", 10, "bold"), padding=6)
        self.style.map("Primary.TButton",
            background=[("active", "#0088cc"), ("!disabled", "#00a8ff")],
            foreground=[("!disabled", "#ffffff")]
        )
        self.style.configure("Danger.TButton", font=("Helvetica", 10, "bold"), padding=6)
        self.style.map("Danger.TButton",
            background=[("active", "#cc2244"), ("!disabled", "#ff3366")],
            foreground=[("!disabled", "#ffffff")]
        )

    def _build_ui(self):
        # Title bar
        title_frame = tk.Frame(self.root, bg="#1e1e26", height=48)
        title_frame.pack(fill=tk.X, side=tk.TOP, padx=10, pady=(10, 5))
        
        tk.Label(
            title_frame,
            text="📷 ESP32 OV7670 Camera Monitor & Spatial Detector",
            font=("Helvetica", 14, "bold"),
            fg="#00e5ff",
            bg="#1e1e26"
        ).pack(side=tk.LEFT, padx=15, pady=8)

        self.lbl_conn_status = tk.Label(
            title_frame,
            text="● Disconnected",
            font=("Helvetica", 10, "bold"),
            fg="#ff4466",
            bg="#1e1e26"
        )
        self.lbl_conn_status.pack(side=tk.RIGHT, padx=15)

        # Content frame
        content_frame = tk.Frame(self.root, bg="#141419")
        content_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # Left Sidebar (Controls)
        sidebar = ttk.Frame(content_frame, style="Card.TFrame", width=280)
        sidebar.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 8), pady=0)
        sidebar.pack_propagate(False)

        # Right Panel (Views)
        main_panel = ttk.Frame(content_frame, style="Card.TFrame")
        main_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=0, pady=0)

        self._build_sidebar(sidebar)
        self._build_views(main_panel)

    def _build_sidebar(self, parent):
        pad_x, pad_y = 12, 8

        # 1. Connection
        ttk.Label(parent, text="ESP32 Connection", style="Header.TLabel").pack(anchor=tk.W, padx=pad_x, pady=(12, 4))
        
        conn_box = tk.Frame(parent, bg="#1e1e26")
        conn_box.pack(fill=tk.X, padx=pad_x, pady=(0, pad_y))
        
        self.entry_url = tk.Entry(conn_box, bg="#2a2a38", fg="white", insertbackground="white", font=("Courier", 10))
        self.entry_url.insert(0, self.config.esp32_url)
        self.entry_url.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6))

        btn_check = ttk.Button(conn_box, text="Connect", command=self.on_check_connection)
        btn_check.pack(side=tk.RIGHT)

        ttk.Separator(parent).pack(fill=tk.X, padx=pad_x, pady=10)

        # 2. Camera Controls
        ttk.Label(parent, text="Camera Operations", style="Header.TLabel").pack(anchor=tk.W, padx=pad_x, pady=(4, 6))

        self.btn_stream = ttk.Button(parent, text="▶ Start Live Stream", style="Primary.TButton", command=self.on_toggle_stream)
        self.btn_stream.pack(fill=tk.X, padx=pad_x, pady=4)

        btn_snap = ttk.Button(parent, text="📸 Capture Snapshot", command=self.on_snapshot)
        btn_snap.pack(fill=tk.X, padx=pad_x, pady=4)

        ttk.Separator(parent).pack(fill=tk.X, padx=pad_x, pady=10)

        # 3. Telemetry Info
        ttk.Label(parent, text="Live Telemetry", style="Header.TLabel").pack(anchor=tk.W, padx=pad_x, pady=(4, 6))

        info_box = tk.Frame(parent, bg="#181820", padx=10, pady=8)
        info_box.pack(fill=tk.X, padx=pad_x, pady=4)

        self.lbl_fps = tk.Label(info_box, text="Stream FPS: --", bg="#181820", fg="#88ddff", font=("Helvetica", 10))
        self.lbl_fps.pack(anchor=tk.W, pady=2)

        self.lbl_dist = tk.Label(info_box, text="Obstacle Distance: --", bg="#181820", fg="#ffaa00", font=("Helvetica", 10, "bold"))
        self.lbl_dist.pack(anchor=tk.W, pady=2)

        self.lbl_status = ttk.Label(parent, text="Status: Ready", style="Status.TLabel")
        self.lbl_status.pack(anchor=tk.W, padx=pad_x, pady=6)

        # Export Button at bottom
        self.btn_export = ttk.Button(parent, text="💾 Save Frame & Analysis", command=self.on_export)
        self.btn_export.pack(side=tk.BOTTOM, fill=tk.X, padx=pad_x, pady=(0, 14))

    def _build_views(self, parent):
        parent.rowconfigure(0, weight=1)
        parent.columnconfigure(0, weight=1)
        parent.columnconfigure(1, weight=1)

        # Left: Camera Feed Card
        cam_card = tk.Frame(parent, bg="#1a1a22", bd=1, relief="ridge")
        cam_card.grid(row=0, column=0, sticky="nsew", padx=8, pady=8)

        tk.Label(
            cam_card,
            text="LIVE CAMERA FEED + OBSTACLE OVERLAY",
            font=("Helvetica", 10, "bold"),
            fg="#00e5ff",
            bg="#1a1a22"
        ).pack(anchor=tk.NW, padx=12, pady=8)

        self.lbl_cam_view = tk.Label(cam_card, bg="#101014", text="Camera stream not started", fg="#777788")
        self.lbl_cam_view.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

        # Right: Spatial Radar Card
        radar_card = tk.Frame(parent, bg="#1a1a22", bd=1, relief="ridge")
        radar_card.grid(row=0, column=1, sticky="nsew", padx=8, pady=8)

        tk.Label(
            radar_card,
            text="FRONTAL 2D SPATIAL RADAR",
            font=("Helvetica", 10, "bold"),
            fg="#00e5ff",
            bg="#1a1a22"
        ).pack(anchor=tk.NW, padx=12, pady=8)

        self.lbl_radar_view = tk.Label(radar_card, bg="#101014", text="Radar updates with camera feed", fg="#777788")
        self.lbl_radar_view.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

    # =========================================================================
    # Actions
    # =========================================================================

    def on_check_connection(self):
        url = self.entry_url.get().strip()
        self.client.set_url(url)
        self.lbl_conn_status.config(text="● Connecting...", fg="#ffaa00")

        def task():
            ok, msg = self.client.check_connection()
            def update():
                if ok:
                    self.lbl_conn_status.config(text=f"● {msg}", fg="#00ff88")
                else:
                    self.lbl_conn_status.config(text=f"● {msg}", fg="#ff4466")
            self.root.after(0, update)

        threading.Thread(target=task, daemon=True).start()

    def on_snapshot(self):
        self.client.set_url(self.entry_url.get().strip())
        self.lbl_status.config(text="Capturing single frame...")

        def task():
            frame = self.client.capture_frame()
            def update():
                if frame is not None:
                    self._process_and_display(frame)
                    self.lbl_status.config(text="Snapshot captured.")
                else:
                    self.lbl_status.config(text="Capture failed.")
            self.root.after(0, update)

        threading.Thread(target=task, daemon=True).start()

    def on_toggle_stream(self):
        if not self.is_streaming:
            self.client.set_url(self.entry_url.get().strip())
            self.is_streaming = True
            self.btn_stream.config(text="⏹ Stop Live Stream", style="Danger.TButton")
            self.lbl_status.config(text="Streaming live...")

            def stream_thread():
                def on_frame(frame, fps):
                    def update_ui():
                        self.lbl_fps.config(text=f"Stream FPS: {fps:.1f}")
                        self._process_and_display(frame)
                    self.root.after(0, update_ui)

                def stop_check():
                    return not self.is_streaming

                self.client.start_stream(frame_cb=on_frame, stop_check=stop_check, fps_limit=15.0)

            threading.Thread(target=stream_thread, daemon=True).start()
        else:
            self.is_streaming = False
            self.btn_stream.config(text="▶ Start Live Stream", style="Primary.TButton")
            self.lbl_status.config(text="Stream stopped.")

    def _process_and_display(self, frame_gray: np.ndarray):
        self.latest_frame = frame_gray
        
        # Analyze obstacles & distance
        analyzed_bgr, dist_m = self.engine.analyze_frame_obstacles(frame_gray)
        self.latest_analyzed = analyzed_bgr
        self.latest_dist = dist_m
        self.lbl_dist.config(text=f"Obstacle Distance: {dist_m:.2f} m")

        # Display camera feed
        self._display_cam(analyzed_bgr)

        # Generate & display radar
        radar_bgr = self.engine.generate_fov_radar_map(dist_m)
        self.latest_radar_img = radar_bgr
        self._display_radar(radar_bgr)

    def _display_cam(self, bgr_img: np.ndarray):
        h, w, _ = bgr_img.shape
        lbl_w = max(240, self.lbl_cam_view.winfo_width())
        lbl_h = max(200, self.lbl_cam_view.winfo_height())

        scale = min(lbl_w / w, lbl_h / h)
        new_w, new_h = max(10, int(w * scale)), max(10, int(h * scale))

        resized = cv2.resize(bgr_img, (new_w, new_h), interpolation=cv2.INTER_NEAREST)
        rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        img = Image.fromarray(rgb)
        imgtk = ImageTk.PhotoImage(image=img)

        self.lbl_cam_view.config(image=imgtk, text="")
        self.lbl_cam_view.image = imgtk

    def _display_radar(self, radar_bgr: np.ndarray):
        h, w, _ = radar_bgr.shape
        lbl_w = max(240, self.lbl_radar_view.winfo_width())
        lbl_h = max(200, self.lbl_radar_view.winfo_height())

        scale = min(lbl_w / w, lbl_h / h)
        new_w, new_h = max(10, int(w * scale)), max(10, int(h * scale))

        resized = cv2.resize(radar_bgr, (new_w, new_h), interpolation=cv2.INTER_AREA)
        rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
        img = Image.fromarray(rgb)
        imgtk = ImageTk.PhotoImage(image=img)

        self.lbl_radar_view.config(image=imgtk, text="")
        self.lbl_radar_view.image = imgtk

    def on_export(self):
        if self.latest_analyzed is None:
            messagebox.showinfo("No Image", "Capture or stream a frame before saving.")
            return

        os.makedirs(self.config.output_dir, exist_ok=True)
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        cam_path = os.path.join(self.config.output_dir, f"frame_{ts}.png")
        radar_path = os.path.join(self.config.output_dir, f"radar_{ts}.png")
        data_path = os.path.join(self.config.output_dir, f"data_{ts}.json")

        cv2.imwrite(cam_path, self.latest_analyzed)
        if self.latest_radar_img is not None:
            cv2.imwrite(radar_path, self.latest_radar_img)

        data = {
            "timestamp": ts,
            "obstacle_distance_m": round(self.latest_dist, 3)
        }
        with open(data_path, "w") as f:
            json.dump(data, f, indent=2)

        messagebox.showinfo(
            "Saved",
            f"Saved to {self.config.output_dir}/\n• {os.path.basename(cam_path)}\n• {os.path.basename(radar_path)}"
        )
