#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <bluefruit.h>
#include <elink_gdem0213b74.h>

namespace {

constexpr char kDeviceName[] = "Nodara-Eink-Text";
constexpr size_t kTextCapacity = 128;
constexpr uint32_t kTextCompleteDelayMs = 300;

BLEUart bleuart;
char text_buffer[kTextCapacity];
size_t text_length;
uint32_t last_text_byte_at;
bool display_requested;

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

void on_uart_rx(uint16_t connection_handle)
{
    (void)connection_handle;

    char received[48];
    int count;
    while ((count = bleuart.read(received, sizeof(received))) > 0) {
        for (int i = 0; i < count; i++) {
            const char character = received[i];
            if (character == '\r') {
                continue;
            }
            if (character == '\n') {
                display_requested = true;
                continue;
            }
            if (character >= 0x20 && character <= 0x7e && text_length < kTextCapacity - 1) {
                text_buffer[text_length++] = character;
            }
        }
    }

    last_text_byte_at = millis();
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

void display_received_text()
{
    text_buffer[text_length] = '\0';
    elink_gdem0213b74_show_text(text_buffer);

    if (Bluefruit.connected()) {
        bleuart.println("OK: text displayed");
    }

    text_length = 0;
    display_requested = false;
}

} // namespace

void setup()
{
    Serial.begin(115200);

    elink_gdem0213b74_setup();
    (void)elink_gdem0213b74_start();

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

    elink_gdem0213b74_show_text("BLE text ready\nSend ASCII text");
    Serial.println("BLE e-ink text ready: Nodara-Eink-Text");
}

void loop()
{
    if (text_length > 0 &&
        (display_requested || (uint32_t)(millis() - last_text_byte_at) >= kTextCompleteDelayMs)) {
        display_received_text();
    }

    delay(10);
}
