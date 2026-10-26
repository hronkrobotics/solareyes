#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>

// Solar Eyes v0.1
// ESP32-C3 SuperMini + 0.96" 128x64 I2C OLED
// SDA = GPIO6, SCL = GPIO7. These are chosen pins for this build.
// Initialize them explicitly. They are not a universal ESP32-C3 default.

#define SDA_PIN 6
#define SCL_PIN 7

// Yellow/blue glass is fixed by the module. On a typical 0.96" panel the
// top 16 rows are yellow and the rest are blue when the module is upright.
// If the picture is shifted or garbled, the controller is likely SH1106.
// Replace the constructor with:
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN);
// If the yellow band sits at the bottom after mounting, use U8G2_R2.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0, U8X8_PIN_NONE, /* clock */ SCL_PIN, /* data */ SDA_PIN);

enum class Expr : uint8_t {
  Idle,
  Blink,
  Glance,
  Surprised,
  Sleepy
};

static const int LEFT_X = 32;
static const int RIGHT_X = 96;
static const int EYE_Y = 36;
static const uint32_t FRAME_MS = 33;

Expr expr = Expr::Idle;
int8_t glanceDir = 1;
uint32_t exprStart = 0;
uint32_t exprEnd = 0;
uint32_t nextFrame = 0;

void enterExpr(Expr next, uint32_t now, uint32_t durationMs) {
  expr = next;
  exprStart = now;
  exprEnd = now + durationMs;
}

int blinkLid(uint32_t now) {
  uint32_t elapsed = now - exprStart;
  uint32_t dur = exprEnd - exprStart;
  if (dur == 0) {
    return 0;
  }
  uint32_t half = dur / 2;
  if (half == 0) {
    return 100;
  }
  if (elapsed <= half) {
    return (int)((elapsed * 100) / half);
  }
  uint32_t down = dur - half;
  if (down == 0 || elapsed >= dur) {
    return 0;
  }
  return (int)(((dur - elapsed) * 100) / down);
}

void advanceExpr(uint32_t now) {
  // Every open expression closes with a blink, then a new expression is chosen.
  if (expr != Expr::Blink) {
    enterExpr(Expr::Blink, now, 220);
    return;
  }

  uint32_t roll = esp_random() % 100;
  if (roll < 45) {
    enterExpr(Expr::Idle, now, 1400 + (esp_random() % 900));
  } else if (roll < 70) {
    glanceDir = (esp_random() & 1) ? 1 : -1;
    enterExpr(Expr::Glance, now, 800 + (esp_random() % 500));
  } else if (roll < 85) {
    enterExpr(Expr::Surprised, now, 700);
  } else {
    enterExpr(Expr::Sleepy, now, 1500 + (esp_random() % 800));
  }
}

void drawEye(int cx, int rx, int ry, int pupilDx, int pupilDy, int pupilR, int lid) {
  int maxDx = rx - pupilR - 2;
  int maxDy = ry - pupilR - 2;
  if (maxDx < 0) {
    maxDx = 0;
  }
  if (maxDy < 0) {
    maxDy = 0;
  }
  pupilDx = constrain(pupilDx, -maxDx, maxDx);
  pupilDy = constrain(pupilDy, -maxDy, maxDy);

  u8g2.setDrawColor(1);
  u8g2.drawFilledEllipse(cx, EYE_Y, rx, ry);

  u8g2.setDrawColor(0);
  u8g2.drawDisc(cx + pupilDx, EYE_Y + pupilDy, pupilR);

  // Specular dot, kept off so it reads on the black pupil.
  u8g2.setDrawColor(1);
  u8g2.drawPixel(cx + pupilDx - pupilR / 2, EYE_Y + pupilDy - pupilR / 2);

  if (lid > 0) {
    int cover = (ry * 2 * lid) / 100;
    u8g2.setDrawColor(0);
    u8g2.drawBox(cx - rx - 2, EYE_Y - ry - 2, rx * 2 + 4, cover + 2);
  }
}

void drawBrow(int cx, int rx, int y) {
  int width = rx * 2 - 8;
  if (width < 8) {
    width = 8;
  }
  if (y < 0) {
    y = 0;
  }
  if (y > 12) {
    y = 12;
  }
  u8g2.setDrawColor(1);
  u8g2.drawRBox(cx - width / 2, y, width, 3, 1);
}

void render(uint32_t now) {
  int rx = 16;
  int ry = 14;
  int pupilR = 4;
  int pdx = 0;
  int pdy = 0;
  int lid = 0;
  int browY = 6;

  switch (expr) {
    case Expr::Idle:
      break;
    case Expr::Blink:
      lid = blinkLid(now);
      break;
    case Expr::Glance:
      pdx = glanceDir * 6;
      browY = (glanceDir < 0) ? 5 : 7;
      break;
    case Expr::Surprised:
      rx = 18;
      ry = 18;
      pupilR = 2;
      browY = 2;
      break;
    case Expr::Sleepy:
      ry = 12;
      pupilR = 3;
      pdy = 3;
      lid = 46;
      browY = 10;
      break;
  }

  u8g2.clearBuffer();
  drawEye(LEFT_X, rx, ry, pdx, pdy, pupilR, lid);
  drawEye(RIGHT_X, rx, ry, pdx, pdy, pupilR, lid);
  drawBrow(LEFT_X, rx, browY);
  drawBrow(RIGHT_X, rx, browY);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);

  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(255);

  uint32_t now = millis();
  enterExpr(Expr::Idle, now, 1600);
  nextFrame = now;
  render(now);

  Serial.println("Solar Eyes v0.1");
  Serial.println("SDA GPIO6, SCL GPIO7, SSD1306 128x64, Wi-Fi off");
}

void loop() {
  uint32_t now = millis();

  if ((int32_t)(now - exprEnd) >= 0) {
    advanceExpr(now);
  }

  if ((int32_t)(now - nextFrame) < 0) {
    return;
  }
  nextFrame = now + FRAME_MS;
  render(now);
}
