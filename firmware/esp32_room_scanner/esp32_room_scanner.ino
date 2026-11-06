/*
 * Dedicated Non-FIFO OV7670 Camera Server with Real-time Frame Capture
 * Compatible with Python Dashboard & Web Browser
 */

#include <OV7670.h>
#include <WiFi.h>
#include <Wire.h>
#include "camera_pins.h"

// Wi-Fi Credentials for router / phone hotspot (optional)
const char* WIFI_SSID     = "Subhradeep";
const char* WIFI_PASSWORD = "12345678";

// Standalone Access Point Name
const char* AP_SSID       = "ESP32-Camera";

OV7670 cam;
WiFiServer server(80);

// Frame buffer in internal RAM: 160 x 120 uint16_t (38.4 KB)
static uint16_t frameBuffer[160 * 120];

// Pre-computed 54-byte BMP Header for 160x120 24-bit BGR image (total 57,654 bytes)
const uint8_t bmpHeader[54] = {
  0x42, 0x4D, 0x36, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0xA0, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

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

  // 1. Status endpoint
  if (req.indexOf("GET /status") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close\r\n");
    String currentIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    client.printf("{\"status\":\"ready\",\"ip\":\"%s\"}\n", currentIP.c_str());
    return;
  }

  // 2. Camera Frame Capture endpoint (BMP format)
  if (req.indexOf("GET /capture") != -1) {
    // A. Read 120 lines sequentially into RAM buffer (forward raster order)
    for (int y = 0; y < 120; y++) {
      uint16_t* line = cam.getLine(y);
      if (line != NULL) {
        memcpy(&frameBuffer[y * 160], line, 160 * sizeof(uint16_t));
      } else {
        memset(&frameBuffer[y * 160], 0, 160 * sizeof(uint16_t));
      }
    }

    // B. Send HTTP header + BMP header
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/bmp");
    client.println("Content-Length: 57654");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Connection: close\r\n");
    client.write(bmpHeader, sizeof(bmpHeader));

    // C. Stream BMP rows bottom-to-top from in-memory buffer
    uint8_t rowBgr[160 * 3]; // 480 bytes per row
    for (int y = 119; y >= 0; y--) {
      uint16_t* line = &frameBuffer[y * 160];
      for (int x = 0; x < 160; x++) {
        uint16_t p = line[x];
        // RGB565 to 24-bit BGR conversion
        rowBgr[x * 3 + 0] = (p & 0x1F) << 3;          // Blue
        rowBgr[x * 3 + 1] = ((p >> 5) & 0x3F) << 2;   // Green
        rowBgr[x * 3 + 2] = ((p >> 11) & 0x1F) << 3;  // Red
      }
      client.write(rowBgr, sizeof(rowBgr));
    }
    return;
  }

  // 3. Web browser live auto-refresh view
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close\r\n");
  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'><title>OV7670 Live</title>");
  client.println("<style>body{background:#141419;color:#00e5ff;text-align:center;font-family:sans-serif;padding:20px;}");
  client.println("img{border:3px solid #00a8ff;border-radius:8px;image-rendering:pixelated;width:320px;height:240px;background:#000;}</style></head><body>");
  client.println("<h2>📷 Non-FIFO OV7670 Live Feed</h2>");
  client.println("<img id='feed' src='/capture' alt='Loading...'><br><br>");
  client.println("<p id='fps' style='color:#a0a0b0;'>FPS: calculating...</p>");
  client.println("<script>");
  client.println("const img = document.getElementById('feed');");
  client.println("const fpsText = document.getElementById('fps');");
  client.println("let count = 0, lastTime = performance.now();");
  client.println("function reload(){ img.src = '/capture?t=' + Date.now(); }");
  client.println("img.onload = () => {");
  client.println("  count++;");
  client.println("  const now = performance.now();");
  client.println("  if(now - lastTime >= 1000){ fpsText.innerText = 'FPS: ' + count; count = 0; lastTime = now; }");
  client.println("  setTimeout(reload, 60);");
  client.println("};");
  client.println("img.onerror = () => { setTimeout(reload, 500); };");
  client.println("</script></body></html>");
}
