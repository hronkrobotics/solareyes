#pragma once
#include <Wire.h>
#include "config.h"

// Shake detection with an LIS3DH accelerometer on the shared I2C bus. An LIS2DH12
// uses the same registers and works too. Both of its interrupt generators watch
// high-pass filtered data, so gravity and slow tilting are ignored:
// - Generator 1 has the low WAKE_MG threshold and drives the INT1 pin. That pin wakes
//   the board from deep sleep, and while awake any movement counts as activity.
// - Generator 2 has the high SHAKE_MG threshold and is only read over I2C.
// Both are latched, so each reading reports anything that happened since the last one.
// If no accelerometer answers, motion is skipped and only the button wakes the board.
// To use a different accelerometer, only this file has to change.

struct Motion {
  bool moved;          // above WAKE_MG since the last update
  bool shaking;        // above SHAKE_MG, with no pause longer than SHAKE_GAP_MS
  uint32_t shakingMs;  // how long the current shake has lasted
};

namespace lis3dh {
constexpr uint8_t WHO_AM_I = 0x0F;
constexpr uint8_t CTRL_REG1 = 0x20;
constexpr uint8_t CTRL_REG2 = 0x21;
constexpr uint8_t CTRL_REG3 = 0x22;
constexpr uint8_t CTRL_REG4 = 0x23;
constexpr uint8_t CTRL_REG5 = 0x24;
constexpr uint8_t REFERENCE = 0x26;
constexpr uint8_t INT1_CFG = 0x30;
constexpr uint8_t INT1_SRC = 0x31;
constexpr uint8_t INT1_THS = 0x32;
constexpr uint8_t INT1_DURATION = 0x33;
constexpr uint8_t INT2_CFG = 0x34;
constexpr uint8_t INT2_SRC = 0x35;
constexpr uint8_t INT2_THS = 0x36;
constexpr uint8_t INT2_DURATION = 0x37;
constexpr uint8_t ID = 0x33;
constexpr uint8_t ACTIVE = 0x40;  // IA bit in INTx_SRC
}  // namespace lis3dh

uint8_t accelAddr = 0;
bool shaking = false;
uint32_t shakeStart = 0;
uint32_t lastShake = 0;

bool accelWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(accelAddr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

int accelRead(uint8_t reg) {
  Wire.beginTransmission(accelAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return -1;
  }
  if (Wire.requestFrom((int)accelAddr, 1) != 1) {
    return -1;
  }
  return Wire.read();
}

bool accelEvent(uint8_t srcReg) {
  int src = accelRead(srcReg);
  return src >= 0 && (src & lis3dh::ACTIVE);
}

// At +-4 g, one threshold step is 32 mg.
uint8_t thresholdSteps(uint16_t mg) {
  uint16_t steps = (mg + 16) / 32;
  if (steps < 1) {
    steps = 1;
  }
  if (steps > 127) {
    steps = 127;
  }
  return steps;
}

bool motionBegin() {
  // SDO/SA0 low gives 0x18, high gives 0x19.
  const uint8_t addrs[] = {0x18, 0x19};
  for (uint8_t addr : addrs) {
    accelAddr = addr;
    if (accelRead(lis3dh::WHO_AM_I) == lis3dh::ID) {
      break;
    }
    accelAddr = 0;
  }
  if (!accelAddr) {
    return false;
  }

  accelWrite(lis3dh::CTRL_REG1, 0x3F);  // 25 Hz low-power mode, X/Y/Z on, about 4 uA
  accelWrite(lis3dh::CTRL_REG2, 0x03);  // high-pass filter on both interrupt generators
  accelWrite(lis3dh::CTRL_REG3, 0x40);  // generator 1 drives INT1
  accelWrite(lis3dh::CTRL_REG4, 0x10);  // +-4 g
  accelWrite(lis3dh::CTRL_REG5, 0x0A);  // latch both until their source register is read
  accelWrite(lis3dh::INT1_THS, thresholdSteps(WAKE_MG));
  accelWrite(lis3dh::INT1_DURATION, 0);
  accelWrite(lis3dh::INT2_THS, thresholdSteps(SHAKE_MG));
  accelWrite(lis3dh::INT2_DURATION, 0);
  accelRead(lis3dh::REFERENCE);  // start the filter from the current reading
  accelWrite(lis3dh::INT1_CFG, 0x2A);  // any axis above the threshold
  accelWrite(lis3dh::INT2_CFG, 0x2A);
  accelRead(lis3dh::INT1_SRC);
  accelRead(lis3dh::INT2_SRC);

  pinMode(PIN_ACCEL_INT, INPUT);
  return true;
}

Motion motionUpdate(uint32_t now) {
  Motion motion = {false, false, 0};
  if (!accelAddr) {
    return motion;
  }
  motion.moved = accelEvent(lis3dh::INT1_SRC);
  if (accelEvent(lis3dh::INT2_SRC)) {
    if (!shaking) {
      shaking = true;
      shakeStart = now;
    }
    lastShake = now;
  } else if (shaking && now - lastShake > SHAKE_GAP_MS) {
    shaking = false;
  }
  motion.shaking = shaking;
  motion.shakingMs = shaking ? now - shakeStart : 0;
  return motion;
}

// Clears both latches so INT1 starts low, and reports whether it can wake the board.
bool motionArmWake() {
  if (!accelAddr) {
    return false;
  }
  accelRead(lis3dh::INT1_SRC);
  accelRead(lis3dh::INT2_SRC);
  return true;
}
