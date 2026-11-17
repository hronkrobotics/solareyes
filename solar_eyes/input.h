#pragma once
#include "config.h"

// The momentary button, between PIN_BUTTON and GND, using the internal pull-up.

bool buttonDown = false;
uint32_t buttonEdgeAt = 0;

void inputBegin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  // A press that woke the board can still be held here. Only the next press counts.
  buttonDown = digitalRead(PIN_BUTTON) == LOW;
}

// True once for each press, after debouncing.
bool buttonPressed(uint32_t now) {
  bool down = digitalRead(PIN_BUTTON) == LOW;
  if (down == buttonDown || now - buttonEdgeAt < DEBOUNCE_MS) {
    return false;
  }
  buttonDown = down;
  buttonEdgeAt = now;
  return down;
}
