#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;

static constexpr uint8_t ADS1_ADDR = 0x48;
static constexpr uint8_t ADS2_ADDR = 0x49;
static constexpr float ADS1115_GAIN16_MV_PER_COUNT = 0.0078125f;
static constexpr float SHUNT_OHMS = 1000.0f;

bool ads1Ok = false;
bool ads2Ok = false;

float rawToMv(int16_t raw) {
  return raw * ADS1115_GAIN16_MV_PER_COUNT;
}

float mvToUa(float mv) {
  return (mv / SHUNT_OHMS) * 1000.0f;
}

void scanI2c() {
  Serial.println("I2C scan:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  found 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
    }
  }
}

void printDiff(const char *label, Adafruit_ADS1115 &ads, int pair) {
  int16_t raw = pair == 0 ? ads.readADC_Differential_0_1()
                          : ads.readADC_Differential_2_3();
  float mv = rawToMv(raw);
  float ua = mvToUa(mv);

  Serial.print(label);
  Serial.print(" raw=");
  Serial.print(raw);
  Serial.print(" vshunt_mV=");
  Serial.print(mv, 4);
  Serial.print(" ishunt_uA=");
  Serial.println(ua, 4);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("Teensy ADS1115 differential shunt validation");
  Serial.println("Expected wiring: ADC1 A0-A1, ADC1 A2-A3, ADC2 A0-A1");

  Wire.begin();
  scanI2c();

  ads1.setGain(GAIN_SIXTEEN);
  ads1.setDataRate(RATE_ADS1115_475SPS);
  ads1Ok = ads1.begin(ADS1_ADDR, &Wire);
  Serial.print("ADS1 0x48 init: ");
  Serial.println(ads1Ok ? "OK" : "FAIL");

  ads2.setGain(GAIN_SIXTEEN);
  ads2.setDataRate(RATE_ADS1115_475SPS);
  ads2Ok = ads2.begin(ADS2_ADDR, &Wire);
  Serial.print("ADS2 0x49 init: ");
  Serial.println(ads2Ok ? "OK" : "FAIL");
}

void loop() {
  Serial.println("---");
  if (ads1Ok) {
    printDiff("shunt_read  ADC1 A0-A1", ads1, 0);
    printDiff("shunt_set   ADC1 A2-A3", ads1, 1);
  } else {
    Serial.println("ADS1 unavailable");
  }

  if (ads2Ok) {
    printDiff("shunt_reset ADC2 A0-A1", ads2, 0);
  } else {
    Serial.println("ADS2 unavailable");
  }

  delay(1000);
}
