// Solar Eyes v2
// Animated eyes on a 0.96" SSD1306 OLED, run by a Seeed XIAO ESP32-S3 from a LiPo.
// Right button: next mood. Left button: previous mood. Shake it for about 3 seconds:
// it gets dizzy. With no button and no movement for SLEEP_AFTER_MS, the eyes close
// and it goes to deep sleep. A shake wakes it again, and so does either button.

#include <Wire.h>
#include "config.h"
#include "display.h"
#include "animations.h"
#include "input.h"
#include "motion.h"
#include "power.h"

// RTC memory survives deep sleep, so it wakes up in the same mood.
RTC_DATA_ATTR uint8_t savedMood = 0;

bool hasAccel = false;
uint32_t nextFrame = 0;
uint32_t nextMotionPoll = 0;

void wake(uint32_t now) {
  if (displayAsleep) {
    displayWake();
  }
  faceWake(now);
}

// A button that is down right now is left out, or it would wake the board straight
// back up.
uint64_t wakePins() {
  uint64_t pins = 0;
  if (!nextButton.down) {
    pins |= 1ULL << PIN_BUTTON_NEXT;
  }
  if (!prevButton.down) {
    pins |= 1ULL << PIN_BUTTON_PREV;
  }
  if (motionArmWake()) {
    pins |= 1ULL << PIN_ACCEL_INT;
  }
  return pins;
}

void setup() {
  Serial.begin(115200);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_HZ);
  Wire.setTimeOut(50);

  uint32_t now = millis();
  displayBegin();
  inputBegin(now);
  hasAccel = motionBegin();

  faceBegin(now, savedMood, wokeFromSleep());
  noteActivity(now);
  nextFrame = now;

  Serial.println("Solar Eyes v2");
  Serial.println("SDA D9 (GPIO8), SCL D10 (GPIO9), SSD1306 128x64 @ 100 kHz");
  Serial.printf("woke by: %s, mood: %s\n", wakeReason(), MOOD_NAMES[(uint8_t)mood]);
  Serial.println(hasAccel ? "accelerometer found" : "no accelerometer, button wake only");
}

void loop() {
  uint32_t now = millis();

  bool next = buttonPressed(nextButton, now);
  bool prev = buttonPressed(prevButton, now);
  if (next || prev) {
    if (faceAwake()) {
      savedMood = faceStepMood(now, next ? 1 : -1);
    } else {
      wake(now);
    }
    noteActivity(now);
  } else if (buttonHeld(nextButton, now) || buttonHeld(prevButton, now)) {
    noteActivity(now);
  }

  if (hasAccel && (int32_t)(now - nextMotionPoll) >= 0) {
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
    if (DEEP_SLEEP) {
      deepSleep(wakePins());
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
    // Yield so the USB serial port stays responsive.
    delay(2);
    return;
  }
  nextFrame = now + FRAME_MS;
  faceDraw(now);
  displayStartFrame();
}
