#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <button.h>
#include <elink_gdem0213b74.h>
#include <lora_ra01s.h>
#include <stdio.h>
#include <string.h>

namespace {

struct button_message_t {
    uint8_t index;
    const char *direction;
    const char *message;
};

constexpr button_message_t kButtonMessages[] = {
    {0, "UP", "HELLO"},
    {1, "LEFT", "BYE"},
    {2, "RIGHT", "PING"},
    {3, "DOWN", "PONG"},
};
constexpr size_t kButtonMessageCount = sizeof(kButtonMessages) / sizeof(kButtonMessages[0]);

uint32_t send_count;

[[noreturn]] void halt_with_error(int error)
{
    Serial.print("ERROR: LoRa start failed: ");
    Serial.println(error);

    elink_gdem0213b74_show_text("LoRa init failed\nCheck serial log");

    pinMode(PIN_LED_RED, OUTPUT);
    while (true) {
        digitalWrite(PIN_LED_RED, LOW);
        delay(200);
        digitalWrite(PIN_LED_RED, HIGH);
        delay(200);
    }
}

const button_message_t *find_button_message(uint8_t button_index)
{
    for (size_t i = 0; i < kButtonMessageCount; i++) {
        if (kButtonMessages[i].index == button_index) {
            return &kButtonMessages[i];
        }
    }
    return nullptr;
}

void send_message(const button_message_t &binding)
{
    const uint32_t attempt = send_count + 1;
    const int result = lora_ra01s_send((const uint8_t *)binding.message, strlen(binding.message));

    if (result == 0) {
        send_count = attempt;
        Serial.print("TX #");
        Serial.print(attempt);
        Serial.print(": ");
        Serial.println(binding.message);
    } else {
        Serial.print("TX #");
        Serial.print(attempt);
        Serial.print(" failed: ");
        Serial.println(result);
    }
}

void on_button(uint8_t button_index, btn_event_t event, void *)
{
    if (event != BTN_EVENT_PRESS_UP) {
        return;
    }

    const button_message_t *binding = find_button_message(button_index);
    if (binding != nullptr) {
        send_message(*binding);
    }
}

void show_ready_screen()
{
    char text[96];
    snprintf(text, sizeof(text), "LoRa text sender\nREADY\n%s %s  %s %s\n%s %s %s %s", kButtonMessages[0].direction,
             kButtonMessages[0].message, kButtonMessages[1].direction, kButtonMessages[1].message,
             kButtonMessages[2].direction, kButtonMessages[2].message, kButtonMessages[3].direction,
             kButtonMessages[3].message);
    elink_gdem0213b74_show_text(text);
}

void print_button_help()
{
    Serial.print("Press ");
    for (size_t i = 0; i < kButtonMessageCount; i++) {
        if (i != 0) {
            Serial.print(", ");
        }
        Serial.print(kButtonMessages[i].direction);
        Serial.print('=');
        Serial.print(kButtonMessages[i].message);
    }
    Serial.println();
}

} // namespace

void setup()
{
    Serial.begin(115200);

    elink_gdem0213b74_setup();
    button_setup();
    elink_gdem0213b74_start();

    const int result = lora_ra01s_start();
    if (result != 0) {
        halt_with_error(result);
    }

    pinMode(PIN_LED_GREEN, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);

    show_ready_screen();
    Serial.println("LoRa text sender ready");
    print_button_help();

    for (size_t i = 0; i < kButtonMessageCount; i++) {
        (void)btn_register_cb(kButtonMessages[i].index, BTN_EVENT_PRESS_UP, on_button, nullptr);
    }
}

void loop()
{
    (void)button_loop();
    delay(1);
}
