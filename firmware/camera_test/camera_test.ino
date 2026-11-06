/*
 * Standalone Camera Bringup Test for ESP32 + OV7670 (Non-FIFO)
 *
 * Verifies:
 *   1. XCLK clock output on GPIO 18
 *   2. I2C / SCCB communication on GPIO 16 (SCL) & GPIO 17 (SDA)
 *   3. 8-bit parallel DMA image capture (D0-D7, VSYNC, HREF, PCLK)
 *   4. Live auto-refreshing video feed in your web browser
 */

#include "esp_camera.h"
#include "img_converters.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <WiFi.h>

// ============================================================================
// 1. Wi-Fi Configuration (Put your Wi-Fi credentials here)
// ============================================================================
const char *WIFI_SSID = "Subhradeep";
const char *WIFI_PASSWORD = "12345678";

// ============================================================================
// 2. Camera Pin Definitions (Matching your exact physical wiring)
// ============================================================================
#define PWDN_GPIO_NUM -1  // Tied to GND physically
#define RESET_GPIO_NUM -1 // Tied to 3.3V physically
#define XCLK_GPIO_NUM 18  // D18 (XLK)
#define SIOD_GPIO_NUM 17  // TX2 (SDA)
#define SIOC_GPIO_NUM 16  // RX2 (SCL)

#define Y9_GPIO_NUM 13 // D13 (D7)
#define Y8_GPIO_NUM 4  // D4  (D6)
#define Y7_GPIO_NUM 19 // D19 (D5)
#define Y6_GPIO_NUM 39 // VN  (D4)
#define Y5_GPIO_NUM 36 // VP  (D3)
#define Y4_GPIO_NUM 32 // D32 (D2)
#define Y3_GPIO_NUM 35 // D35 (D1)
#define Y2_GPIO_NUM 34 // D34 (D0)

#define VSYNC_GPIO_NUM 21 // D21 (VS)
#define HREF_GPIO_NUM 22  // D22 (HS)
#define PCLK_GPIO_NUM 23  // D23 (PLK)

WiFiServer server(80);

void setup() {
  // Disable brownout detector to prevent reboots during Wi-Fi transmission
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  ESP32 + OV7670 Standalone Camera Test   ");
  Serial.println("==========================================");

  // Configure camera parameters
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  // OV7670 clock frequency (10MHz is stable for jumper-wire connections)
  config.xclk_freq_hz = 10000000;

  // Grayscale QQVGA (160x120) fits comfortably in internal ESP32 SRAM
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  Serial.println("[INFO] Probing OV7670 camera registers...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Camera init failed with error code: 0x%x\n", err);
    if (err == ESP_ERR_NOT_FOUND) {
      Serial.println("[DIAGNOSTIC] Camera not responding on I2C/SCCB!");
      Serial.println("   -> Check wiring on RX2 (GPIO 16) and TX2 (GPIO 17).");
      Serial.println("   -> Verify if 4.7k pull-up resistors to 3.3V are "
                     "needed on SDA & SCL.");
    }
    while (true) {
      delay(1000);
    }
  }

  Serial.println("[SUCCESS] OV7670 initialized and communicating!");

  // Connect to Wi-Fi
  Serial.printf("[WIFI] Connecting to '%s'...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int dots = 0;
  while (WiFi.status() != WL_CONNECTED && dots < 40) {
    delay(500);
    Serial.print(".");
    dots++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] Open this URL in your browser: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WIFI] Could not connect to Wi-Fi. Check SSID/Password.");
  }

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  String req = client.readStringUntil('\r');
  client.flush();

  // 1. Raw JPEG snapshot endpoint: GET /capture
  if (req.indexOf("GET /capture") != -1) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      client.println("HTTP/1.1 503 Service Unavailable\r\n\r\nCapture failed");
      return;
    }

    uint8_t *jpg_buf = NULL;
    size_t jpg_size = 0;
    bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_size);
    esp_camera_fb_return(fb);

    if (!ok || jpg_buf == NULL) {
      client.println(
          "HTTP/1.1 500 Internal Error\r\n\r\nJPEG conversion failed");
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

  // 2. Interactive Browser Live-View Page: GET /
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close\r\n");
  client.println("<!DOCTYPE html><html><head><meta "
                 "charset='utf-8'><title>OV7670 Live Test</title>");
  client.println("<style>");
  client.println("body{font-family:system-ui,sans-serif;background:#121217;"
                 "color:#e5e5ea;text-align:center;padding:20px;}");
  client.println(".card{display:inline-block;background:#1c1c24;padding:24px;"
                 "border-radius:12px;box-shadow:0 8px 24px rgba(0,0,0,0.5);}");
  client.println("img{border:3px solid "
                 "#00a8ff;border-radius:8px;image-rendering:pixelated;width:"
                 "320px;height:240px;background:#000;}");
  client.println(".badge{background:#00a8ff22;color:#00a8ff;padding:4px "
                 "12px;border-radius:20px;font-size:13px;font-weight:600;}");
  client.println("</style></head><body>");
  client.println("<div class='card'>");
  client.println("<h2>📷 OV7670 Non-FIFO Camera Test</h2>");
  client.println(
      "<span class='badge'>Live Auto-Refresh (QQVGA 160x120)</span><br><br>");
  client.println("<img id='feed' src='/capture' alt='Camera Feed'><br><br>");
  client.println("<p id='fps' style='color:#888;'>FPS: calculating...</p>");
  client.println("</div>");
  client.println("<script>");
  client.println("const img = document.getElementById('feed');");
  client.println("const fpsText = document.getElementById('fps');");
  client.println("let count = 0, lastTime = performance.now();");
  client.println("function reload(){");
  client.println("  img.src = '/capture?t=' + Date.now();");
  client.println("}");
  client.println("img.onload = () => {");
  client.println("  count++;");
  client.println("  const now = performance.now();");
  client.println("  if(now - lastTime >= 1000){");
  client.println("    fpsText.innerText = 'FPS: ' + count;");
  client.println("    count = 0; lastTime = now;");
  client.println("  }");
  client.println("  setTimeout(reload, 50);"); // ~15-20 FPS loop
  client.println("};");
  client.println("img.onerror = () => { setTimeout(reload, 500); };");
  client.println("</script></body></html>");
}
