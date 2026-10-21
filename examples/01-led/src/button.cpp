#include "button.h"

#include <Arduino.h>
#include <variant.h>

namespace {

constexpr uint8_t kButtonCount = 6;
constexpr uint32_t kDebounceMs = 20;

constexpr uint8_t kButtonPins[kButtonCount] = {
    PIN_BTN_F1,
    PIN_BTN_F2,
    PIN_BTN_F3,
    PIN_BTN_F4,
    PIN_BTN_F5,
    PIN_BTN_F6,
};

button_release_callback_t callbacks[kButtonCount] = {};
bool raw_pressed[kButtonCount] = {};
bool stable_pressed[kButtonCount] = {};
uint32_t changed_at[kButtonCount] = {};

} // namespace

void button_setup()
{
    const uint32_t now = millis();
    for (uint8_t index = 0; index < kButtonCount; ++index) {
        pinMode(kButtonPins[index], INPUT_PULLUP);
        raw_pressed[index] = digitalRead(kButtonPins[index]) == LOW;
        stable_pressed[index] = raw_pressed[index];
        changed_at[index] = now;
    }
}

bool button_register_release_callback(uint8_t button_index, button_release_callback_t callback)
{
    if (button_index >= kButtonCount) {
        return false;
    }

    callbacks[button_index] = callback;
    return true;
}

void button_loop()
{
    const uint32_t now = millis();

    for (uint8_t index = 0; index < kButtonCount; ++index) {
        const bool pressed = digitalRead(kButtonPins[index]) == LOW;
        if (pressed != raw_pressed[index]) {
            raw_pressed[index] = pressed;
            changed_at[index] = now;
        }

        if (pressed == stable_pressed[index] || now - changed_at[index] < kDebounceMs) {
            continue;
        }

        stable_pressed[index] = pressed;
        if (!pressed && callbacks[index] != nullptr) {
            callbacks[index](index);
        }
    }
}
