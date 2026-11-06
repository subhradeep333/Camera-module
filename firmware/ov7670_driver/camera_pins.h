#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

#define PWDN_GPIO_NUM     -1   // Tied to GND physically
#define RESET_GPIO_NUM    -1   // Tied to 3.3V physically
#define XCLK_GPIO_NUM     18   // D18 (XLK)
#define SIOD_GPIO_NUM     17   // TX2 (SDA)
#define SIOC_GPIO_NUM     16   // RX2 (SCL)

#define Y9_GPIO_NUM       13   // D13 (D7)
#define Y8_GPIO_NUM        4   // D4  (D6)
#define Y7_GPIO_NUM       19   // D19 (D5)
#define Y6_GPIO_NUM       39   // VN  (D4)
#define Y5_GPIO_NUM       36   // VP  (D3)
#define Y4_GPIO_NUM       32   // D32 (D2)
#define Y3_GPIO_NUM       35   // D35 (D1)
#define Y2_GPIO_NUM       34   // D34 (D0)

#define VSYNC_GPIO_NUM    21   // D21 (VS)
#define HREF_GPIO_NUM     22   // D22 (HS)
#define PCLK_GPIO_NUM     23   // D23 (PLK)

#endif // CAMERA_PINS_H
