import os
os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")
from dataclasses import dataclass

@dataclass
class ScannerConfig:
    # Network
    esp32_url: str = "http://192.168.1.50"
    timeout_sec: float = 3.0
    
    # Camera specifications (OV7670 QQVGA)
    frame_width: int = 160
    frame_height: int = 120
    camera_fov_horizontal: float = 60.0  # horizontal FOV in degrees
    camera_height_cm: float = 20.0       # camera height above floor in cm
    
    # Spatial / Obstacle detection
    radar_max_range_m: float = 4.0       # max plot range in meters
    
    # Storage
    output_dir: str = os.path.join(os.path.dirname(__file__), "..", "scans")

DEFAULT_CONFIG = ScannerConfig()
