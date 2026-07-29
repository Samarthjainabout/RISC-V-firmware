/*
 * Teensy 4.1 DAC81416 voltage setter for DAC Teensy 000FAF0B.
 *
 * This sketch is a focused replacement for the power-up portion of
 * DAC_analog_vltgs.ino. It avoids Arduino map() for floating-point voltages
 * and only configures DAC1, which is the DAC bank shown in the setup diagram.
 *
 * Teensy 4.1 -> DAC81416 wiring from DAC_read.h / setup diagram:
 *   pin 2  -> DAC1 CS
 *   pin 3  -> DAC2 CS, kept idle high
 *   pin 6  -> LDAC, driven low for immediate updates
 *   pin 26 -> SPI1 MOSI
 *   pin 27 -> SPI1 SCK
 *
 * DAC1 channel map from Setup_block_diagram:
 *   DAC[0]  -> Vcc_read       = 0.0 V at idle
 *   DAC[1]  -> Vcc_wl_read    = 0.0 V at idle
 *   DAC[2]  -> Vcc_set        = 0.0 V at idle, through shunt
 *   DAC[3]  -> Vcc_wl_set     = 0.0 V at idle
 *   DAC[4]  -> Vcc_wl_reset   = 0.0 V at idle
 *   DAC[5]  -> Vcc_reset      = 0.0 V at idle
 *   DAC[9]  -> Iref           = 0.5 V
 *   DAC[10] -> Vcomp          = 2.0 V
 *   DAC[11] -> Bias_comp2     = 0.6 V
 *   DAC[12] -> Vbias          = 1.6 V
 *   DAC[13] -> dc_bias        = 1.0 V
 *   DAC[14] -> VDDa1          = 5.0 V
 *   DAC[15] -> VDDc2/Vccd2    = 2.1 V
 *
 * DAC[6], DAC[7], and DAC[8] are left at 0.0 V because the setup diagram
 * marks DAC[6]/DAC[7]/DAC[8] as not connected in the supplied Caravel map.
 */

#include <Arduino.h>
#include <SPI.h>

static constexpr uint8_t PIN_DAC1_CS = 2;
static constexpr uint8_t PIN_DAC2_CS = 3;
static constexpr uint8_t PIN_LDAC = 6;
static constexpr uint8_t PIN_SPI1_MOSI = 26;
static constexpr uint8_t PIN_SPI1_SCK = 27;
static constexpr uint8_t PIN_LED = LED_BUILTIN;

static constexpr uint8_t NUM_DAC_CHANNELS = 16;
static constexpr float DAC_FULL_SCALE_VOLTS = 5.0f;

static SPISettings dac_spi_settings(4000000, MSBFIRST, SPI_MODE1);
static float current_iref_volts = 0.5f;
static float current_vcomp_volts = 2.0f;
static float current_bias_comp2_volts = 0.6f;
static float current_vbias_volts = 1.6f;
static float current_dc_bias_volts = 1.0f;
static float current_vdda1_volts = 5.0f;
static float current_vddc2_volts = 2.1f;

struct DacTarget {
  uint8_t channel;
  const char *label;
  float volts;
};

static constexpr DacTarget TARGETS[] = {
    {0, "Vcc_read", 0.0f},
    {1, "Vcc_wl_read", 0.0f},
    {2, "Vcc_set", 0.0f},
    {3, "Vcc_wl_set", 0.0f},
    {4, "Vcc_wl_reset", 0.0f},
    {5, "Vcc_reset", 0.0f},
    {6, "unmapped_DAC6", 0.0f},
    {7, "unmapped_DAC7", 0.0f},
    {8, "unmapped_DAC8", 0.0f},
    {9, "Iref", 0.5f},
    {10, "Vcomp", 2.0f},
    {11, "Bias_comp2", 0.6f},
    {12, "Vbias", 1.6f},
    {13, "dc_bias", 1.0f},
    {14, "VDDa1", 5.0f},
    {15, "VDDc2_Vccd2", 2.1f},
};

static uint16_t volts_to_code(float volts)
{
  if (volts <= 0.0f) {
    return 0;
  }
  if (volts >= DAC_FULL_SCALE_VOLTS) {
    return 65535U;
  }
  return (uint16_t)((volts * 65535.0f / DAC_FULL_SCALE_VOLTS) + 0.5f);
}

static void dac_transfer(uint8_t cs_pin, uint8_t address, uint16_t data)
{
  SPI1.beginTransaction(dac_spi_settings);
  digitalWriteFast(cs_pin, LOW);
  SPI1.transfer(address);
  SPI1.transfer((uint8_t)(data >> 8));
  SPI1.transfer((uint8_t)(data & 0xffU));
  digitalWriteFast(cs_pin, HIGH);
  SPI1.endTransaction();
  delayMicroseconds(5);
}

static void dac1_write_register(uint8_t address, uint16_t data)
{
  dac_transfer(PIN_DAC1_CS, address, data);
}

static void dac1_write_channel(uint8_t channel, float volts)
{
  dac1_write_register((uint8_t)(0x10U + (channel & 0x0fU)), volts_to_code(volts));
}

static void dac1_init(void)
{
  /*
   * Power the device and internal reference before enabling the output amps.
   * GENCONFIG uses 0x3F00 to preserve the reset value of reserved bits while
   * clearing REF-PWDWN, leaving all channels single-ended.
   */
  dac1_write_register(0x03, 0x0A84);
  dac1_write_register(0x04, 0x3F00);
  delay(10);

  /*
   * Span registers 0x0A..0x0D set all channels to 0..5 V. SYNC disabled means
   * output updates happen immediately on CS rising edge.
   */
  dac1_write_register(0x0A, 0x0000);
  dac1_write_register(0x0B, 0x0000);
  dac1_write_register(0x0C, 0x0000);
  dac1_write_register(0x0D, 0x0000);
  dac1_write_register(0x06, 0x0000);
  dac1_write_register(0x09, 0x0000);
  dac1_write_register(0x05, 0xffff);
}

static void apply_targets(void)
{
  for (uint8_t channel = 0; channel < NUM_DAC_CHANNELS; channel++) {
    dac1_write_channel(channel, 0.0f);
  }

  for (const DacTarget &target : TARGETS) {
    dac1_write_channel(target.channel, target.volts);
  }

  dac1_write_channel(9, current_iref_volts);
  dac1_write_channel(10, current_vcomp_volts);
  dac1_write_channel(11, current_bias_comp2_volts);
  dac1_write_channel(12, current_vbias_volts);
  dac1_write_channel(13, current_dc_bias_volts);
  dac1_write_channel(14, current_vdda1_volts);
  dac1_write_channel(15, current_vddc2_volts);
}

static void zero_all(void)
{
  for (uint8_t channel = 0; channel < NUM_DAC_CHANNELS; channel++) {
    dac1_write_channel(channel, 0.0f);
  }
}

static void print_targets(void)
{
  Serial.println("[DAC-SETTER] Expected DAC1 outputs:");
  for (const DacTarget &target : TARGETS) {
    float volts = target.volts;
    if (target.channel == 9) {
      volts = current_iref_volts;
    } else if (target.channel == 10) {
      volts = current_vcomp_volts;
    } else if (target.channel == 11) {
      volts = current_bias_comp2_volts;
    } else if (target.channel == 12) {
      volts = current_vbias_volts;
    } else if (target.channel == 13) {
      volts = current_dc_bias_volts;
    } else if (target.channel == 14) {
      volts = current_vdda1_volts;
    } else if (target.channel == 15) {
      volts = current_vddc2_volts;
    }
    Serial.print("[DAC-SETTER] DAC[");
    Serial.print(target.channel);
    Serial.print("] ");
    Serial.print(target.label);
    Serial.print(" = ");
    Serial.print(volts, 3);
    Serial.print(" V code=");
    Serial.println(volts_to_code(volts));
  }
}

static void set_bias_comp2(float volts)
{
  if (volts < 0.0f || volts > DAC_FULL_SCALE_VOLTS) {
    Serial.println("[DAC-SETTER] ERROR voltage out of range 0.000..5.000 V");
    return;
  }

  current_bias_comp2_volts = volts;
  dac1_write_channel(11, current_bias_comp2_volts);

  Serial.print("[DAC-SETTER] Bias_comp2 DAC[11] = ");
  Serial.print(current_bias_comp2_volts, 3);
  Serial.print(" V code=");
  Serial.println(volts_to_code(current_bias_comp2_volts));
}

static bool set_named_rail(const String &name, float volts)
{
  if (volts < 0.0f || volts > DAC_FULL_SCALE_VOLTS) {
    Serial.println("[DAC-SETTER] ERROR voltage out of range 0.000..5.000 V");
    return true;
  }

  uint8_t channel = 255;
  const char *label = nullptr;
  float *current_value = nullptr;

  if (name == "iref") {
    channel = 9;
    label = "Iref";
    current_value = &current_iref_volts;
  } else if (name == "vcomp") {
    channel = 10;
    label = "Vcomp";
    current_value = &current_vcomp_volts;
  } else if (name == "bias_comp2" || name == "bias" || name == "dac11") {
    channel = 11;
    label = "Bias_comp2";
    current_value = &current_bias_comp2_volts;
  } else if (name == "vbias") {
    channel = 12;
    label = "Vbias";
    current_value = &current_vbias_volts;
  } else if (name == "dc_bias" || name == "dcbias") {
    channel = 13;
    label = "dc_bias";
    current_value = &current_dc_bias_volts;
  } else if (name == "vdda1") {
    channel = 14;
    label = "VDDa1";
    current_value = &current_vdda1_volts;
  } else if (name == "vddc2" || name == "vccd2") {
    channel = 15;
    label = "VDDc2_Vccd2";
    current_value = &current_vddc2_volts;
  } else {
    return false;
  }

  *current_value = volts;
  dac1_write_channel(channel, volts);

  Serial.print("[DAC-SETTER] ");
  Serial.print(label);
  Serial.print(" DAC[");
  Serial.print(channel);
  Serial.print("] = ");
  Serial.print(volts, 3);
  Serial.print(" V code=");
  Serial.println(volts_to_code(volts));
  return true;
}

static bool parse_set_command(const String &command, String *name, float *volts)
{
  String value_text;

  int separator = command.indexOf(' ');
  int equals = command.indexOf('=');
  if (equals >= 0 && (separator < 0 || equals < separator)) {
    separator = equals;
  }

  if (separator > 0) {
    *name = command.substring(0, separator);
    value_text = command.substring(separator + 1);
  } else if (command.startsWith("bias_comp2")) {
    *name = "bias_comp2";
    value_text = command.substring(10);
  } else if (command.startsWith("bias")) {
    *name = "bias";
    value_text = command.substring(4);
  } else if (command.startsWith("dac11")) {
    *name = "dac11";
    value_text = command.substring(5);
  } else {
    return false;
  }

  value_text.trim();
  if (value_text.startsWith("=")) {
    value_text = value_text.substring(1);
    value_text.trim();
  }

  if (value_text.length() == 0) {
    Serial.println("[DAC-SETTER] ERROR missing voltage. Use: dc_bias 1.000");
    return true;
  }

  *volts = value_text.toFloat();
  return true;
}

static void process_serial(void)
{
  if (Serial.available() == 0) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();

  if (command == "apply") {
    apply_targets();
    Serial.println("[DAC-SETTER] applied targets");
  } else if (command == "zero") {
    zero_all();
    Serial.println("[DAC-SETTER] all channels set to 0 V");
  } else {
    float volts = 0.0f;
    String name;
    if (parse_set_command(command, &name, &volts)) {
      set_named_rail(name, volts);
      return;
    }

    if (command == "status" || command == "help" || command == "?") {
      print_targets();
      Serial.println("[DAC-SETTER] commands: apply, zero, status, iref <V>, vcomp <V>, bias_comp2 <V>, vbias <V>, dc_bias <V>");
    }
  }
}

void setup()
{
  pinMode(PIN_DAC1_CS, OUTPUT);
  pinMode(PIN_DAC2_CS, OUTPUT);
  pinMode(PIN_LDAC, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  digitalWriteFast(PIN_DAC1_CS, HIGH);
  digitalWriteFast(PIN_DAC2_CS, HIGH);
  digitalWriteFast(PIN_LDAC, LOW);
  digitalWriteFast(PIN_LED, LOW);

  SPI1.setMOSI(PIN_SPI1_MOSI);
  SPI1.setSCK(PIN_SPI1_SCK);
  SPI1.begin();

  Serial.begin(115200);
  delay(200);

  dac1_init();
  apply_targets();

  Serial.println("[DAC-SETTER] booted and applied DAC1 targets");
  print_targets();
}

void loop()
{
  process_serial();

  static uint32_t last_blink_ms = 0;
  if (millis() - last_blink_ms >= 1000) {
    last_blink_ms = millis();
    digitalToggleFast(PIN_LED);
  }
}
