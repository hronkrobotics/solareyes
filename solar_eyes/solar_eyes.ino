// Solar Eyes v2
// Animated eyes on a 0.96" SSD1306 OLED, run by a Seeed XIAO ESP32-S3 from a LiPo.
// Short press: next mood. Shake it for about 3 seconds: it gets dizzy.
// No button and no movement for SLEEP_AFTER_MS: the eyes close and it goes to deep
// sleep. The button, or picking it up, wakes it again.

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

void setup() {
  Serial.begin(115200);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_HZ);
  Wire.setTimeOut(50);

  displayBegin();
  inputBegin();
  hasAccel = motionBegin();

  uint32_t now = millis();
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

  if (buttonPressed(now)) {
    if (faceAwake()) {
      savedMood = faceNextMood(now);
    } else {
      wake(now);
    }
    noteActivity(now);
  } else if (buttonDown) {
    // A held button would wake it straight back up, so it counts as activity.
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
      deepSleep(motionArmWake());
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
