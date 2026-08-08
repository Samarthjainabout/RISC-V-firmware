/*
  Teensy -> Si5351 clock setup for Caravel.

  I2C device:
    - Si5351 at address 0x60 on Wire

  Output:
    - Si5351 CLK1 = 1 MHz
    - Si5351 CLK0 = 1 MHz also enabled for the existing probed
      oscillator output path used by the earlier 1 MHz sketch
    - 1 MHz square wave
    - Other Si5351 clock outputs disabled
*/

#include <Wire.h>
#include <Adafruit_SI5351.h>

static const uint8_t SI5351_ADDR = 0x60;
static const uint8_t SI5351_REG_DEVICE_STATUS = 0;
static const uint8_t SI5351_REG_OUTPUT_ENABLE = 3;
static const uint8_t SI5351_REG_CLK0_CONTROL = 16;
static const uint8_t SI5351_REG_CLK1_CONTROL = 17;
static const uint8_t SI5351_REG_PLL_RESET = 177;
static const uint32_t CLK1_HZ = 1000000UL;

Adafruit_SI5351 clockgen;

static bool writeSi5351Reg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(SI5351_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool readSi5351Reg(uint8_t reg, uint8_t *value) {
  Wire.beginTransmission(SI5351_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(SI5351_ADDR, (uint8_t)1) != 1) {
    return false;
  }

  *value = Wire.read();
  return true;
}

static void printHex8(uint8_t value) {
  const char hex[] = "0123456789ABCDEF";
  Serial.print("0x");
  Serial.print(hex[(value >> 4) & 0x0F]);
  Serial.print(hex[value & 0x0F]);
}

static void configureClock() {
  Serial.println("SI5351_CLK1_1MHZ_CONFIG_START");

  if (clockgen.begin(&Wire) != ERROR_NONE) {
    Serial.println("SI5351_NOT_FOUND addr=0x60");
    while (true) {
      delay(1000);
    }
  }

  clockgen.enableOutputs(false);

  // 25 MHz crystal * 32 = 800 MHz PLLA. Generate 8 MHz with the
  // multisynth (/100), then use R_DIV_8 to output 1 MHz. This avoids
  // the very large /800 multisynth setting on this board.
  if (clockgen.setupPLLInt(SI5351_PLL_A, 32) != ERROR_NONE) {
    Serial.println("SI5351_PLLA_CONFIG_FAILED");
    while (true) {
      delay(1000);
    }
  }

  if (clockgen.setupMultisynth(0, SI5351_PLL_A, 100, 0, 1) != ERROR_NONE) {
    Serial.println("SI5351_CLK0_MULTISYNTH_CONFIG_FAILED");
    while (true) {
      delay(1000);
    }
  }

  if (clockgen.setupMultisynth(1, SI5351_PLL_A, 100, 0, 1) != ERROR_NONE) {
    Serial.println("SI5351_CLK1_MULTISYNTH_CONFIG_FAILED");
    while (true) {
      delay(1000);
    }
  }

  if (clockgen.setupRdiv(0, SI5351_R_DIV_8) != ERROR_NONE) {
    Serial.println("SI5351_CLK0_RDIV_CONFIG_FAILED");
    while (true) {
      delay(1000);
    }
  }

  if (clockgen.setupRdiv(1, SI5351_R_DIV_8) != ERROR_NONE) {
    Serial.println("SI5351_CLK1_RDIV_CONFIG_FAILED");
    while (true) {
      delay(1000);
    }
  }

  // Reset both PLLs after programming, then enable outputs through the
  // library path used by the earlier 1 MHz sketch.
  writeSi5351Reg(SI5351_REG_PLL_RESET, 0xAC);
  clockgen.enableOutputs(true);

  uint8_t status = 0xFF;
  uint8_t outputEnable = 0xFF;
  uint8_t clk0Control = 0xFF;
  uint8_t clk1Control = 0xFF;
  readSi5351Reg(SI5351_REG_DEVICE_STATUS, &status);
  readSi5351Reg(SI5351_REG_OUTPUT_ENABLE, &outputEnable);
  readSi5351Reg(SI5351_REG_CLK0_CONTROL, &clk0Control);
  readSi5351Reg(SI5351_REG_CLK1_CONTROL, &clk1Control);

  Serial.print("SI5351_CLK1_1MHZ_OK freq_hz=");
  Serial.print(CLK1_HZ);
  Serial.print(" outputs=CLK0,CLK1 oe_reg=");
  printHex8(outputEnable);
  Serial.print(" status=");
  printHex8(status);
  Serial.print(" clk0_ctrl=");
  printHex8(clk0Control);
  Serial.print(" clk1_ctrl=");
  printHex8(clk1Control);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin();
  Wire.setClock(400000);

  configureClock();
}

void loop() {
  static unsigned long lastPrintMs = 0;

  if (millis() - lastPrintMs >= 5000) {
    lastPrintMs = millis();
    uint8_t status = 0xFF;
    uint8_t outputEnable = 0xFF;
    uint8_t clk0Control = 0xFF;
    uint8_t clk1Control = 0xFF;
    if (readSi5351Reg(SI5351_REG_DEVICE_STATUS, &status)) {
      Serial.print("SI5351_CLK1_1MHZ_HEARTBEAT status=");
      printHex8(status);
      readSi5351Reg(SI5351_REG_OUTPUT_ENABLE, &outputEnable);
      readSi5351Reg(SI5351_REG_CLK0_CONTROL, &clk0Control);
      readSi5351Reg(SI5351_REG_CLK1_CONTROL, &clk1Control);
      Serial.print(" oe_reg=");
      printHex8(outputEnable);
      Serial.print(" clk0_ctrl=");
      printHex8(clk0Control);
      Serial.print(" clk1_ctrl=");
      printHex8(clk1Control);
      Serial.println();
    } else {
      Serial.println("SI5351_STATUS_READ_FAILED");
    }
  }
}
