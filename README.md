# ESP32 + OV7670 Camera Monitor & Spatial Detector

A dedicated camera streaming and obstacle detection software suite for an **ESP32** and **OV7670 camera module**.

---

## 1. Hardware Pinout & Wiring

| OV7670 Camera Pin | ESP32 Board Label | ESP32 GPIO | Description |
| :--- | :--- | :--- | :--- |
| **3V3** | `3V3` | 3.3V | 3.3V Logic Power (Never 5V) |
| **GND** | `GND` | GND | Ground |
| **SIOC** *(Clock)* | `RX2` | **GPIO 16** | SCCB / I2C Clock |
| **SIOD** *(Data)* | `TX2` | **GPIO 17** | SCCB / I2C Data |
| **VSYNC** | `D21` | **GPIO 21** | Vertical Sync |
| **HREF** | `D22` | **GPIO 22** | Horizontal Reference |
| **PCLK** | `D23` | **GPIO 23** | Pixel Clock |
| **XCLK** | `D18` | **GPIO 18** | Camera Clock (10 MHz PWM) |
| **D0** | `D34` | **GPIO 34** | Data Bit 0 (LSB) |
| **D1** | `D35` | **GPIO 35** | Data Bit 1 |
| **D2** | `D32` | **GPIO 32** | Data Bit 2 |
| **D3** | `VP` | **GPIO 36** | Data Bit 3 |
| **D4** | `VN` | **GPIO 39** | Data Bit 4 |
| **D5** | `D19` | **GPIO 19** | Data Bit 5 |
| **D6** | `D4` | **GPIO 4** | Data Bit 6 |
| **D7** | `D13` | **GPIO 13** | Data Bit 7 (MSB) |
| **RESET** | `3V3` | 3.3V | Pull to 3.3V (keeps camera awake) |
| **PWDN** | `GND` | GND | Pull to GND (normal mode) |

---

## 2. Firmware Flashing (Arduino IDE)

1. Open [`firmware/esp32_room_scanner/esp32_room_scanner.ino`](file:///Users/apple/camera%20module/firmware/esp32_room_scanner/esp32_room_scanner.ino).
2. Note: No servo libraries are required!
3. Select **Board:** `ESP32 Dev Module`.
4. Click **Upload** and open the **Serial Monitor (115200 baud)** to note the IP address.

---

## 3. Running the Host Software

### Graphical Monitor Dashboard
```bash
./.venv/bin/python run_scanner.py
```
* **Connect:** Enter the ESP32 IP address and click **Connect**.
* **Stream:** Click **▶ Start Live Stream** to see the real-time camera feed and obstacle detection radar.
* **Snapshot:** Click **📸 Capture Snapshot** to grab a single frame.
* **Save:** Click **💾 Save Frame & Analysis** to store the images in `scans/`.

### Terminal Mode (CLI)
```bash
./.venv/bin/python run_scanner.py --cli --ip 192.168.1.50
```
