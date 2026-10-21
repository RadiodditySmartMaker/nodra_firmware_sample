#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <bluefruit.h>
#include <elink_gdem0213b74.h>

namespace {

constexpr char kDeviceName[] = "Nodara-BLE-Demo";

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

void start_advertising()
{
    Bluefruit.Advertising.clearData();
    Bluefruit.ScanResponse.clearData();
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addName();
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

    pinMode(PIN_LED_GREEN, OUTPUT); // Physical blue LED.
    digitalWrite(PIN_LED_GREEN, HIGH);

    Bluefruit.configPrphBandwidth(BANDWIDTH_NORMAL);
    if (!Bluefruit.begin(1, 0)) {
        Serial.println("ERROR: Bluefruit.begin failed");
        halt_with_error();
    }
    Bluefruit.setTxPower(0);
    Bluefruit.setName(kDeviceName);
    start_advertising();

    digitalWrite(PIN_LED_GREEN, LOW);
    elink_gdem0213b74_show_text("BLE ADVERTISING\nNodara-BLE-Demo\nScan in nRF Connect");
    Serial.println("BLE advertising started: Nodara-BLE-Demo");
}

void loop()
{
    // The Bluefruit stack handles BLE advertising in the background.
    delay(1000);
}
