import time
import requests
import numpy as np
import cv2
from typing import Optional, Tuple, Callable

try:
    from config import ScannerConfig
except ImportError:
    from host_software.config import ScannerConfig

class ScannerClient:
    def __init__(self, config: ScannerConfig):
        self.config = config
        self.base_url = config.esp32_url.rstrip("/")

    def set_url(self, url: str):
        self.base_url = url.rstrip("/")
        if not self.base_url.startswith("http"):
            self.base_url = f"http://{self.base_url}"
        self.config.esp32_url = self.base_url

    def check_connection(self) -> Tuple[bool, str]:
        """Checks if the ESP32 camera is reachable."""
        try:
            resp = requests.get(f"{self.base_url}/status", timeout=2.5)
            if resp.status_code == 200:
                return True, "Connected to Camera"
            return False, f"HTTP Error {resp.status_code}"
        except requests.exceptions.RequestException as e:
            return False, f"Connection Failed: {e.__class__.__name__}"

    def capture_frame(self) -> Optional[np.ndarray]:
        """Captures a single snapshot from the camera."""
        try:
            resp = requests.get(
                f"{self.base_url}/capture",
                timeout=self.config.timeout_sec
            )
            if resp.status_code == 200:
                content = resp.content
                # Strip any accidental leading whitespace before 'BM' magic bytes
                bm_idx = content.find(b'BM')
                if bm_idx != -1 and bm_idx > 0:
                    content = content[bm_idx:]
                
                img_bytes = np.frombuffer(content, dtype=np.uint8)
                frame = cv2.imdecode(img_bytes, cv2.IMREAD_GRAYSCALE)
                if frame is None and len(content) > 0:
                    print(f"[WARN] Failed to decode image ({len(content)} bytes). Response preview: {content[:60]}")
                return frame
            return None
        except requests.exceptions.RequestException as e:
            return None

    def start_stream(
        self,
        frame_cb: Callable[[np.ndarray, float], None],
        stop_check: Callable[[], bool],
        fps_limit: float = 15.0
    ):
        """
        Continuously polls the camera for frames and yields them to frame_cb(frame, fps).
        """
        interval = 1.0 / max(1.0, fps_limit)
        fps_counter = 0
        fps_start = time.time()
        current_fps = 0.0

        while not stop_check():
            t_start = time.time()
            frame = self.capture_frame()
            if frame is not None:
                fps_counter += 1
                if time.time() - fps_start >= 1.0:
                    current_fps = fps_counter / (time.time() - fps_start)
                    fps_counter = 0
                    fps_start = time.time()
                frame_cb(frame, current_fps)

            elapsed = time.time() - t_start
            sleep_time = interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
