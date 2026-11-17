#pragma once
#include "config.h"

// The two momentary buttons. Each signal leg uses the internal pull-up. Both other legs
// share D6, which is driven low and stands in for ground.

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
  // Pad 6 on the back of the XIAO is the button ground. High drive keeps it near 0 V
  // with both buttons pressed, and the level is held through System OFF so a press
  // can still wake the chip.
  uint32_t ground = g_ADigitalPinMap[PIN_BUTTON_GND];
  nrf_gpio_cfg(ground, NRF_GPIO_PIN_DIR_OUTPUT, NRF_GPIO_PIN_INPUT_DISCONNECT,
               NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_H0H1, NRF_GPIO_PIN_NOSENSE);
  nrf_gpio_pin_clear(ground);

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
