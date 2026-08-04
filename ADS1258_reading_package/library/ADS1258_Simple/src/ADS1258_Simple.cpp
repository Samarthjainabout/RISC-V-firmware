#include "ADS1258_Simple.h"

ADS1258Simple::ADS1258Simple(
  uint8_t csPin,
  uint8_t drdyPin,
  SPIClass &spi,
  uint32_t spiHz,
  uint8_t spiMode
) :
  _csPin(csPin),
  _drdyPin(drdyPin),
  _spi(&spi),
  _spiHz(spiHz),
  _spiMode(spiMode)
{
}

void ADS1258Simple::begin(bool resetDevice) {
  pinMode(_csPin, OUTPUT);
  digitalWrite(_csPin, HIGH);
  pinMode(_drdyPin, INPUT);

  _spi->begin();

  if (resetDevice) {
    reset();
  }
}

void ADS1258Simple::reset() {
  select();
  transfer(CMD_RESET);
  deselect();

  // Datasheet reset time is short, but leave margin for clock/startup settling.
  delay(2);
}

void ADS1258Simple::pulseConvert() {
  select();
  transfer(CMD_PULSE_CONVERT);
  deselect();
}

uint8_t ADS1258Simple::readRegister(uint8_t reg) {
  select();
  transfer(CMD_READ_REGISTER | (reg & 0x0F));
  uint8_t value = transfer(CMD_NOP);
  deselect();
  return value;
}

void ADS1258Simple::writeRegister(uint8_t reg, uint8_t value) {
  select();
  transfer(CMD_WRITE_REGISTER | (reg & 0x0F));
  transfer(value);
  deselect();
}

uint8_t ADS1258Simple::readId() {
  return readRegister(REG_ID);
}

bool ADS1258Simple::isAds1258() {
  uint8_t id = readId();
  if (id == 0x00 || id == 0xFF) {
    return false;
  }

  // ID4 is 0 for ADS1258 and 1 for ADS1158.
  return (id & 0x10) == 0;
}

void ADS1258Simple::configureAutoScanSingleEnded(
  uint16_t channelMask,
  uint8_t dataRate,
  uint8_t extraConfig0
) {
  writeRegister(REG_CONFIG0, CONFIG0_STATUS_ENABLE | (extraConfig0 & 0x7E));
  writeRegister(REG_CONFIG1, CONFIG1_IDLE_SLEEP | CONFIG1_SENSOR_BIAS_OFF | (dataRate & 0x03));
  writeRegister(REG_MUXDIF, 0x00);
  writeRegister(REG_SYSRED, 0x00);
  writeRegister(REG_MUXSG0, channelMask & 0x00FF);
  writeRegister(REG_MUXSG1, (channelMask >> 8) & 0x00FF);
}

void ADS1258Simple::configureAutoScanDifferential(
  uint8_t differentialMask,
  uint8_t dataRate,
  uint8_t extraConfig0
) {
  writeRegister(REG_CONFIG0, CONFIG0_STATUS_ENABLE | (extraConfig0 & 0x7E));
  writeRegister(REG_CONFIG1, CONFIG1_IDLE_SLEEP | CONFIG1_SENSOR_BIAS_OFF | (dataRate & 0x03));
  writeRegister(REG_MUXSG0, 0x00);
  writeRegister(REG_MUXSG1, 0x00);
  writeRegister(REG_SYSRED, 0x00);
  writeRegister(REG_MUXDIF, differentialMask);
}

void ADS1258Simple::configureFixedChannel(
  uint8_t positiveAin,
  uint8_t negativeAin,
  uint8_t dataRate,
  uint8_t extraConfig0
) {
  writeRegister(REG_CONFIG0, CONFIG0_MUXMOD_FIXED | CONFIG0_STATUS_ENABLE | (extraConfig0 & 0x7E));
  writeRegister(REG_CONFIG1, CONFIG1_IDLE_SLEEP | CONFIG1_SENSOR_BIAS_OFF | (dataRate & 0x03));
  writeRegister(REG_MUXSCH, ((positiveAin & 0x0F) << 4) | (negativeAin & 0x0F));
}

bool ADS1258Simple::dataReady() const {
  return digitalRead(_drdyPin) == LOW;
}

bool ADS1258Simple::waitForDataReady(uint32_t timeoutMs) const {
  uint32_t startMs = millis();

  while (!dataReady()) {
    if (timeoutMs > 0 && (millis() - startMs) >= timeoutMs) {
      return false;
    }
    yield();
  }

  return true;
}

ADS1258Simple::ChannelData ADS1258Simple::readChannel() {
  ChannelData data;

  select();
  transfer(CMD_CHANNEL_DATA_READ);
  data.status = transfer(CMD_NOP);
  uint8_t byte2 = transfer(CMD_NOP);
  uint8_t byte1 = transfer(CMD_NOP);
  uint8_t byte0 = transfer(CMD_NOP);
  deselect();

  uint32_t raw24 = ((uint32_t)byte2 << 16) | ((uint32_t)byte1 << 8) | byte0;
  data.code = signExtend24(raw24);
  data.isNew = (data.status & STATUS_NEW) != 0;
  data.overRange = (data.status & STATUS_OVER_RANGE) != 0;
  data.supplyLow = (data.status & STATUS_SUPPLY_LOW) != 0;
  data.channelId = data.status & STATUS_CHANNEL_ID_MASK;

  return data;
}

float ADS1258Simple::codeToVoltage(int32_t code, float vrefVolts) const {
  return ((float)code * vrefVolts) / 7864320.0f; // 0x780000
}

const char *ADS1258Simple::channelName(uint8_t channelId) const {
  switch (channelId) {
    case CH_DIFF0: return "DIFF0 AIN0-AIN1";
    case CH_DIFF1: return "DIFF1 AIN2-AIN3";
    case CH_DIFF2: return "DIFF2 AIN4-AIN5";
    case CH_DIFF3: return "DIFF3 AIN6-AIN7";
    case CH_DIFF4: return "DIFF4 AIN8-AIN9";
    case CH_DIFF5: return "DIFF5 AIN10-AIN11";
    case CH_DIFF6: return "DIFF6 AIN12-AIN13";
    case CH_DIFF7: return "DIFF7 AIN14-AIN15";
    case CH_AIN0: return "AIN0";
    case CH_AIN1: return "AIN1";
    case CH_AIN2: return "AIN2";
    case CH_AIN3: return "AIN3";
    case CH_AIN4: return "AIN4";
    case CH_AIN5: return "AIN5";
    case CH_AIN6: return "AIN6";
    case CH_AIN7: return "AIN7";
    case CH_AIN8: return "AIN8";
    case CH_AIN9: return "AIN9";
    case CH_AIN10: return "AIN10";
    case CH_AIN11: return "AIN11";
    case CH_AIN12: return "AIN12";
    case CH_AIN13: return "AIN13";
    case CH_AIN14: return "AIN14";
    case CH_AIN15: return "AIN15";
    case CH_OFFSET: return "OFFSET";
    case CH_VCC: return "VCC";
    case CH_TEMP: return "TEMP";
    case CH_GAIN: return "GAIN";
    case CH_REF: return "REF";
    default: return "UNKNOWN";
  }
}

void ADS1258Simple::select() {
  _spi->beginTransaction(SPISettings(_spiHz, MSBFIRST, _spiMode));
  digitalWrite(_csPin, LOW);
}

void ADS1258Simple::deselect() {
  digitalWrite(_csPin, HIGH);
  _spi->endTransaction();
}

uint8_t ADS1258Simple::transfer(uint8_t value) {
  return _spi->transfer(value);
}

int32_t ADS1258Simple::signExtend24(uint32_t raw24) {
  raw24 &= 0x00FFFFFFUL;

  if (raw24 & 0x00800000UL) {
    raw24 |= 0xFF000000UL;
  }

  return (int32_t)raw24;
}

