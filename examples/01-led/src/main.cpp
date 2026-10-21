#include <Arduino.h>

#include "button.h"
#include "display.h"

namespace {

constexpr uint8_t kButtonUp = 0;
constexpr uint8_t kButtonLeft = 1;
constexpr uint8_t kButtonRight = 2;

struct led_choice_t {
    uint8_t button_index;
    uint8_t pin;
    const char *color;
};

/* The board's physical green and blue LEDs use the opposite legacy pin names. */
constexpr led_choice_t kLedChoices[] = {
    {kButtonLeft, PIN_LED_RED, "RED"},
    {kButtonRight, PIN_LED_BLUE, "GREEN"},
    {kButtonUp, PIN_LED_GREEN, "BLUE"},
};

void turn_all_leds_off()
{
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_BLUE, HIGH);
    digitalWrite(PIN_LED_GREEN, HIGH);
}

void show_welcome()
{
    display_show_text("WELCOME TO LED DEMO\nUP: BLUE\nLEFT: RED\nRIGHT: GREEN");
}

void select_led(const led_choice_t &choice)
{
    turn_all_leds_off();
    digitalWrite(choice.pin, LOW); /* LEDs are active low. */

    String message = String(choice.color) + " LED IS ON";
    display_show_text(message.c_str());
}

void on_button_release(uint8_t button_index)
{
    for (const auto &choice : kLedChoices) {
        if (choice.button_index == button_index) {
            select_led(choice);
            return;
        }
    }
}

} // namespace

void setup()
{
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    turn_all_leds_off();

    display_setup();
    button_setup();
    display_start();
    show_welcome();

    button_register_release_callback(kButtonUp, on_button_release);
    button_register_release_callback(kButtonLeft, on_button_release);
    button_register_release_callback(kButtonRight, on_button_release);
}

void loop()
{
    button_loop();
    delay(1);
}
