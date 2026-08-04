#include <SPI.h>
#include <ADS1258_Simple.h>

// Update these pins for your board.
const uint8_t CS_PIN = 10;
const uint8_t DRDY_PIN = 2;
const uint8_t START_PIN = 9;
const uint8_t RESET_PIN = 8;
const uint8_t PWDN_PIN = 7;

// Set this to your actual reference voltage: VREFP - VREFN.
const float VREF_VOLTS = 2.500f;

// First-test default: read AIN0 only, measured versus AINCOM.
// Bit 0 = AIN0, bit 1 = AIN1, ... bit 15 = AIN15.
// Change to 0xFFFF to scan all single-ended channels.
const uint16_t CHANNEL_MASK = 0x0001;

ADS1258Simple ads1258(CS_PIN, DRDY_PIN);

void printRegister(const char *name, uint8_t reg) {
  Serial.print(name);
  Serial.print(" = 0x");
  uint8_t value = ads1258.readRegister(reg);
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.println(value, HEX);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ; // Wait briefly for native-USB boards.
  }

  pinMode(START_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(PWDN_PIN, OUTPUT);

  // Stop conversions while configuring. Keep device out of reset and power-down.
  digitalWrite(START_PIN, LOW);
  digitalWrite(PWDN_PIN, HIGH);
  digitalWrite(RESET_PIN, HIGH);

  ads1258.begin(false);

  // Hardware reset, then software reset for a known state.
  digitalWrite(RESET_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(RESET_PIN, HIGH);
  delay(5);
  ads1258.reset();
  delay(5);

  Serial.println();
  Serial.println("ADS1258 basic read");

  uint8_t id = ads1258.readId();
  Serial.print("ID register = 0x");
  if (id < 0x10) {
    Serial.print('0');
  }
  Serial.println(id, HEX);

  if (!ads1258.isAds1258()) {
    Serial.println("WARNING: ADS1258 ID check failed. Check SPI wiring, power, clock, CS, RESET, and PWDN.");
  }

  ads1258.configureAutoScanSingleEnded(
    CHANNEL_MASK,
    ADS1258Simple::DRATE_1831_SPS
  );

  printRegister("CONFIG0", ADS1258Simple::REG_CONFIG0);
  printRegister("CONFIG1", ADS1258Simple::REG_CONFIG1);
  printRegister("MUXSG0 ", ADS1258Simple::REG_MUXSG0);
  printRegister("MUXSG1 ", ADS1258Simple::REG_MUXSG1);

  Serial.println("time_ms,channel,status_hex,raw_code,volts,flags");

  // Start continuous conversions.
  digitalWrite(START_PIN, HIGH);
}

void loop() {
  if (!ads1258.waitForDataReady(1000)) {
    Serial.println("DRDY timeout. Check clock source, START, RESET/PWDN, and DRDY wiring.");
    return;
  }

  ADS1258Simple::ChannelData sample = ads1258.readChannel();

  if (!sample.isNew) {
    return;
  }

  float volts = ads1258.codeToVoltage(sample.code, VREF_VOLTS);

  Serial.print(millis());
  Serial.print(',');
  Serial.print(ads1258.channelName(sample.channelId));
  Serial.print(",0x");
  if (sample.status < 0x10) {
    Serial.print('0');
  }
  Serial.print(sample.status, HEX);
  Serial.print(',');
  Serial.print(sample.code);
  Serial.print(',');
  Serial.print(volts, 8);
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
}

