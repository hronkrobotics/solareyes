#pragma once
#include <Arduino.h>

// Pins on the XIAO nRF52840 Sense, as Arduino pin numbers. The XIAO's default I2C pins
// are D4/D5. This build puts the OLED on D9/D10, so the sketch sets them explicitly.
// The accelerometer is the IMU built into the board, on its own internal I2C bus
// (Wire1, PIN_LSM6DS3TR_C_POWER and PIN_LSM6DS3TR_C_INT1 in the board's variant).
// "Right" and "left" are seen from the front.
constexpr uint8_t PIN_SDA = D9;
constexpr uint8_t PIN_SCL = D10;
constexpr uint8_t PIN_BUTTON_NEXT = D0;  // right-hand button to GND: next mood
constexpr uint8_t PIN_BUTTON_PREV = D3;  // left-hand button to GND: previous mood

constexpr uint8_t OLED_ADDR = 0x3C;

// The v1 panel did not respond at the library's default 400 kHz. The eyes move one
// easing step per frame, so a faster bus also makes them move faster.
constexpr uint32_t I2C_HZ = 100000;

// No button press and no movement for this long: the eyes close and the OLED turns
// off. 0 keeps it awake.
constexpr uint32_t SLEEP_AFTER_MS = 60000;

// true: on battery, System OFF once the OLED is off, and wake on a shake or either
// button. On USB power it only turns the OLED off, so the serial port stays up.
// false: only the OLED turns off, on battery too.
constexpr bool DEEP_SLEEP = true;

// A button held down longer than this, say pressed against something in a pocket,
// stops keeping the keychain awake and is left out of the wake-up pins.
constexpr uint32_t STUCK_BUTTON_MS = 10000;

// Accelerometer thresholds in milli-g.
constexpr uint16_t WAKE_MG = 250;    // shake to wake, with gravity filtered out
constexpr uint16_t SHAKE_MG = 1200;  // a hard shake: the reading jumps this much between polls

// Shaking for DIZZY_AFTER_MS makes the eyes dizzy, and they stay dizzy for
// DIZZY_FOR_MS after the shaking stops. A pause longer than SHAKE_GAP_MS ends a shake.
constexpr uint32_t DIZZY_AFTER_MS = 3000;
constexpr uint32_t DIZZY_FOR_MS = 4000;
constexpr uint32_t SHAKE_GAP_MS = 500;

constexpr uint16_t FRAME_MS = 33;
constexpr uint16_t DEBOUNCE_MS = 30;
constexpr uint16_t MOTION_POLL_MS = 50;
