import cv2
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from typing import Tuple, List

try:
    from config import ScannerConfig
except ImportError:
    from host_software.config import ScannerConfig

class MappingEngine:
    def __init__(self, config: ScannerConfig):
        self.config = config

    def estimate_radial_distance(self, frame: np.ndarray) -> float:
        """
        Estimates the distance to the nearest obstacle/wall base in the frame
        using Ground-Plane Inverse Perspective Projection (IPM).
        """
        h, w = frame.shape
        roi = frame[:, w // 4 : 3 * w // 4]
        
        blurred = cv2.GaussianBlur(roi, (5, 5), 0)
        edges = cv2.Canny(blurred, 40, 110)

        obstacle_y = None
        for y in range(h - 5, h // 3, -1):
            row_energy = np.mean(edges[y, :])
            if row_energy > 25.0:
                obstacle_y = y
                break

        horizon_y = h * 0.45
        if obstacle_y is None or obstacle_y <= horizon_y:
            return self.config.radar_max_range_m

        dy = max(1.0, float(obstacle_y - horizon_y))
        focal_factor = 280.0
        distance_m = (self.config.camera_height_cm / 100.0) * (focal_factor / dy)
        return float(np.clip(distance_m, 0.2, self.config.radar_max_range_m))

    def analyze_frame_obstacles(self, frame: np.ndarray) -> Tuple[np.ndarray, float]:
        """
        Analyzes the frame and overlays obstacle/boundary detection lines
        and computed distance in meters.
        """
        h, w = frame.shape
        dist_m = self.estimate_radial_distance(frame)
        
        # Color visualization
        bgr = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
        blurred = cv2.GaussianBlur(frame, (5, 5), 0)
        edges = cv2.Canny(blurred, 40, 110)

        # Highlight edge points in green/red depending on distance
        color = (0, 0, 255) if dist_m < 0.8 else (0, 255, 120)
        bgr[edges > 0] = color

        # Bottom HUD with distance
        cv2.rectangle(bgr, (0, h - 22), (w, h), (20, 20, 25), -1)
        status_text = f"Front Distance: {dist_m:.2f}m"
        cv2.putText(bgr, status_text, (8, h - 7),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.38, (0, 240, 255), 1, cv2.LINE_AA)

        return bgr, dist_m

    def generate_fov_radar_map(self, dist_m: float) -> np.ndarray:
        """
        Generates a 2D Frontal Sector Radar (-30 deg to +30 deg) showing
        the detected obstacle distance in front of the camera.
        """
        fig, ax = plt.subplots(figsize=(4.0, 4.0), subplot_kw={'projection': 'polar'})
        fig.patch.set_facecolor('#1e1e26')
        ax.set_facecolor('#1e1e26')

        # Frontal sector from 60 to 120 degrees (centered at 90 deg = straight ahead)
        ax.set_theta_zero_location("E")
        ax.set_thetamin(60)
        ax.set_thetamax(120)
        ax.set_ylim(0, self.config.radar_max_range_m)

        ax.tick_params(colors='#888888', labelsize=8)
        ax.grid(color='#444455', linestyle='--', linewidth=0.8)

        # Draw detected obstacle arc
        sector_angles = np.linspace(np.radians(65), np.radians(115), 30)
        sector_dists = [dist_m] * len(sector_angles)

        color = '#ff0055' if dist_m < 1.0 else '#00f0ff'
        ax.plot(sector_angles, sector_dists, color=color, linewidth=3.5, label='Front Obstacle')
        ax.scatter([np.radians(90)], [dist_m], color=color, s=70, zorder=5)

        # Camera position
        ax.scatter([np.radians(90)], [0], color='#00ff88', s=70, marker='^', label='Camera Position')

        ax.set_title("Frontal Spatial Radar (60° FOV)", color='#ffffff', fontsize=11, pad=12)
        ax.legend(loc='lower center', bbox_to_anchor=(0.5, -0.25), ncol=2,
                  fontsize=8, facecolor='#2a2a35', edgecolor='none', labelcolor='white')

        fig.tight_layout()
        fig.canvas.draw()

        rgba = np.asarray(fig.canvas.buffer_rgba())
        bgr = cv2.cvtColor(rgba, cv2.COLOR_RGBA2BGR)
        plt.close(fig)

        return bgr
