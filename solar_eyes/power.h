#pragma once
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config.h"

// The inactivity timer and deep sleep. There is no power switch: deep sleep is the off
// state. Waking restarts the sketch from setup(), so nothing here returns once asleep.

uint32_t lastActivity = 0;

void noteActivity(uint32_t now) {
  lastActivity = now;
}

bool idleTooLong(uint32_t now) {
  return SLEEP_AFTER_MS && now - lastActivity >= SLEEP_AFTER_MS;
}

bool wokeFromSleep() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
}

const char *wakeReason() {
  if (!wokeFromSleep()) {
    return "power on";
  }
  return (esp_sleep_get_ext1_wakeup_status() & (1ULL << PIN_ACCEL_INT)) ? "a shake" : "a button";
}

// Turn the OLED off first. Every wake-up pin is active low, so they all share ext1.
// The buttons rely on their RTC pull-ups, which only work with the RTC domain kept on.
void deepSleep(uint64_t wakePins) {
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  const uint8_t buttons[] = {PIN_BUTTON_NEXT, PIN_BUTTON_PREV};
  for (uint8_t pin : buttons) {
    rtc_gpio_pullup_en((gpio_num_t)pin);
    rtc_gpio_pulldown_dis((gpio_num_t)pin);
  }
  if (wakePins) {
    esp_sleep_enable_ext1_wakeup(wakePins, ESP_EXT1_WAKEUP_ANY_LOW);
  }
  Serial.println("deep sleep");
  Serial.flush();
  esp_deep_sleep_start();
}
