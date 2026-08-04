#include <Arduino.h>
#include <SPI.h>

static constexpr uint8_t PIN_ADS_CS = 10;
static constexpr uint8_t PIN_ADS_DRDY = 14;
static constexpr uint8_t PIN_ADS_START = 15;
static constexpr uint8_t PIN_ADS_RESET = 16;
static constexpr uint8_t PIN_ADS_PWDN = 17;

static constexpr uint8_t CMD_READ_REGISTER = 0x40;
static constexpr uint8_t CMD_WRITE_REGISTER = 0x60;
static constexpr uint8_t CMD_CHANNEL_DATA_READ = 0x30;
static constexpr uint8_t CMD_RESET = 0xC0;
static constexpr uint8_t CMD_NOP = 0x00;

static constexpr uint8_t REG_CONFIG0 = 0x00;
static constexpr uint8_t REG_CONFIG1 = 0x01;
static constexpr uint8_t REG_MUXDIF = 0x03;
static constexpr uint8_t REG_MUXSG0 = 0x04;
static constexpr uint8_t REG_MUXSG1 = 0x05;
static constexpr uint8_t REG_SYSRED = 0x06;
static constexpr uint8_t REG_GPIOC = 0x07;
static constexpr uint8_t REG_GPIOD = 0x08;
static constexpr uint8_t REG_ID = 0x09;

static void printHex2(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

static const char *modeName(uint8_t mode) {
  switch (mode) {
    case SPI_MODE0: return "MODE0";
    case SPI_MODE1: return "MODE1";
    case SPI_MODE2: return "MODE2";
    case SPI_MODE3: return "MODE3";
    default: return "UNKNOWN";
  }
}

static void selectAdc(uint32_t hz, uint8_t mode) {
  SPI.beginTransaction(SPISettings(hz, MSBFIRST, mode));
  digitalWriteFast(PIN_ADS_CS, LOW);
  delayMicroseconds(2);
}

static void deselectAdc() {
  delayMicroseconds(2);
  digitalWriteFast(PIN_ADS_CS, HIGH);
  SPI.endTransaction();
}

static uint8_t readRegisterRaw(uint8_t reg, uint32_t hz, uint8_t mode) {
  selectAdc(hz, mode);
  SPI.transfer(CMD_READ_REGISTER | (reg & 0x0F));
  uint8_t value = SPI.transfer(CMD_NOP);
  deselectAdc();
  return value;
}

static void readChannelRaw(uint8_t &status, uint8_t &b2, uint8_t &b1, uint8_t &b0) {
  selectAdc(1000000UL, SPI_MODE3);
  SPI.transfer(CMD_CHANNEL_DATA_READ);
  status = SPI.transfer(CMD_NOP);
  b2 = SPI.transfer(CMD_NOP);
  b1 = SPI.transfer(CMD_NOP);
  b0 = SPI.transfer(CMD_NOP);
  deselectAdc();
}

static void writeRegisterRaw(uint8_t reg, uint8_t value, uint32_t hz, uint8_t mode) {
  selectAdc(hz, mode);
  SPI.transfer(CMD_WRITE_REGISTER | (reg & 0x0F));
  SPI.transfer(value);
  deselectAdc();
}

static void softwareReset(uint32_t hz, uint8_t mode) {
  selectAdc(hz, mode);
  SPI.transfer(CMD_RESET);
  deselectAdc();
  delay(5);
}

static void printPinState(const char *label) {
  Serial.print(label);
  Serial.print(" drdy=");
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

static void sweepSpiReads() {
  static const uint32_t speeds[] = {100000UL, 500000UL, 1000000UL, 4000000UL};
  static const uint8_t modes[] = {SPI_MODE0, SPI_MODE1, SPI_MODE2, SPI_MODE3};

  Serial.println("SPI_SWEEP_BEGIN");
  for (uint8_t si = 0; si < sizeof(speeds) / sizeof(speeds[0]); si++) {
    for (uint8_t mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
      uint32_t hz = speeds[si];
      uint8_t mode = modes[mi];
      uint8_t id = readRegisterRaw(REG_ID, hz, mode);
      uint8_t c0 = readRegisterRaw(REG_CONFIG0, hz, mode);
      uint8_t c1 = readRegisterRaw(REG_CONFIG1, hz, mode);

      Serial.print("SPI_TRY hz=");
      Serial.print(hz);
      Serial.print(" mode=");
      Serial.print(modeName(mode));
      Serial.print(" drdy=");
      Serial.print(digitalReadFast(PIN_ADS_DRDY));
      Serial.print(" id=0x");
      printHex2(id);
      Serial.print(" config0=0x");
      printHex2(c0);
      Serial.print(" config1=0x");
      printHex2(c1);
      Serial.print(" detect=");
      Serial.println((id != 0x00 && id != 0xFF && ((id & 0x10) == 0)) ? "MAYBE_PASS" : "FAIL");
    }
  }
  Serial.println("SPI_SWEEP_END");
}

static void printRegisterSet(const char *label) {
  Serial.print(label);
  Serial.print(" id=0x");
  printHex2(readRegisterRaw(REG_ID, 1000000UL, SPI_MODE3));
  Serial.print(" config0=0x");
  printHex2(readRegisterRaw(REG_CONFIG0, 1000000UL, SPI_MODE3));
  Serial.print(" config1=0x");
  printHex2(readRegisterRaw(REG_CONFIG1, 1000000UL, SPI_MODE3));
  Serial.print(" muxdif=0x");
  printHex2(readRegisterRaw(REG_MUXDIF, 1000000UL, SPI_MODE3));
  Serial.print(" muxsg0=0x");
  printHex2(readRegisterRaw(REG_MUXSG0, 1000000UL, SPI_MODE3));
  Serial.print(" muxsg1=0x");
  printHex2(readRegisterRaw(REG_MUXSG1, 1000000UL, SPI_MODE3));
  Serial.print(" sysred=0x");
  printHex2(readRegisterRaw(REG_SYSRED, 1000000UL, SPI_MODE3));
  Serial.print(" gpioc=0x");
  printHex2(readRegisterRaw(REG_GPIOC, 1000000UL, SPI_MODE3));
  Serial.print(" gpiod=0x");
  printHex2(readRegisterRaw(REG_GPIOD, 1000000UL, SPI_MODE3));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 10000) {
  }

  Serial.println();
  Serial.println("ADS1258_PIN_DIAG_BEGIN");
  Serial.println("wired pins: cs=10 drdy=14 start=15 reset=16 pwdn=17 sck=13 mosi=11 miso=12");

  pinMode(PIN_ADS_CS, OUTPUT);
  pinMode(PIN_ADS_START, OUTPUT);
  pinMode(PIN_ADS_RESET, OUTPUT);
  pinMode(PIN_ADS_PWDN, OUTPUT);
  pinMode(PIN_ADS_DRDY, INPUT);

  digitalWriteFast(PIN_ADS_CS, HIGH);
  digitalWriteFast(PIN_ADS_START, LOW);
  digitalWriteFast(PIN_ADS_PWDN, HIGH);
  digitalWriteFast(PIN_ADS_RESET, HIGH);

  SPI.begin();

  printPinState("AFTER_INIT");

  Serial.println("HARD_RESET_PULSE_BEGIN");
  digitalWriteFast(PIN_ADS_START, LOW);
  digitalWriteFast(PIN_ADS_PWDN, HIGH);
  digitalWriteFast(PIN_ADS_RESET, LOW);
  printPinState("RESET_LOW");
  delay(10);
  digitalWriteFast(PIN_ADS_RESET, HIGH);
  delay(20);
  printPinState("RESET_HIGH");
  Serial.println("HARD_RESET_PULSE_END");

  softwareReset(1000000UL, SPI_MODE3);
  printPinState("AFTER_SPI_RESET_MODE3");

  writeRegisterRaw(REG_CONFIG0, 0x0A, 1000000UL, SPI_MODE3);
  writeRegisterRaw(REG_CONFIG1, 0x83, 1000000UL, SPI_MODE3);
  writeRegisterRaw(REG_MUXSG0, 0x00, 1000000UL, SPI_MODE3);
  writeRegisterRaw(REG_MUXSG1, 0x00, 1000000UL, SPI_MODE3);
  writeRegisterRaw(REG_SYSRED, 0x00, 1000000UL, SPI_MODE3);
  writeRegisterRaw(REG_MUXDIF, 0x07, 1000000UL, SPI_MODE3);

  // ADS1258EVM default J3 connects START to ADS GPIO0, not J6-1.
  // Make GPIO0 an output, then drive it high so default J3 starts conversions.
  writeRegisterRaw(REG_GPIOC, 0xFE, 1000000UL, SPI_MODE3);
  writeRegisterRaw(REG_GPIOD, 0x01, 1000000UL, SPI_MODE3);

  digitalWriteFast(PIN_ADS_START, HIGH);
  delay(20);
  printPinState("START_HIGH");
  printRegisterSet("CONFIGURED");
  sweepSpiReads();

  Serial.println("ADS1258_PIN_DIAG_LOOPING");
}

void loop() {
  static uint32_t lastMs = 0;
  if (millis() - lastMs < 1000) {
    return;
  }
  lastMs = millis();

  uint8_t id = readRegisterRaw(REG_ID, 1000000UL, SPI_MODE3);
  uint8_t c0 = readRegisterRaw(REG_CONFIG0, 1000000UL, SPI_MODE3);
  uint8_t c1 = readRegisterRaw(REG_CONFIG1, 1000000UL, SPI_MODE3);
  uint8_t muxdif = readRegisterRaw(REG_MUXDIF, 1000000UL, SPI_MODE3);
  uint8_t muxsg0 = readRegisterRaw(REG_MUXSG0, 1000000UL, SPI_MODE3);
  uint8_t muxsg1 = readRegisterRaw(REG_MUXSG1, 1000000UL, SPI_MODE3);
  uint8_t gpioc = readRegisterRaw(REG_GPIOC, 1000000UL, SPI_MODE3);
  uint8_t gpiod = readRegisterRaw(REG_GPIOD, 1000000UL, SPI_MODE3);
  uint8_t status;
  uint8_t b2;
  uint8_t b1;
  uint8_t b0;
  readChannelRaw(status, b2, b1, b0);

  Serial.print("LOOP_STATUS ms=");
  Serial.print(millis());
  Serial.print(" drdy=");
  Serial.print(digitalReadFast(PIN_ADS_DRDY));
  Serial.print(" id=0x");
  printHex2(id);
  Serial.print(" config0=0x");
  printHex2(c0);
  Serial.print(" config1=0x");
  printHex2(c1);
  Serial.print(" muxdif=0x");
  printHex2(muxdif);
  Serial.print(" muxsg0=0x");
  printHex2(muxsg0);
  Serial.print(" muxsg1=0x");
  printHex2(muxsg1);
  Serial.print(" gpioc=0x");
  printHex2(gpioc);
  Serial.print(" gpiod=0x");
  printHex2(gpiod);
  Serial.print(" ch_status=0x");
  printHex2(status);
  Serial.print(" new=");
  Serial.print((status & 0x80) ? 1 : 0);
  Serial.print(" chid=0x");
  printHex2(status & 0x1F);
  Serial.print(" raw=0x");
  printHex2(b2);
  printHex2(b1);
  printHex2(b0);
  Serial.println();
}
