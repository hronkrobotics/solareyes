// Solar Eyes v2
// Animated eyes on a 0.96" SSD1306 OLED, run by a Seeed XIAO nRF52840 Sense from a LiPo.
// Right button: next mood. Left button: previous mood. Shake it for about 3 seconds:
// it gets dizzy. With no button and no movement for SLEEP_AFTER_MS, the eyes close and
// it goes to sleep. A shake wakes it again, and so does either button.

#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include "config.h"
#include "display.h"
#include "animations.h"
#include "input.h"
#include "motion.h"
#include "power.h"

uint32_t nextFrame = 0;
uint32_t nextMotionPoll = 0;
bool serialWasOpen = false;

// Seeds random() from the nRF52840's hardware random number generator.
uint32_t hardwareSeed() {
  NRF_RNG->CONFIG = RNG_CONFIG_DERCEN_Msk;
  NRF_RNG->TASKS_START = 1;
  uint32_t seed = 0;
  for (int i = 0; i < 4; i++) {
    NRF_RNG->EVENTS_VALRDY = 0;
    while (!NRF_RNG->EVENTS_VALRDY) {
    }
    seed = (seed << 8) | NRF_RNG->VALUE;
  }
  NRF_RNG->TASKS_STOP = 1;
  return seed;
}

// The native USB port drops anything printed before a serial monitor opens it, so this
// runs each time one connects.
void report() {
  Serial.println("Solar Eyes v2 on a XIAO nRF52840 Sense");
  Serial.print("I2C devices on D9 (SDA) and D10 (SCL):");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
    }
  }
  Serial.println();
  Serial.println(displayFound ? "OLED found at 0x3C"
                              : "no OLED at 0x3C: check SDA on D9 and SCL on D10");
  Serial.println(imuFound ? "IMU found (LSM6DS3TR-C)" : "IMU not found, button wake only");
  Serial.printf("woke by: %s, mood: %s, power: %s\n", wakeReason(),
                MOOD_NAMES[(uint8_t)mood], usbPowered() ? "USB" : "battery");
}

void wake(uint32_t now) {
  if (displayAsleep) {
    displayWake();
  }
  faceWake(now);
}

// A button that is down right now is left out, or it would wake the chip straight back up.
void goToSleep() {
  if (!nextButton.down) {
    wakeOn(PIN_BUTTON_NEXT, false);
  }
  if (!prevButton.down) {
    wakeOn(PIN_BUTTON_PREV, false);
  }
  if (motionArmWake()) {
    wakeOn(PIN_LSM6DS3TR_C_INT1, true);
  }
  saveMood((uint8_t)mood);
  systemOff();
}

void setup() {
  powerBegin();
  Serial.begin(115200);
  randomSeed(hardwareSeed());

  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.begin();
  Wire.setClock(I2C_HZ);

  uint32_t now = millis();
  displayBegin();
  inputBegin(now);
  motionBegin();

  faceBegin(now, loadMood(), wokeFromSleep());
  noteActivity(now);
  nextFrame = now;
}

void loop() {
  uint32_t now = millis();

  bool serialOpen = (bool)Serial;
  if (serialOpen && !serialWasOpen) {
    report();
  }
  serialWasOpen = serialOpen;

  bool next = buttonPressed(nextButton, now);
  bool prev = buttonPressed(prevButton, now);
  if (next || prev) {
    if (faceAwake()) {
      faceStepMood(now, next ? 1 : -1);
    } else {
      wake(now);
    }
    noteActivity(now);
  } else if (buttonHeld(nextButton, now) || buttonHeld(prevButton, now)) {
    noteActivity(now);
  }

  if (imuFound && (int32_t)(now - nextMotionPoll) >= 0) {
    nextMotionPoll = now + MOTION_POLL_MS;
    Motion motion = motionUpdate(now);
    if (motion.moved) {
      if (!faceAwake()) {
        wake(now);
      }
      noteActivity(now);
    }
    faceShake(now, motion.shaking, motion.shakingMs);
  }

  if (faceAwake() && idleTooLong(now)) {
    faceDoze(now);
  }
  faceUpdate(now);

  if (faceAsleep() && !displayAsleep && !displayBusy()) {
    displaySleep();
    if (DEEP_SLEEP && !usbPowered()) {
      goToSleep();
    }
  }

  if (displayAsleep) {
    delay(20);
    return;
  }
  if (displayBusy()) {
    displaySendPage();
    return;
  }
  if ((int32_t)(now - nextFrame) < 0) {
    delay(2);
    return;
  }
  nextFrame = now + FRAME_MS;
  faceDraw(now);
  displayStartFrame();
}
