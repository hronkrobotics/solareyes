#pragma once
#include "config.h"

// The two momentary buttons, each between its pin and GND, using the internal pull-ups.

struct Button {
  uint8_t pin;
  bool down;
  uint32_t edgeAt;
  uint32_t downSince;
};

Button nextButton = {PIN_BUTTON_NEXT, false, 0, 0};
Button prevButton = {PIN_BUTTON_PREV, false, 0, 0};

void buttonBegin(Button &button, uint32_t now) {
  pinMode(button.pin, INPUT_PULLUP);
  // A press that woke the board can still be held here. Only the next press counts.
  button.down = digitalRead(button.pin) == LOW;
  button.downSince = now;
}

void inputBegin(uint32_t now) {
  buttonBegin(nextButton, now);
  buttonBegin(prevButton, now);
}

// True once for each press, after debouncing.
bool buttonPressed(Button &button, uint32_t now) {
  bool down = digitalRead(button.pin) == LOW;
  if (down == button.down || now - button.edgeAt < DEBOUNCE_MS) {
    return false;
  }
  button.down = down;
  button.edgeAt = now;
  if (down) {
    button.downSince = now;
  }
  return down;
}

// Down, but not for so long that it looks stuck.
bool buttonHeld(const Button &button, uint32_t now) {
  return button.down && now - button.downSince < STUCK_BUTTON_MS;
}
