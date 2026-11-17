#pragma once
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "config.h"

using namespace Adafruit_LittleFS_Namespace;

// The inactivity timer, saving the mood, and System OFF, the nRF52840's deepest sleep.
// There is no power switch: System OFF is the off state. Waking resets the chip, so the
// sketch starts again from setup(), and only what is in flash survives.

constexpr const char *MOOD_FILE = "/mood";

uint32_t lastActivity = 0;
uint8_t storedMood = 0;
bool wokeByImu = false;
bool wokeByButton = false;

uint32_t pinLatch(uint8_t pin) {
  uint32_t gpio = g_ADigitalPinMap[pin];
  uint32_t latch = gpio < 32 ? NRF_P0->LATCH : NRF_P1->LATCH;
  return (latch >> (gpio & 31)) & 1;
}

void powerBegin() {
  // Seeed warns that P0.14 high while the battery charges can push the battery-sense
  // pin past its voltage limit. Low costs about 3 uA through the divider.
  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, LOW);

  // The GPIO latch still shows which pin woke the chip from System OFF.
  wokeByImu = pinLatch(PIN_LSM6DS3TR_C_INT1);
  wokeByButton = pinLatch(PIN_BUTTON_NEXT) || pinLatch(PIN_BUTTON_PREV);
  NRF_P0->LATCH = 0xFFFFFFFF;
  NRF_P1->LATCH = 0xFFFFFFFF;
}

void noteActivity(uint32_t now) {
  lastActivity = now;
}

bool idleTooLong(uint32_t now) {
  return SLEEP_AFTER_MS && now - lastActivity >= SLEEP_AFTER_MS;
}

bool usbPowered() {
  return NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk;
}

bool wokeFromSleep() {
  return readResetReason() & (POWER_RESETREAS_OFF_Msk | POWER_RESETREAS_VBUS_Msk);
}

const char *wakeReason() {
  uint32_t reason = readResetReason();
  if (reason & POWER_RESETREAS_VBUS_Msk) {
    return "USB plugged in";
  }
  if (reason & POWER_RESETREAS_OFF_Msk) {
    if (wokeByImu) {
      return "a shake";
    }
    return wokeByButton ? "a button" : "a shake or a button";
  }
  return (reason & POWER_RESETREAS_RESETPIN_Msk) ? "the reset button" : "power on";
}

uint8_t loadMood() {
  InternalFS.begin();
  File file(InternalFS);
  if (file.open(MOOD_FILE, FILE_O_READ)) {
    file.read(&storedMood, 1);
    file.close();
  }
  return storedMood;
}

// Only writes flash when the mood changed since the last save.
void saveMood(uint8_t mood) {
  if (mood == storedMood) {
    return;
  }
  InternalFS.remove(MOOD_FILE);
  File file(InternalFS);
  if (file.open(MOOD_FILE, FILE_O_WRITE)) {
    file.write(&mood, 1);
    file.close();
    storedMood = mood;
  }
}

// Wakes the chip from System OFF when the pin reaches this level. A pin already at that
// level would wake it straight back up, so leave those out.
void wakeOn(uint8_t pin, bool high) {
  nrf_gpio_cfg_sense_input(g_ADigitalPinMap[pin],
                           high ? NRF_GPIO_PIN_NOPULL : NRF_GPIO_PIN_PULLUP,
                           high ? NRF_GPIO_PIN_SENSE_HIGH : NRF_GPIO_PIN_SENSE_LOW);
}

// Turn the OLED off and arm the wake-up pins first. Output pins, like the IMU's power,
// hold their level while the chip is off.
void systemOff() {
  NRF_P0->LATCH = 0xFFFFFFFF;
  NRF_P1->LATCH = 0xFFFFFFFF;
  uint8_t softDevice = 0;
  sd_softdevice_is_enabled(&softDevice);
  if (softDevice) {
    sd_power_system_off();
  } else {
    NRF_POWER->SYSTEMOFF = 1;
  }
  while (true) {
  }
}
