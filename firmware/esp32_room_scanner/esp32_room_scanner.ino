/*
 * ESP32 + OV7670 Camera Server (Camera Only, No Servo)
 * 
 * Hardware:
 *   - ESP32 (WROOM-32 recommended)
 *   - OV7670 camera module (without FIFO)
 */

#include "esp_camera.h"
#include <WiFi.h>
#include "img_converters.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "camera_pins.h"

// ============================================================================
// Wi-Fi Credentials
// ============================================================================
const char* WIFI_SSID     = "Subhradeep";
const char* WIFI_PASSWORD = "12345678";

// ============================================================================
// Globals
// ============================================================================
WiFiServer server(80);

// ============================================================================
// Camera Initialization
// ============================================================================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  
  // 10 MHz XCLK clock
  config.xclk_freq_hz = 10000000;
  
  // Grayscale QQVGA = 160x120 pixels (~19.2 KB/frame), fits inside ESP32 internal SRAM
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAMERA] Initialization failed with error: 0x%x\n", err);
    return false;
  }
  
  Serial.println("[CAMERA] Initialized successfully.");
  return true;
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("     ESP32 + OV7670 Camera Streamer      ");
  Serial.println("==========================================");

  // Initialize Camera
  if (!initCamera()) {
    Serial.println("[FATAL] Camera could not start. Halting.");
    while (true) { delay(1000); }
  }

  // Connect Wi-Fi
  Serial.printf("[WIFI] Connecting to '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected successfully!");
    Serial.print("[WIFI] Camera URL: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WIFI] Connection failed. Check SSID/Password.");
  }

  server.begin();
  Serial.println("[HTTP] Server listening on port 80.");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  String req = client.readStringUntil('\r');
  client.flush();

  // 1. Status endpoint: GET /status
  if (req.indexOf("GET /status") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close\r\n");
    client.printf("{\"status\":\"ready\",\"ip\":\"%s\"}\n",
                  WiFi.localIP().toString().c_str());
    return;
  }

  // 2. Snapshot capture: GET /capture
  if (req.indexOf("GET /capture") != -1) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[CAMERA] Frame capture failed!");
      client.println("HTTP/1.1 503 Service Unavailable\r\n\r\nCapture failed");
      return;
    }

    uint8_t* jpg_buf = NULL;
    size_t jpg_size = 0;
    bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_size);
    esp_camera_fb_return(fb);

    if (!ok || jpg_buf == NULL) {
      client.println("HTTP/1.1 500 Internal Server Error\r\n\r\nJPEG conversion failed");
      return;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.printf("Content-Length: %u\r\n", jpg_size);
    client.println("Access-Control-Allow-Origin: *");
    client.println("Connection: close\r\n");
    client.write(jpg_buf, jpg_size);
    free(jpg_buf);
    return;
  }

  // 3. Web browser live viewer: GET /
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close\r\n");
  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Camera</title>");
  client.println("<style>");
  client.println("body{font-family:system-ui,sans-serif;background:#141419;color:#f0f0f5;text-align:center;padding:25px;}");
  client.println(".card{display:inline-block;background:#1e1e26;padding:24px;border-radius:12px;box-shadow:0 8px 24px rgba(0,0,0,0.5);}");
  client.println("img{border:3px solid #00a8ff;border-radius:8px;image-rendering:pixelated;width:320px;height:240px;background:#000;}");
  client.println(".badge{background:#00a8ff22;color:#00a8ff;padding:4px 12px;border-radius:20px;font-size:13px;font-weight:600;}");
  client.println("</style></head><body>");
  client.println("<div class='card'>");
  client.println("<h2>📷 ESP32 OV7670 Camera Stream</h2>");
  client.println("<span class='badge'>Live Auto-Refresh (QQVGA 160x120)</span><br><br>");
  client.println("<img id='feed' src='/capture' alt='Camera Feed'><br><br>");
  client.println("<p id='fps' style='color:#a0a0b0;'>FPS: calculating...</p>");
  client.println("</div>");
  client.println("<script>");
  client.println("const img = document.getElementById('feed');");
  client.println("const fpsText = document.getElementById('fps');");
  client.println("let count = 0, lastTime = performance.now();");
  client.println("function reload(){ img.src = '/capture?t=' + Date.now(); }");
  client.println("img.onload = () => {");
  client.println("  count++;");
  client.println("  const now = performance.now();");
  client.println("  if(now - lastTime >= 1000){");
  client.println("    fpsText.innerText = 'FPS: ' + count;");
  client.println("    count = 0; lastTime = now;");
  client.println("  }");
  client.println("  setTimeout(reload, 60);");
  client.println("};");
  client.println("img.onerror = () => { setTimeout(reload, 500); };");
  client.println("</script></body></html>");
}
