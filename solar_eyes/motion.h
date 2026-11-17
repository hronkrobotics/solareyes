#pragma once
#include <Wire.h>
#include "config.h"

// Shake detection with the LSM6DS3TR-C IMU built into the XIAO nRF52840 Sense. It sits on
// the board's internal I2C bus (Wire1) and is powered from a pin that stays on during
// System OFF, so it can wake the board:
// - Its wake-up interrupt watches high-pass filtered data, so gravity and slow tilting
//   are ignored. Past WAKE_MG it drives INT1 high and holds it there until WAKE_UP_SRC is
//   read. That pin wakes the board, and while awake any movement counts as activity.
// - This chip has one motion threshold, so hard shakes are spotted in software: the
//   reading jumps by more than SHAKE_MG between polls.
// The gyroscope stays off. If the IMU does not answer, motion is skipped and only the
// buttons wake the board. To use a different accelerometer, only this file has to change.

struct Motion {
  bool moved;          // above WAKE_MG since the last update
  bool shaking;        // hard shakes with no pause longer than SHAKE_GAP_MS
  uint32_t shakingMs;  // how long the current shake has lasted
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
bool havePrev = false;
int32_t prevX = 0;
int32_t prevY = 0;
int32_t prevZ = 0;
bool shaking = false;
uint32_t shakeStart = 0;
uint32_t lastShake = 0;

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

Motion motionUpdate(uint32_t now) {
  Motion motion = {false, false, 0};
  if (!imuFound) {
    return motion;
  }
  int src = imuRead(lsm6ds3::WAKE_UP_SRC);
  motion.moved = src >= 0 && (src & lsm6ds3::WU_IA);

  uint8_t raw[6];
  if (imuRead(lsm6ds3::OUTX_L_XL, raw, sizeof(raw))) {
    // 0.122 mg per count at +-4 g.
    int32_t x = (int16_t)(raw[0] | raw[1] << 8) * 122 / 1000;
    int32_t y = (int16_t)(raw[2] | raw[3] << 8) * 122 / 1000;
    int32_t z = (int16_t)(raw[4] | raw[5] << 8) * 122 / 1000;
    if (havePrev) {
      int32_t dx = x - prevX;
      int32_t dy = y - prevY;
      int32_t dz = z - prevZ;
      int32_t limit = (int32_t)SHAKE_MG * SHAKE_MG;
      if (dx * dx + dy * dy + dz * dz > limit) {
        if (!shaking) {
          shaking = true;
          shakeStart = now;
        }
        lastShake = now;
      } else if (shaking && now - lastShake > SHAKE_GAP_MS) {
        shaking = false;
      }
    }
    prevX = x;
    prevY = y;
    prevZ = z;
    havePrev = true;
  }

  motion.shaking = shaking;
  motion.shakingMs = shaking ? now - shakeStart : 0;
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
