/*
 * Dedicated Non-FIFO OV7670 Camera Server using kobatan/OV7670-ESP32 Library
 * Features ultra-reliable Open Wi-Fi Hotspot ('ESP32-Camera')
 */

#include <OV7670.h>
#include <WiFi.h>
#include <Wire.h>
#include "camera_pins.h"

// Wi-Fi Credentials for your router / phone hotspot
const char* WIFI_SSID     = "Subhradeep";
const char* WIFI_PASSWORD = "12345678";

// Standalone Access Point Name
const char* AP_SSID       = "ESP32-Camera";

OV7670 cam;
WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  Dedicated Non-FIFO OV7670 Camera Server ");
  Serial.println("==========================================");

  // Initialize Wire for SCCB on our exact pins (SDA=17, SCL=16)
  Wire.begin(SIOD_GPIO_NUM, SIOC_GPIO_NUM, 100000);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  
  // Data bus mapping for I2Scamera
  config.D0           = Y2_GPIO_NUM; // D34
  config.D1           = Y3_GPIO_NUM; // D35
  config.D2           = Y4_GPIO_NUM; // D32
  config.D3           = Y5_GPIO_NUM; // VP (D36)
  config.D4           = Y6_GPIO_NUM; // VN (D39)
  config.D5           = Y7_GPIO_NUM; // D19
  config.D6           = Y8_GPIO_NUM; // D4
  config.D7           = Y9_GPIO_NUM; // D13
  config.XCLK         = XCLK_GPIO_NUM; // D18
  config.PCLK         = PCLK_GPIO_NUM; // D23
  config.VSYNC        = VSYNC_GPIO_NUM; // D21
  config.xclk_freq_hz = 10000000;

  Serial.println("[INFO] Initializing OV7670 with dedicated driver...");
  esp_err_t err = cam.init(&config, QQVGA, RGB565);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed: 0x%x\n", err);
    while (true) { delay(1000); }
  }
  
  // Reset registers over SCCB to load QQVGA RGB565 settings
  cam.reset();
  Serial.println("[SUCCESS] OV7670 initialized successfully!");

  // Try router first
  Serial.printf("[WIFI] Connecting to '%s'...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected to router!");
    Serial.print("[WIFI] Camera URL: http://");
    Serial.println(WiFi.localIP());
  } else {
    // Switch completely to pure Access Point mode on Channel 1
    Serial.println("\n[WIFI] Could not connect to router.");
    Serial.println("[WIFI] Starting Open Wi-Fi Hotspot on Channel 1...");
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    
    // Broadcast open network on Channel 1 (no password needed)
    WiFi.softAP(AP_SSID, NULL, 1);
    delay(300);

    Serial.printf("[WIFI] Broadcast Active: '%s' (Open network, no password)\n", AP_SSID);
    Serial.print("[WIFI] Camera URL: http://");
    Serial.println(WiFi.softAPIP());
  }

  server.begin();
  Serial.println("[HTTP] Server listening on port 80.");
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  String req = client.readStringUntil('\r');
  client.flush();

  if (req.indexOf("GET /status") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close\r\n");
    String currentIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    client.printf("{\"status\":\"ready\",\"ip\":\"%s\"}\n", currentIP.c_str());
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close\r\n");
  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'><title>OV7670 Camera</title></head>");
  client.println("<body style='background:#141419;color:#00e5ff;text-align:center;font-family:sans-serif;padding:30px;'>");
  client.println("<h2>📷 Non-FIFO OV7670 Camera Live!</h2>");
  client.println("<p style='color:#a0a0b0;'>Hardware connection & I2S DMA active.</p>");
  client.println("<p style='color:#00ff88;font-size:18px;'>● Connected to ESP32 Hotspot successfully!</p>");
  client.println("</body></html>");
}
