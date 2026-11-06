/*
 * Quick I2C / SCCB Probe for OV7670
 * 
 * Generates 10MHz XCLK on GPIO 18 and checks if OV7670 responds on address 0x21
 */

#include <Wire.h>

#define XCLK_PIN 18
#define SDA_PIN  17 // TX2
#define SCL_PIN  16 // RX2

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- OV7670 Hardware Diagnostic Tool ---");

  // Step 1: Generate 10 MHz XCLK using ESP32 LEDC PWM
  // (OV7670 needs XCLK to respond on I2C)
  #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(XCLK_PIN, 10000000, 1);
    ledcWrite(XCLK_PIN, 1); // 50% duty cycle
  #else
    ledcSetup(0, 10000000, 1);
    ledcAttachPin(XCLK_PIN, 0);
    ledcWrite(0, 1);
  #endif
  Serial.println("[1] Generated 10 MHz XCLK on GPIO 18.");

  // Step 2: Initialize I2C with internal pullups enabled
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN, 100000); // 100 kHz I2C
  delay(100);

  Serial.printf("[2] Scanning I2C bus on SDA=%d, SCL=%d...\n", SDA_PIN, SCL_PIN);
  
  int devicesFound = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("  --> SUCCESS: Found device at address 0x%02X", address);
      if (address == 0x21) {
        Serial.print(" (*** OV7670 Camera Sensor! ***)");
      }
      Serial.println();
      devicesFound++;
    } else if (error == 4) {
      Serial.printf("  --> Bus error at address 0x%02X\n", address);
    }
  }

  if (devicesFound == 0) {
    Serial.println("\n[!] NO I2C DEVICES RESPONDED.");
    Serial.println("Common fixes:");
    Serial.println("  1. Swap SDA (TX2) and SCL (RX2) wires.");
    Serial.println("  2. Check RESET wire (must be in 3.3V).");
    Serial.println("  3. Check PWDN wire (must be in GND).");
    Serial.println("  4. Add two 4.7k ohm pullup resistors from SDA & SCL to 3.3V.");
  } else {
    Serial.println("\n[✓] Camera hardware and I2C connection are verified!");
  }
}

void loop() {
  delay(5000);
}

