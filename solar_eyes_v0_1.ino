#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// Solar Eyes v0.1
// ESP32-C3 SuperMini + 0.96" 128x64 I2C OLED
// SDA = GPIO6, SCL = GPIO7. These are chosen pins for this build.
// Initialize them explicitly. They are not a universal ESP32-C3 default.

#define SDA_PIN 6
#define SCL_PIN 7
#define OLED_ADDR 0x3C

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
  LookSide,
  LookUp,
  LookDown,
  Happy,
  Wink,
  Sleepy
};

// Rows 0-15 are the yellow glass. Leaving them off keeps the lit pixels blue.
static const int BLUE_TOP = 16;
static const int LEFT_X = 34;
static const int RIGHT_X = 96;
static const int EYE_Y = 44;
static const int BROW_Y = 18;
static const uint32_t FRAME_MS = 33;

Expr expr = Expr::Idle;
int8_t glanceDir = 1;
bool winkLeft = true;
uint32_t exprStart = 0;
uint32_t exprEnd = 0;
uint32_t nextFrame = 0;

void oledPush();

void enterExpr(Expr next, uint32_t now, uint32_t durationMs) {
  expr = next;
  exprStart = now;
  exprEnd = now + durationMs;
}

struct Pose {
  int dxL;
  int dxR;
  int dyL;
  int dyR;
  int wL;
  int wR;
  int hL;
  int hR;
  int browL;
  int browR;
  int archL;
  int archR;
};

Pose shown = {0, 0, 0, 0, 26, 26, 34, 34, 0, 0, 4, 4};

void approach(int &value, int target, int step) {
  if (value < target) {
    int next = value + step;
    value = next > target ? target : next;
  } else if (value > target) {
    int next = value - step;
    value = next < target ? target : next;
  }
}

void advanceExpr(uint32_t now) {
  uint32_t roll = esp_random() % 100;
  if (roll < 18) {
    enterExpr(Expr::Idle, now, 420 + (esp_random() % 280));
  } else if (roll < 34) {
    enterExpr(Expr::Blink, now, 170);
  } else if (roll < 58) {
    glanceDir = (esp_random() & 1) ? 1 : -1;
    enterExpr(Expr::LookSide, now, 480 + (esp_random() % 280));
  } else if (roll < 72) {
    enterExpr(Expr::LookUp, now, 420);
  } else if (roll < 84) {
    enterExpr(Expr::LookDown, now, 420);
  } else if (roll < 93) {
    winkLeft = esp_random() & 1;
    enterExpr(Expr::Wink, now, 260);
  } else {
    enterExpr(Expr::Sleepy, now, 700);
  }
}

void drawEyeShape(int cx, int cy, int w, int h) {
  if (w < 10) {
    w = 10;
  }
  if (h < 3) {
    h = 3;
  }
  int y = cy - h / 2;
  if (y < BLUE_TOP) {
    h -= BLUE_TOP - y;
    y = BLUE_TOP;
  }
  int radius = 5;
  if (radius > w / 2) {
    radius = w / 2;
  }
  if (radius > h / 2) {
    radius = h / 2;
  }
  u8g2.setDrawColor(1);
  u8g2.drawRBox(cx - w / 2, y, w, h, radius);
}

void drawBrow(int cx, int y, int w, int arch) {
  int x0 = cx - w / 2;
  int x1 = cx + w / 2;
  int yEnd = y;
  int yMid = y - arch;
  if (yEnd < BLUE_TOP) {
    yEnd = BLUE_TOP;
  }
  if (yMid < BLUE_TOP) {
    yMid = BLUE_TOP;
  }
  u8g2.setDrawColor(1);
  for (int t = 0; t < 2; t++) {
    u8g2.drawLine(x0, yEnd + t, cx, yMid + t);
    u8g2.drawLine(cx, yMid + t, x1, yEnd + t);
  }
}

void keepBlueOnly() {
  memset(u8g2.getBufferPtr(), 0, 128 * 2);
}

Pose targetPose() {
  Pose pose = {0, 0, 0, 0, 26, 26, 34, 34, 0, 0, 4, 4};
  switch (expr) {
    case Expr::Idle:
      break;
    case Expr::Blink:
      pose.hL = 4;
      pose.hR = 4;
      pose.browL = 2;
      pose.browR = 2;
      pose.archL = 2;
      pose.archR = 2;
      break;
    case Expr::LookSide:
      if (glanceDir < 0) {
        pose.dxL = -2;
        pose.dxR = -8;
        pose.wL = 28;
        pose.wR = 23;
        pose.hL = 36;
        pose.hR = 31;
        pose.browL = -1;
        pose.browR = 0;
        pose.archL = 5;
        pose.archR = 3;
      } else {
        pose.dxL = 8;
        pose.dxR = 2;
        pose.wL = 23;
        pose.wR = 28;
        pose.hL = 31;
        pose.hR = 36;
        pose.browL = 0;
        pose.browR = -1;
        pose.archL = 3;
        pose.archR = 5;
      }
      break;
    case Expr::LookUp:
      pose.dyL = -2;
      pose.dyR = -2;
      pose.hL = 36;
      pose.hR = 36;
      pose.browL = -2;
      pose.browR = -2;
      pose.archL = 5;
      pose.archR = 5;
      break;
    case Expr::LookDown:
      pose.dyL = 3;
      pose.dyR = 3;
      pose.hL = 26;
      pose.hR = 26;
      pose.browL = 2;
      pose.browR = 2;
      pose.archL = 2;
      pose.archR = 2;
      break;
    case Expr::Happy:
      pose.wL = 28;
      pose.wR = 28;
      pose.hL = 32;
      pose.hR = 32;
      pose.browL = -2;
      pose.browR = -2;
      pose.archL = 5;
      pose.archR = 5;
      break;
    case Expr::Wink:
      if (winkLeft) {
        pose.dxL = 2;
        pose.hL = 4;
        pose.browL = 1;
        pose.archL = 2;
      } else {
        pose.dxR = -2;
        pose.hR = 4;
        pose.browR = 1;
        pose.archR = 2;
      }
      break;
    case Expr::Sleepy:
      pose.dyL = 2;
      pose.dyR = 2;
      pose.hL = 22;
      pose.hR = 22;
      pose.browL = 2;
      pose.browR = 2;
      pose.archL = 2;
      pose.archR = 2;
      break;
  }
  return pose;
}

int bobAt(uint32_t now) {
  int bob = (int)((now / 140) % 8);
  if (bob > 4) {
    bob = 8 - bob;
  }
  return bob - 2;
}

void render(uint32_t now) {
  Pose goal = targetPose();
  approach(shown.dxL, goal.dxL, 1);
  approach(shown.dxR, goal.dxR, 1);
  approach(shown.dyL, goal.dyL, 1);
  approach(shown.dyR, goal.dyR, 1);
  approach(shown.wL, goal.wL, 1);
  approach(shown.wR, goal.wR, 1);
  approach(shown.hL, goal.hL, 2);
  approach(shown.hR, goal.hR, 2);
  approach(shown.browL, goal.browL, 1);
  approach(shown.browR, goal.browR, 1);
  approach(shown.archL, goal.archL, 1);
  approach(shown.archR, goal.archR, 1);

  int bobL = bobAt(now);
  int bobR = bobAt(now + 80);
  int browYL = BROW_Y + shown.browL + bobL;
  int browYR = BROW_Y + shown.browR + bobR;
  if (browYL < BLUE_TOP) {
    browYL = BLUE_TOP;
  }
  if (browYR < BLUE_TOP) {
    browYR = BLUE_TOP;
  }

  u8g2.clearBuffer();
  drawBrow(LEFT_X + shown.dxL, browYL, shown.wL, shown.archL);
  drawBrow(RIGHT_X + shown.dxR, browYR, shown.wR, shown.archR);
  drawEyeShape(LEFT_X + shown.dxL, EYE_Y + shown.dyL + bobL, shown.wL, shown.hL);
  drawEyeShape(RIGHT_X + shown.dxR, EYE_Y + shown.dyR + bobR, shown.wR, shown.hR);
  keepBlueOnly();
  oledPush();
}

void oledCmd(uint8_t value) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(value);
  Wire.endTransmission();
}

void oledInit() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  const uint8_t seq[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
      0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
      0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
  for (uint8_t value : seq) {
    oledCmd(value);
  }
}

void oledPush() {
  const uint8_t *buf = u8g2.getBufferPtr();
  for (uint8_t page = 0; page < 8; page++) {
    oledCmd(0xB0 | page);
    oledCmd(0x00);
    oledCmd(0x10);
    const uint8_t *row = buf + (page * 128);
    for (int col = 0; col < 128; col += 16) {
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(0x40);
      for (int i = 0; i < 16; i++) {
        Wire.write(row[col + i]);
      }
      Wire.endTransmission();
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Same 100 kHz init that lit this panel. U8g2 only draws into RAM.
  oledInit();

  uint32_t now = millis();
  enterExpr(Expr::Idle, now, 1600);
  nextFrame = now;
  render(now);

  Serial.println("Solar Eyes v0.1");
  Serial.println("SDA GPIO6, SCL GPIO7, SSD1306 128x64 @ 100 kHz");
}

void loop() {
  uint32_t now = millis();

  if ((int32_t)(now - exprEnd) >= 0) {
    advanceExpr(now);
  }

  if ((int32_t)(now - nextFrame) < 0) {
    // C3 is single-core. A tight spin here starves USB and the port drops.
    delay(5);
    return;
  }
  nextFrame = now + FRAME_MS;
  render(now);
}
