#pragma once
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config.h"

// The inactivity timer and deep sleep. Waking from deep sleep restarts the sketch from
// setup(), so nothing here returns once the board is asleep.

uint32_t lastActivity = 0;

void noteActivity(uint32_t now) {
  lastActivity = now;
}

bool idleTooLong(uint32_t now) {
  return SLEEP_AFTER_MS && now - lastActivity >= SLEEP_AFTER_MS;
}

bool wokeFromSleep() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  return cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1;
}

const char *wakeReason() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "button";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "movement";
    default:
      return "power on";
  }
}

// Turn the OLED off first. The button always wakes the board. Movement wakes it too
// when an accelerometer is fitted, since its INT1 pin stays high once triggered.
void deepSleep(bool wakeOnMotion) {
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON, 0);
  // ext0 keeps the RTC domain powered, so this pull-up holds the button high in sleep.
  rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTON);
  if (wakeOnMotion) {
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_ACCEL_INT, ESP_EXT1_WAKEUP_ANY_HIGH);
  }
  Serial.println("deep sleep");
  Serial.flush();
  esp_deep_sleep_start();
}
