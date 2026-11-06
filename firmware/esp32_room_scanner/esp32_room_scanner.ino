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
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("Connection: close\r\n\r\n");
    String currentIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    client.printf("{\"status\":\"ready\",\"ip\":\"%s\"}\n", currentIP.c_str());
    return;
  }

  // 2. Camera Frame Capture endpoint (BMP format)
  if (req.indexOf("GET /capture") != -1) {
    // A. Capture 160x120 frame from camera into internal RAM
    cam.getFrame((uint8_t*)frameBuffer);

    // B. Send HTTP header + BMP header
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: image/bmp\r\n");
    client.print("Content-Length: 57654\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("Connection: close\r\n\r\n");
    client.write(bmpHeader, sizeof(bmpHeader));

    // C. Stream BMP rows bottom-to-top from in-memory buffer
    uint8_t rowBgr[160 * 3]; // 480 bytes per row
    for (int y = 119; y >= 0; y--) {
      uint16_t* line = &frameBuffer[y * 160];
      for (int x = 0; x < 160; x++) {
        uint16_t p = line[x];
        uint8_t b0 = p & 0xFF;         // Byte 0 from sensor: RRRRRGGG
        uint8_t b1 = (p >> 8) & 0xFF;  // Byte 1 from sensor: GGGBBBBB

        // TrueColor 24-bit BGR conversion
        rowBgr[x * 3 + 0] = (b1 & 0x1F) << 3;                          // Blue
        rowBgr[x * 3 + 1] = ((b0 & 0x07) << 5) | ((b1 & 0xE0) >> 3);  // Green
        rowBgr[x * 3 + 2] = b0 & 0xF8;                                 // Red
      }
      client.write(rowBgr, sizeof(rowBgr));
    }
    Serial.println("[HTTP] Frame captured & sent.");
    return;
  }

  // 3. Web browser live auto-refresh view
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/html\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print("<!DOCTYPE html><html><head><meta charset='utf-8'><title>OV7670 Live</title>");
  client.print("<style>body{background:#141419;color:#00e5ff;text-align:center;font-family:sans-serif;padding:20px;}");
  client.print("img{border:3px solid #00a8ff;border-radius:8px;image-rendering:pixelated;width:320px;height:240px;background:#000;}</style></head><body>");
  client.print("<h2>📷 Non-FIFO OV7670 Live Feed</h2>");
  client.print("<img id='feed' src='/capture' alt='Loading...'><br><br>");
  client.print("<p id='fps' style='color:#a0a0b0;'>FPS: calculating...</p>");
  client.print("<script>");
  client.print("const img = document.getElementById('feed');");
  client.print("const fpsText = document.getElementById('fps');");
  client.print("let count = 0, lastTime = performance.now();");
  client.print("function reload(){ img.src = '/capture?t=' + Date.now(); }");
  client.print("img.onload = () => {");
  client.print("  count++;");
  client.print("  const now = performance.now();");
  client.print("  if(now - lastTime >= 1000){ fpsText.innerText = 'FPS: ' + count; count = 0; lastTime = now; }");
  client.print("  setTimeout(reload, 60);");
  client.print("};");
  client.print("img.onerror = () => { setTimeout(reload, 500); };");
  client.print("</script></body></html>");
}
