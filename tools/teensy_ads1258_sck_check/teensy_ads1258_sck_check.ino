#include <Arduino.h>

static constexpr uint8_t PIN_ADS_SCK = 13;
static constexpr uint8_t PIN_ADS_CS = 10;
static constexpr uint8_t PIN_ADS_DRDY = 14;
static constexpr uint8_t PIN_ADS_START = 15;
static constexpr uint8_t PIN_ADS_RESET = 16;
static constexpr uint8_t PIN_ADS_PWDN = 17;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
  }

  pinMode(PIN_ADS_SCK, OUTPUT);
  pinMode(PIN_ADS_CS, OUTPUT);
  pinMode(PIN_ADS_DRDY, INPUT);
  pinMode(PIN_ADS_START, OUTPUT);
  pinMode(PIN_ADS_RESET, OUTPUT);
  pinMode(PIN_ADS_PWDN, OUTPUT);

  digitalWriteFast(PIN_ADS_CS, HIGH);
  digitalWriteFast(PIN_ADS_START, HIGH);
  digitalWriteFast(PIN_ADS_RESET, HIGH);
  digitalWriteFast(PIN_ADS_PWDN, HIGH);
  digitalWriteFast(PIN_ADS_SCK, LOW);

  Serial.println();
  Serial.println("ADS1258_SCK_CHECK_BEGIN");
  Serial.println("Teensy pin 13 / ADS1258 pin 22 SCK is driven as ~100 kHz square-wave bursts.");
  Serial.println("Expected LA: channel connected to SCK toggles for 250 ms, then stays low for 250 ms.");
}

void loop() {
  static uint32_t lastPrintMs = 0;

  uint32_t burstStart = millis();
  while (millis() - burstStart < 250) {
    digitalWriteFast(PIN_ADS_SCK, HIGH);
    delayMicroseconds(5);
    digitalWriteFast(PIN_ADS_SCK, LOW);
    delayMicroseconds(5);
  }

  if (millis() - lastPrintMs >= 1000) {
    lastPrintMs = millis();
    Serial.print("SCK_BURST drdy=");
    Serial.print(digitalReadFast(PIN_ADS_DRDY));
    Serial.print(" start=");
    Serial.print(digitalReadFast(PIN_ADS_START));
    Serial.print(" reset=");
    Serial.print(digitalReadFast(PIN_ADS_RESET));
    Serial.print(" pwdn=");
    Serial.print(digitalReadFast(PIN_ADS_PWDN));
    Serial.print(" cs=");
    Serial.println(digitalReadFast(PIN_ADS_CS));
  }

  digitalWriteFast(PIN_ADS_SCK, LOW);
  delay(250);
}
