#pragma once
#include <Wire.h>
#include "config.h"

// Shake detection with the LSM6DS3TR-C IMU built into the XIAO nRF52840 Sense. It sits on
// the board's internal I2C bus (Wire1) and is powered from a pin that stays on during
// System OFF, so it can wake the board:
// - Its wake-up interrupt watches high-pass filtered data, so gravity and slow tilting
//   are ignored. Past WAKE_MG it drives INT1 high and holds it there until WAKE_UP_SRC is
//   read. That pin wakes the board, and while awake any movement counts as activity.
// - In software, each reading is split into gravity (a smoothed average), which gives the
//   tilt, and what is left over, which is the case being moved or shaken.
// The gyroscope stays off. If the IMU does not answer, motion is skipped and only the
// buttons wake the board. To use a different accelerometer, only this file has to change.

struct Motion {
  bool moved;    // a bump past WAKE_MG, or the tilt changed
  bool shaking;  // shaken within the last 300 ms
  bool dizzy;    // shaken enough to get dizzy
  float tiltX;   // -1 to 1, positive when tilted left, so the eyes look right
  float tiltY;   // -1 to 1, positive when tilted back, so the eyes look down
  float pushX;   // the case's acceleration in g, toward the screen's right
  float pushY;   // and toward the top of the screen
};

namespace lsm6ds3 {
constexpr uint8_t ADDR = 0x6A;
constexpr uint8_t WHO_AM_I = 0x0F;
constexpr uint8_t ID = 0x6A;
constexpr uint8_t CTRL1_XL = 0x10;
constexpr uint8_t CTRL2_G = 0x11;
constexpr uint8_t CTRL3_C = 0x12;
constexpr uint8_t CTRL6_C = 0x15;
constexpr uint8_t CTRL8_XL = 0x17;
constexpr uint8_t WAKE_UP_SRC = 0x1B;
constexpr uint8_t OUTX_L_XL = 0x28;
constexpr uint8_t TAP_CFG = 0x58;
constexpr uint8_t WAKE_UP_THS = 0x5B;
constexpr uint8_t WAKE_UP_DUR = 0x5C;
constexpr uint8_t MD1_CFG = 0x5E;
constexpr uint8_t WU_IA = 0x08;  // wake-up event bit in WAKE_UP_SRC
}  // namespace lsm6ds3

bool imuFound = false;
int32_t imuMg[3] = {0, 0, 0};  // the last reading on the IMU's own X, Y, Z
float gravity[3] = {0, 0, 0};  // smoothed, toward the screen's right, top, and front
bool haveGravity = false;
float tiltRef[3] = {0, 0, 0};  // the direction of gravity at the last tilt change
float shakeLevel = 0;
bool shakenOnce = false;
uint32_t lastShakeAt = 0;

bool imuWrite(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(lsm6ds3::ADDR);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission() == 0;
}

bool imuRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire1.beginTransmission(lsm6ds3::ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }
  if (Wire1.requestFrom(lsm6ds3::ADDR, (size_t)len) != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire1.read();
  }
  return true;
}

int imuRead(uint8_t reg) {
  uint8_t value;
  return imuRead(reg, &value, 1) ? value : -1;
}

// At +-4 g, one wake-up threshold step is 62.5 mg.
uint8_t wakeSteps(uint16_t mg) {
  uint16_t steps = (mg * 2 + 62) / 125;
  if (steps < 1) {
    steps = 1;
  }
  if (steps > 63) {
    steps = 63;
  }
  return steps;
}

bool motionBegin() {
  // High drive on the IMU's power pin, the same as Seeed's own IMU library.
  uint32_t power = g_ADigitalPinMap[PIN_LSM6DS3TR_C_POWER];
  nrf_gpio_cfg(power, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
               NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
  nrf_gpio_pin_set(power);
  delay(20);  // the IMU boots for 15 ms after power-up

  Wire1.begin();
  Wire1.setClock(400000);
  if (imuRead(lsm6ds3::WHO_AM_I) != lsm6ds3::ID) {
    return false;
  }

  imuWrite(lsm6ds3::CTRL3_C, 0x01);  // software reset
  delay(5);
  imuWrite(lsm6ds3::CTRL3_C, 0x44);  // block data update, register auto-increment
  imuWrite(lsm6ds3::CTRL1_XL, 0x28);  // 26 Hz, +-4 g
  imuWrite(lsm6ds3::CTRL6_C, 0x10);   // low-power mode, about 14 uA
  imuWrite(lsm6ds3::CTRL2_G, 0x00);   // gyroscope off
  imuWrite(lsm6ds3::CTRL8_XL, 0x20);  // high-pass filter at ODR/100 for wake-up
  imuWrite(lsm6ds3::WAKE_UP_DUR, 0x00);
  imuWrite(lsm6ds3::WAKE_UP_THS, wakeSteps(WAKE_MG));
  imuWrite(lsm6ds3::TAP_CFG, 0x91);  // interrupts on, high-pass filter, latched
  imuWrite(lsm6ds3::MD1_CFG, 0x20);  // wake-up event on INT1
  imuRead(lsm6ds3::WAKE_UP_SRC);

  pinMode(PIN_LSM6DS3TR_C_INT1, INPUT);
  imuFound = true;
  return true;
}

float screenAxis(int8_t pick) {
  float value = imuMg[abs(pick) - 1];
  return pick < 0 ? -value : value;
}

// Turns one component of gravity (-1 to 1) into how far the eyes look that way.
float tiltAmount(float component) {
  float amount = (fabsf(component) - TILT_DEADZONE) / (TILT_FULL - TILT_DEADZONE);
  if (amount <= 0) {
    return 0;
  }
  if (amount > 1) {
    amount = 1;
  }
  return component < 0 ? -amount : amount;
}

Motion motionUpdate(uint32_t now) {
  Motion motion = {false, false, false, 0, 0, 0, 0};
  if (!imuFound) {
    return motion;
  }
  int src = imuRead(lsm6ds3::WAKE_UP_SRC);
  motion.moved = src >= 0 && (src & lsm6ds3::WU_IA);

  uint8_t raw[6];
  if (!imuRead(lsm6ds3::OUTX_L_XL, raw, sizeof(raw))) {
    return motion;
  }
  for (int i = 0; i < 3; i++) {
    // 0.122 mg per count at +-4 g.
    imuMg[i] = (int16_t)(raw[2 * i] | raw[2 * i + 1] << 8) * 122 / 1000;
  }
  float reading[3] = {screenAxis(SCREEN_RIGHT), screenAxis(SCREEN_UP), screenAxis(SCREEN_OUT)};
  float moving[3];
  for (int i = 0; i < 3; i++) {
    if (!haveGravity) {
      gravity[i] = reading[i];
    }
    gravity[i] += (reading[i] - gravity[i]) * 0.2f;
    moving[i] = reading[i] - gravity[i];
  }
  haveGravity = true;
  motion.pushX = moving[0] / 1000.0f;
  motion.pushY = moving[1] / 1000.0f;

  // Shaking fills the meter one step per reading and pauses drain it at half that.
  float jolt = sqrtf(moving[0] * moving[0] + moving[1] * moving[1] + moving[2] * moving[2]);
  float full = (float)DIZZY_SHAKE_MS / MOTION_POLL_MS;
  if (jolt > SHAKE_MG) {
    shakeLevel = fminf(shakeLevel + 1, full + 4);
    shakenOnce = true;
    lastShakeAt = now;
  } else {
    shakeLevel = fmaxf(shakeLevel - 0.5f, 0);
  }
  motion.shaking = shakenOnce && now - lastShakeAt < 300;
  motion.dizzy = shakeLevel >= full;

  float size = sqrtf(gravity[0] * gravity[0] + gravity[1] * gravity[1] + gravity[2] * gravity[2]);
  if (size > 300) {
    float dir[3] = {gravity[0] / size, gravity[1] / size, gravity[2] / size};
    motion.tiltX = tiltAmount(dir[0]);
    motion.tiltY = tiltAmount(dir[2]);
    // Tilting by more than about 12 degrees counts as activity, so playing with the
    // gaze keeps it awake. Slow tilts never set off the wake-up interrupt.
    if (dir[0] * tiltRef[0] + dir[1] * tiltRef[1] + dir[2] * tiltRef[2] < 0.978f) {
      motion.moved = true;
      for (int i = 0; i < 3; i++) {
        tiltRef[i] = dir[i];
      }
    }
  }
  return motion;
}

// Clears the latch so INT1 starts low, and reports whether the IMU can wake the board.
bool motionArmWake() {
  if (!imuFound) {
    return false;
  }
  imuRead(lsm6ds3::WAKE_UP_SRC);
  return true;
}
