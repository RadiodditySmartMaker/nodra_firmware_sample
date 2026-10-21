#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <bluefruit.h>
#include <ctype.h>
#include <elink_gdem0213b74.h>
#include <string.h>

namespace {

constexpr char kDeviceName[] = "Nodara-LED-Control";
constexpr uint8_t kActualBlueLedPin = PIN_LED_GREEN;
constexpr uint8_t kAdvertisingStatusLedPin = PIN_LED_BLUE;
BLEUart bleuart;
bool blue_led_is_on;
bool display_led_state_pending;

[[noreturn]] void halt_with_error()
{
    elink_gdem0213b74_show_text("BLE init failed");
    pinMode(PIN_LED_RED, OUTPUT);
    while (true) {
        digitalWrite(PIN_LED_RED, LOW);
        delay(200);
        digitalWrite(PIN_LED_RED, HIGH);
        delay(200);
    }
}

void set_blue_led(bool on)
{
    blue_led_is_on = on;
    digitalWrite(kActualBlueLedPin, on ? LOW : HIGH);
}

void display_led_state()
{
    elink_gdem0213b74_show_text(blue_led_is_on ? "BLUE LED ON" : "BLUE LED OFF");
    display_led_state_pending = false;
}

void send_reply(const char *message)
{
    if (Bluefruit.connected()) {
        bleuart.println(message);
    }
    Serial.println(message);
}

void on_uart_rx(uint16_t connection_handle)
{
    (void)connection_handle;

    char received[32];
    const int count = bleuart.read(received, sizeof(received) - 1);
    if (count <= 0) {
        return;
    }

    char command[sizeof(received)] = {};
    size_t command_length = 0;
    for (int i = 0; i < count && command_length < sizeof(command) - 1; i++) {
        const unsigned char character = (unsigned char)received[i];
        if (!isspace(character)) {
            command[command_length++] = (char)tolower(character);
        }
    }

    if (strcmp(command, "on") == 0) {
        set_blue_led(true);
        display_led_state_pending = true;
        send_reply("OK: BLUE LED ON");
    } else if (strcmp(command, "off") == 0) {
        set_blue_led(false);
        display_led_state_pending = true;
        send_reply("OK: BLUE LED OFF");
    } else {
        send_reply("ERROR: send on or off");
    }
}

void start_advertising()
{
    Bluefruit.Advertising.clearData();
    Bluefruit.ScanResponse.clearData();
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleuart);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
}

} // namespace

void setup()
{
    Serial.begin(115200);

    elink_gdem0213b74_setup();
    (void)elink_gdem0213b74_start();

    pinMode(kActualBlueLedPin, OUTPUT);
    set_blue_led(false);
    pinMode(kAdvertisingStatusLedPin, OUTPUT);
    digitalWrite(kAdvertisingStatusLedPin, HIGH);

    Bluefruit.configPrphBandwidth(BANDWIDTH_NORMAL);
    if (!Bluefruit.begin(1, 0)) {
        Serial.println("ERROR: Bluefruit.begin failed");
        halt_with_error();
    }
    Bluefruit.setTxPower(0);
    Bluefruit.setName(kDeviceName);
    bleuart.setRxCallback(on_uart_rx);
    bleuart.begin();
    start_advertising();

    digitalWrite(kAdvertisingStatusLedPin, LOW);
    elink_gdem0213b74_show_text("BLE LED CONTROL\nNodara-LED-Control\nSend: on / off");
    Serial.println("BLE LED control ready: Nodara-LED-Control");
}

void loop()
{
    if (display_led_state_pending) {
        display_led_state();
    }

    delay(10);
}
