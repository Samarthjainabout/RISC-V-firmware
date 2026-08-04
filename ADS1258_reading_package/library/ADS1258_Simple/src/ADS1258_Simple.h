#ifndef ADS1258_SIMPLE_H
#define ADS1258_SIMPLE_H

#include <Arduino.h>
#include <SPI.h>

class ADS1258Simple {
public:
  // ADS1258 register addresses.
  static constexpr uint8_t REG_CONFIG0 = 0x00;
  static constexpr uint8_t REG_CONFIG1 = 0x01;
  static constexpr uint8_t REG_MUXSCH  = 0x02;
  static constexpr uint8_t REG_MUXDIF  = 0x03;
  static constexpr uint8_t REG_MUXSG0  = 0x04;
  static constexpr uint8_t REG_MUXSG1  = 0x05;
  static constexpr uint8_t REG_SYSRED  = 0x06;
  static constexpr uint8_t REG_GPIOC   = 0x07;
  static constexpr uint8_t REG_GPIOD   = 0x08;
  static constexpr uint8_t REG_ID      = 0x09;

  // SPI commands.
  static constexpr uint8_t CMD_CHANNEL_DATA_READ = 0x30; // C[2:0]=001, MUL=1
  static constexpr uint8_t CMD_READ_REGISTER     = 0x40; // C[2:0]=010
  static constexpr uint8_t CMD_WRITE_REGISTER    = 0x60; // C[2:0]=011
  static constexpr uint8_t CMD_PULSE_CONVERT     = 0x80; // C[2:0]=100
  static constexpr uint8_t CMD_RESET             = 0xC0; // C[2:0]=110
  static constexpr uint8_t CMD_NOP               = 0x00;

  // CONFIG0 bits.
  static constexpr uint8_t CONFIG0_SPI_RESET_SHORT = 0x40;
  static constexpr uint8_t CONFIG0_MUXMOD_FIXED    = 0x20;
  static constexpr uint8_t CONFIG0_BYPAS_EXTERNAL  = 0x10;
  static constexpr uint8_t CONFIG0_CLKOUT_ENABLE   = 0x08;
  static constexpr uint8_t CONFIG0_CHOP_ENABLE     = 0x04;
  static constexpr uint8_t CONFIG0_STATUS_ENABLE   = 0x02;

  // CONFIG1 bits.
  static constexpr uint8_t CONFIG1_IDLE_SLEEP      = 0x80;
  static constexpr uint8_t CONFIG1_DELAY_0         = 0x10;
  static constexpr uint8_t CONFIG1_DELAY_1         = 0x20;
  static constexpr uint8_t CONFIG1_DELAY_2         = 0x40;
  static constexpr uint8_t CONFIG1_SENSOR_BIAS_OFF = 0x00;
  static constexpr uint8_t CONFIG1_SENSOR_BIAS_1UA = 0x04;
  static constexpr uint8_t CONFIG1_SENSOR_BIAS_24UA = 0x0C;

  // DRATE[1:0]. Auto-scan rates at fCLK ~= 16 MHz, chop off, delay 0.
  static constexpr uint8_t DRATE_1831_SPS  = 0x00;
  static constexpr uint8_t DRATE_6168_SPS  = 0x01;
  static constexpr uint8_t DRATE_15123_SPS = 0x02;
  static constexpr uint8_t DRATE_23739_SPS = 0x03;

  // Status byte bits.
  static constexpr uint8_t STATUS_NEW             = 0x80;
  static constexpr uint8_t STATUS_OVER_RANGE      = 0x40;
  static constexpr uint8_t STATUS_SUPPLY_LOW      = 0x20;
  static constexpr uint8_t STATUS_CHANNEL_ID_MASK = 0x1F;

  // Auto-scan channel IDs.
  static constexpr uint8_t CH_DIFF0 = 0x00;
  static constexpr uint8_t CH_DIFF1 = 0x01;
  static constexpr uint8_t CH_DIFF2 = 0x02;
  static constexpr uint8_t CH_DIFF3 = 0x03;
  static constexpr uint8_t CH_DIFF4 = 0x04;
  static constexpr uint8_t CH_DIFF5 = 0x05;
  static constexpr uint8_t CH_DIFF6 = 0x06;
  static constexpr uint8_t CH_DIFF7 = 0x07;
  static constexpr uint8_t CH_AIN0  = 0x08;
  static constexpr uint8_t CH_AIN1  = 0x09;
  static constexpr uint8_t CH_AIN2  = 0x0A;
  static constexpr uint8_t CH_AIN3  = 0x0B;
  static constexpr uint8_t CH_AIN4  = 0x0C;
  static constexpr uint8_t CH_AIN5  = 0x0D;
  static constexpr uint8_t CH_AIN6  = 0x0E;
  static constexpr uint8_t CH_AIN7  = 0x0F;
  static constexpr uint8_t CH_AIN8  = 0x10;
  static constexpr uint8_t CH_AIN9  = 0x11;
  static constexpr uint8_t CH_AIN10 = 0x12;
  static constexpr uint8_t CH_AIN11 = 0x13;
  static constexpr uint8_t CH_AIN12 = 0x14;
  static constexpr uint8_t CH_AIN13 = 0x15;
  static constexpr uint8_t CH_AIN14 = 0x16;
  static constexpr uint8_t CH_AIN15 = 0x17;
  static constexpr uint8_t CH_OFFSET = 0x18;
  static constexpr uint8_t CH_VCC    = 0x1A;
  static constexpr uint8_t CH_TEMP   = 0x1B;
  static constexpr uint8_t CH_GAIN   = 0x1C;
  static constexpr uint8_t CH_REF    = 0x1D;

  struct ChannelData {
    uint8_t status = 0;
    int32_t code = 0;
    bool isNew = false;
    bool overRange = false;
    bool supplyLow = false;
    uint8_t channelId = 0;
  };

  ADS1258Simple(
    uint8_t csPin,
    uint8_t drdyPin,
    SPIClass &spi = SPI,
    uint32_t spiHz = 4000000UL,
    uint8_t spiMode = SPI_MODE3
  );

  void begin(bool resetDevice = true);
  void reset();
  void pulseConvert();

  uint8_t readRegister(uint8_t reg);
  void writeRegister(uint8_t reg, uint8_t value);
  uint8_t readId();
  bool isAds1258();

  void configureAutoScanSingleEnded(
    uint16_t channelMask,
    uint8_t dataRate = DRATE_1831_SPS,
    uint8_t extraConfig0 = 0
  );

  void configureAutoScanDifferential(
    uint8_t differentialMask,
    uint8_t dataRate = DRATE_1831_SPS,
    uint8_t extraConfig0 = 0
  );

  void configureFixedChannel(
    uint8_t positiveAin,
    uint8_t negativeAin,
    uint8_t dataRate = DRATE_1831_SPS,
    uint8_t extraConfig0 = 0
  );

  bool dataReady() const;
  bool waitForDataReady(uint32_t timeoutMs = 1000) const;
  ChannelData readChannel();

  float codeToVoltage(int32_t code, float vrefVolts) const;
  const char *channelName(uint8_t channelId) const;

private:
  uint8_t _csPin;
  uint8_t _drdyPin;
  SPIClass *_spi;
  uint32_t _spiHz;
  uint8_t _spiMode;

  void select();
  void deselect();
  uint8_t transfer(uint8_t value);
  static int32_t signExtend24(uint32_t raw24);
};

#endif

