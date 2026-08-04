#include <Arduino.h>
#include <SPI.h>
#include <ADS1258_Simple.h>

static constexpr uint8_t PIN_ADS_CS = 10;
static constexpr uint8_t PIN_ADS_DRDY = 14;
static constexpr uint8_t PIN_ADS_START = 15;
static constexpr uint8_t PIN_ADS_RESET = 16;
static constexpr uint8_t PIN_ADS_PWDN = 17;

static constexpr float VREF_VOLTS = 5.000f;
static constexpr float SHUNT_OHMS = 1000.0f;
static constexpr uint8_t DIFF_MASK_READ_SET_RESET = 0x07;

ADS1258Simple ads1258(PIN_ADS_CS, PIN_ADS_DRDY, SPI, 1000000UL, SPI_MODE3);

static void printHex2(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

static float voltsToMicroamps(float volts) {
  return (volts / SHUNT_OHMS) * 1000000.0f;
}

static const char *logicalName(uint8_t channelId) {
  switch (channelId) {
    case ADS1258Simple::CH_DIFF0: return "read_shunt";
    case ADS1258Simple::CH_DIFF1: return "set_shunt";
    case ADS1258Simple::CH_DIFF2: return "reset_shunt";
    default: return "unused";
  }
}

static void startConversions() {
  digitalWrite(PIN_ADS_START, HIGH);

  // ADS1258EVM factory J3 usually connects START to ADS GPIO0, not J6.1.
  // This starts conversions with either J3 route: GPIO0->START or J6.1->START.
  ads1258.writeRegister(ADS1258Simple::REG_GPIOC, 0xFE);
  ads1258.writeRegister(ADS1258Simple::REG_GPIOD, 0x01);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

  Serial.println();
  Serial.println("ADS1258_DETECT_BEGIN");
  Serial.println("pins cs=10 drdy=14 start=15 reset=16 pwdn=17 spi=SCK13/MOSI11/MISO12");

  pinMode(PIN_ADS_START, OUTPUT);
  pinMode(PIN_ADS_RESET, OUTPUT);
  pinMode(PIN_ADS_PWDN, OUTPUT);
  pinMode(PIN_ADS_DRDY, INPUT);

  digitalWrite(PIN_ADS_START, LOW);
  digitalWrite(PIN_ADS_PWDN, HIGH);
  digitalWrite(PIN_ADS_RESET, HIGH);

  ads1258.begin(false);

  digitalWrite(PIN_ADS_RESET, LOW);
  delayMicroseconds(10);
  digitalWrite(PIN_ADS_RESET, HIGH);
  delay(10);
  ads1258.reset();
  delay(10);

  uint8_t id = ads1258.readId();
  Serial.print("ID_REGISTER=0x");
  printHex2(id);
  Serial.println();
  Serial.print("ADS1258_ID_CHECK=");
  Serial.println(ads1258.isAds1258() ? "PASS" : "FAIL");

  ads1258.configureAutoScanDifferential(
    DIFF_MASK_READ_SET_RESET,
    ADS1258Simple::DRATE_23739_SPS
  );

  Serial.print("CONFIG0=0x");
  printHex2(ads1258.readRegister(ADS1258Simple::REG_CONFIG0));
  Serial.println();
  Serial.print("CONFIG1=0x");
  printHex2(ads1258.readRegister(ADS1258Simple::REG_CONFIG1));
  Serial.println();
  Serial.print("MUXDIF=0x");
  printHex2(ads1258.readRegister(ADS1258Simple::REG_MUXDIF));
  Serial.println();
  startConversions();
  delay(20);
  Serial.print("GPIOC=0x");
  printHex2(ads1258.readRegister(ADS1258Simple::REG_GPIOC));
  Serial.println();
  Serial.print("GPIOD=0x");
  printHex2(ads1258.readRegister(ADS1258Simple::REG_GPIOD));
  Serial.println();

  Serial.print("DRDY_AFTER_START=");
  Serial.println(digitalRead(PIN_ADS_DRDY));
  Serial.println("time_ms,logical_channel,status_hex,raw_code,vshunt_mV,ishunt_uA,flags");
}

void loop() {
  static uint16_t printed = 0;
  static bool donePrinted = false;
  static uint32_t lastHeartbeatMs = 0;

  if (donePrinted) {
    if (ads1258.waitForDataReady(1000)) {
      (void)ads1258.readChannel();
    }
    if (millis() - lastHeartbeatMs >= 1000) {
      lastHeartbeatMs = millis();
      Serial.print("ADS1258_DETECT_IDLE drdy=");
      Serial.println(digitalRead(PIN_ADS_DRDY));
    }
    return;
  }

  if (!ads1258.waitForDataReady(1000)) {
    uint8_t id = ads1258.readId();
    uint8_t config0 = ads1258.readRegister(ADS1258Simple::REG_CONFIG0);
    uint8_t config1 = ads1258.readRegister(ADS1258Simple::REG_CONFIG1);
    uint8_t muxdif = ads1258.readRegister(ADS1258Simple::REG_MUXDIF);
    uint8_t gpioc = ads1258.readRegister(ADS1258Simple::REG_GPIOC);
    uint8_t gpiod = ads1258.readRegister(ADS1258Simple::REG_GPIOD);
    Serial.print("DRDY_TIMEOUT drdy=");
    Serial.print(digitalRead(PIN_ADS_DRDY));
    Serial.print(" id=0x");
    printHex2(id);
    Serial.print(" id_check=");
    Serial.print(ads1258.isAds1258() ? "PASS" : "FAIL");
    Serial.print(" config0=0x");
    printHex2(config0);
    Serial.print(" config1=0x");
    printHex2(config1);
    Serial.print(" muxdif=0x");
    printHex2(muxdif);
    Serial.print(" gpioc=0x");
    printHex2(gpioc);
    Serial.print(" gpiod=0x");
    printHex2(gpiod);
    Serial.println();
    return;
  }

  ADS1258Simple::ChannelData sample = ads1258.readChannel();
  if (!sample.isNew) {
    return;
  }

  float volts = ads1258.codeToVoltage(sample.code, VREF_VOLTS);
  float millivolts = volts * 1000.0f;

  Serial.print(millis());
  Serial.print(',');
  Serial.print(logicalName(sample.channelId));
  Serial.print(",0x");
  printHex2(sample.status);
  Serial.print(',');
  Serial.print(sample.code);
  Serial.print(',');
  Serial.print(millivolts, 6);
  Serial.print(',');
  Serial.print(voltsToMicroamps(volts), 6);
  Serial.print(',');

  bool printedFlag = false;
  if (sample.overRange) {
    Serial.print("OVF");
    printedFlag = true;
  }
  if (sample.supplyLow) {
    if (printedFlag) {
      Serial.print('|');
    }
    Serial.print("SUPPLY_LOW");
    printedFlag = true;
  }
  if (!printedFlag) {
    Serial.print("OK");
  }
  Serial.println();

  printed++;
  if (printed >= 60 && !donePrinted) {
    Serial.println("ADS1258_DETECT_DONE");
    donePrinted = true;
  }
}
