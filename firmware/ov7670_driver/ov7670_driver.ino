/*
 * Dedicated Non-FIFO OV7670 Camera Server using kobatan/OV7670-ESP32 Library
 * 
 * Requires:
 *   - OV7670-ESP32 library installed via:
 *     Sketch > Include Library > Add .ZIP Library...
 *   - ESP32 board package 2.0.17 (Tools > Board > Boards Manager > esp32)
 */

#include <OV7670.h>
#include <WiFi.h>
#include "camera_pins.h"

// Wi-Fi Credentials
const char* WIFI_SSID     = "Subhradeep";
const char* WIFI_PASSWORD = "12345678";

OV7670 cam;
WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  Dedicated Non-FIFO OV7670 Camera Server ");
  Serial.println("==========================================");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM; // D34
  config.pin_d1       = Y3_GPIO_NUM; // D35
  config.pin_d2       = Y4_GPIO_NUM; // D32
  config.pin_d3       = Y5_GPIO_NUM; // VP (D36)
  config.pin_d4       = Y6_GPIO_NUM; // VN (D39)
  config.pin_d5       = Y7_GPIO_NUM; // D19
  config.pin_d6       = Y8_GPIO_NUM; // D4
  config.pin_d7       = Y9_GPIO_NUM; // D13
  config.pin_xclk     = XCLK_GPIO_NUM; // D18
  config.pin_pclk     = PCLK_GPIO_NUM; // D23
  config.pin_vsync    = VSYNC_GPIO_NUM; // D21
  config.pin_href     = HREF_GPIO_NUM;  // D22
  config.pin_sscb_sda = SIOD_GPIO_NUM;  // TX2 (GPIO 17)
  config.pin_sscb_scl = SIOC_GPIO_NUM;  // RX2 (GPIO 16)
  config.pin_pwdn     = PWDN_GPIO_NUM;  // GND
  config.pin_reset    = RESET_GPIO_NUM; // 3.3V
  config.xclk_freq_hz = 10000000;
  config.pixel_format = CAMERA_PF_GRAYSCALE;
  config.frame_size   = CAMERA_FS_QQVGA; // 160x120
  config.fb_count     = 1;

  Serial.println("[INFO] Initializing OV7670 with dedicated driver...");
  esp_err_t err = cam.init(&config, OV7670::Mode::QQVGA_YUV422);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
    while (true) { delay(1000); }
  }
  Serial.println("[SUCCESS] OV7670 initialized successfully!");

  // Connect to Wi-Fi
  Serial.printf("[WIFI] Connecting to '%s'...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected!");
  Serial.print("[WIFI] Camera URL: http://");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  String req = client.readStringUntil('\r');
  client.flush();

  // Simple capture endpoint
  if (req.indexOf("GET /capture") != -1 || req.indexOf("GET /status") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close\r\n");
    client.println("Camera Active");
    return;
  }
}
