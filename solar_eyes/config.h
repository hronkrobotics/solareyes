#pragma once
#include <Arduino.h>

// Pins on the XIAO ESP32-S3, as GPIO numbers. The XIAO's default I2C pins are D4/D5.
// This build uses D9/D10, so the sketch sets them explicitly. The OLED and the
// accelerometer share the bus.
constexpr uint8_t PIN_SDA = 8;        // D9
constexpr uint8_t PIN_SCL = 9;        // D10
constexpr uint8_t PIN_BUTTON = 1;     // D0, momentary button to GND
constexpr uint8_t PIN_ACCEL_INT = 2;  // D1, accelerometer INT1

constexpr uint8_t OLED_ADDR = 0x3C;

// The v1 panel did not respond at the library's default 400 kHz. The eyes move one
// easing step per frame, so a faster bus also makes them move faster.
constexpr uint32_t I2C_HZ = 100000;

// No button press and no movement for this long: the eyes close and the OLED turns
// off. 0 keeps it awake.
constexpr uint32_t SLEEP_AFTER_MS = 60000;

// true: deep sleep once the OLED is off, and wake on the button or movement.
// false: only the OLED turns off. Handy while flashing, because the USB port
// disappears during deep sleep.
constexpr bool DEEP_SLEEP = true;

// Accelerometer thresholds in milli-g, with gravity filtered out.
constexpr uint16_t WAKE_MG = 250;    // picking up the keys
constexpr uint16_t SHAKE_MG = 1200;  // a hard shake

// Shaking for DIZZY_AFTER_MS makes the eyes dizzy, and they stay dizzy for
// DIZZY_FOR_MS after the shaking stops. A pause longer than SHAKE_GAP_MS ends a shake.
constexpr uint32_t DIZZY_AFTER_MS = 3000;
constexpr uint32_t DIZZY_FOR_MS = 4000;
constexpr uint32_t SHAKE_GAP_MS = 500;

constexpr uint16_t FRAME_MS = 33;
constexpr uint16_t DEBOUNCE_MS = 30;
constexpr uint16_t MOTION_POLL_MS = 50;
