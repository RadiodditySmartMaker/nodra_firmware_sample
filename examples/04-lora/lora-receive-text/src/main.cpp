#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <elink_gdem0213b74.h>
#include <lora_ra01s.h>
#include <lora_sx126x_regs.h>
#include <lora_sx126x_shell.h>

namespace {

constexpr size_t kRxBufferSize = 64;
constexpr uint32_t kRxTimeoutMs = 5500;

uint32_t rx_count;
bool first_rx_shown;

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

} // namespace

void setup()
{
    Serial.begin(115200);

    elink_gdem0213b74_setup();
    elink_gdem0213b74_start();

    const int result = lora_ra01s_start();
    if (result != 0) {
        halt_with_error(result);
    }

    pinMode(PIN_LED_GREEN, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);

    elink_gdem0213b74_show_text("LoRa text receiver\nREADY\nWaiting: HELLO LORA");

    Serial.println("LoRa text receiver ready");
    Serial.println("Config: 915 MHz, SF7, BW125 kHz, CR4/5, sync 0x12, CRC on");
}

void loop()
{
    uint8_t buf[kRxBufferSize];
    int16_t rssi = 0;
    int8_t snr = 0;

    const int16_t len = sx126x_shell_recv_data(buf, sizeof(buf) - 1, kRxTimeoutMs, &rssi, &snr);
    if (len > 0) {
        buf[len] = '\0';
        rx_count++;

        Serial.print("RX #");
        Serial.print(rx_count);
        Serial.print(": ");
        Serial.print((const char *)buf);
        Serial.print("  RSSI=");
        Serial.print(rssi);
        Serial.print(" dBm  SNR=");
        Serial.println(snr);

        if (!first_rx_shown) {
            first_rx_shown = true;
            char text[64];
            snprintf(text, sizeof(text), "RX: %s\nRSSI %d dBm", (const char *)buf, (int)rssi);
            elink_gdem0213b74_show_text(text);
        }
    } else if (len != SX126X_ERR_RX_TIMEOUT) {
        Serial.print("RX error: ");
        Serial.println(len);
    }
}