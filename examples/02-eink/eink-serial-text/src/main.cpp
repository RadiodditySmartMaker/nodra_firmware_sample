#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <elink_gdem0213b74.h>

namespace {

constexpr size_t kInputCapacity = 128;
char input_buffer[kInputCapacity];
size_t input_length;
bool last_was_cr;
bool prompt_sent;

void submit_input()
{
    input_buffer[input_length] = '\0';
    elink_gdem0213b74_show_text(input_buffer);
    Serial.print("\r\nDisplayed: ");
    Serial.println(input_buffer);
    Serial.print("> ");
    input_length = 0;
}

void receive_byte(char byte)
{
    if (byte == '\r') {
        submit_input();
        last_was_cr = true;
        return;
    }

    if (byte == '\n') {
        if (!last_was_cr) {
            submit_input();
        }
        last_was_cr = false;
        return;
    }

    last_was_cr = false;

    if (byte == '\b' || (uint8_t)byte == 0x7F) {
        if (input_length > 0) {
            input_length--;
            Serial.print("\b \b");
        }
        return;
    }

    if (byte < 0x20 || byte > 0x7E) {
        return;
    }

    if (input_length >= kInputCapacity - 1) {
        Serial.println("Input is too long; press Enter to display it.");
        return;
    }

    input_buffer[input_length++] = byte;
    Serial.write(byte);
}

} // namespace

void setup()
{
    Serial.begin(115200);

    elink_gdem0213b74_setup();
    (void)elink_gdem0213b74_start();
    elink_gdem0213b74_show_text("Serial text ready\nSend ASCII text + Enter");
}

void loop()
{
    if (Serial && !prompt_sent) {
        Serial.println("E-ink serial text ready. Send ASCII text and press Enter.");
        Serial.print("> ");
        prompt_sent = true;
    }

    while (Serial.available() > 0) {
        receive_byte((char)Serial.read());
    }
}
