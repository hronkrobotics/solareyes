#pragma once
#include <Wire.h>
#include <U8g2lib.h>
#include "config.h"

// U8g2 only draws into a 128 x 64 framebuffer in RAM. The sketch sends that buffer to
// the SSD1306 itself, with the same init sequence that lit the v1 panel. A frame takes
// about 110 ms at 100 kHz, so it goes out one page (8 rows) per call and the loop can
// read the button in between.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0, U8X8_PIN_NONE, /* clock */ PIN_SCL, /* data */ PIN_SDA);

uint8_t nextPage = 8;
bool displayAsleep = false;

void oledCmd(uint8_t value) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(value);
  Wire.endTransmission();
}

void displayBegin() {
  const uint8_t seq[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
      0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
  for (uint8_t value : seq) {
    oledCmd(value);
  }
  displayAsleep = false;
}

// Display off and charge pump off. The panel keeps its RAM and draws only microamps.
void displaySleep() {
  oledCmd(0xAE);
  oledCmd(0x8D);
  oledCmd(0x10);
  nextPage = 8;
  displayAsleep = true;
}

void displayWake() {
  oledCmd(0x8D);
  oledCmd(0x14);
  oledCmd(0xAF);
  displayAsleep = false;
}

bool displayBusy() {
  return nextPage < 8;
}

void displayStartFrame() {
  // Point the panel back at the top-left corner in case the last frame was cut short.
  const uint8_t window[] = {0x21, 0, 127, 0x22, 0, 7};
  for (uint8_t value : window) {
    oledCmd(value);
  }
  nextPage = 0;
}

void displaySendPage() {
  const uint8_t *row = u8g2.getBufferPtr() + nextPage * 128;
  oledCmd(0xB0 | nextPage);
  oledCmd(0x00);
  oledCmd(0x10);
  for (int col = 0; col < 128; col += 16) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40);
    for (int i = 0; i < 16; i++) {
      Wire.write(row[col + i]);
    }
    Wire.endTransmission();
  }
  nextPage++;
}
