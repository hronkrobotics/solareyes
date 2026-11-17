#pragma once
#include <Arduino.h>

// Pins on the XIAO nRF52840 Sense, as Arduino pin numbers. The XIAO's default I2C pins
// are D4/D5. This build puts the OLED on D9/D10, so the sketch sets them explicitly.
// The accelerometer is the IMU built into the board, on its own internal I2C bus
// (Wire1, PIN_LSM6DS3TR_C_POWER and PIN_LSM6DS3TR_C_INT1 in the board's variant).
// "Right" and "left" are seen from the front.
constexpr uint8_t PIN_SDA = D9;
constexpr uint8_t PIN_SCL = D10;
constexpr uint8_t PIN_BUTTON_NEXT = D0;  // right-hand button: next mood
constexpr uint8_t PIN_BUTTON_PREV = D1;  // left-hand button: previous mood
constexpr uint8_t PIN_BUTTON_GND = D6;   // shared other leg of both buttons, driven low

constexpr uint8_t OLED_ADDR = 0x3C;

// The v1 panel did not respond at the library's default 400 kHz. The eyes move one
// easing step per frame, so a faster bus also makes them move faster.
constexpr uint32_t I2C_HZ = 100000;

// No button press, no movement, and no change in tilt for this long: the eyes close and
// the OLED turns off. 0 keeps it awake.
constexpr uint32_t SLEEP_AFTER_MS = 5000;

// true: on battery, System OFF once the OLED is off, and wake on a shake or either
// button. On USB power it only turns the OLED off, so the serial port stays up.
// false: only the OLED turns off, on battery too.
constexpr bool DEEP_SLEEP = true;

// A button held down longer than this, say pressed against something in a pocket,
// stops keeping the keychain awake and is left out of the wake-up pins.
constexpr uint32_t STUCK_BUTTON_MS = 10000;

// How the IMU sits relative to the screen: which IMU axis points to the screen's right,
// which points up, and which points out of the screen toward you. 1 = X, 2 = Y, 3 = Z,
// negative to flip. Send 'a' in the serial monitor to print the readings and check.
constexpr int8_t SCREEN_RIGHT = 1;
constexpr int8_t SCREEN_UP = 2;
constexpr int8_t SCREEN_OUT = 3;

// Tilting makes the eyes look the other way. Below TILT_DEADZONE (as a fraction of 1 g,
// about 10 degrees) they stay put, and by TILT_FULL (about 40 degrees) they look all the
// way over.
constexpr float TILT_DEADZONE = 0.17f;
constexpr float TILT_FULL = 0.65f;

// Accelerometer thresholds in milli-g.
constexpr uint16_t WAKE_MG = 250;   // shake to wake, with gravity filtered out
constexpr uint16_t SHAKE_MG = 600;  // a reading this far from gravity counts as shaking

// About DIZZY_SHAKE_MS of solid shaking makes the eyes dizzy. Pauses drain it at half the
// rate, so short gaps do not start it over. They stay dizzy for DIZZY_FOR_MS after the
// shaking stops.
constexpr uint32_t DIZZY_SHAKE_MS = 1000;
constexpr uint32_t DIZZY_FOR_MS = 3000;

constexpr uint16_t FRAME_MS = 33;
constexpr uint16_t DEBOUNCE_MS = 30;
constexpr uint16_t MOTION_POLL_MS = 50;
