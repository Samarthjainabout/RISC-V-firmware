// Teensy 4.1 usb port ----> usb;xxxxxxx/4/0/2

#include "DAC_read.h"
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include<math.h>
#include "TeensyThreads.h"
#include <WS2812Serial.h>


//Debug LED Pins
#define setLed 30
#define resetLed 29
#define readLed 28
#define activeLed 32
#define faultLed 31
#define ws2812s1_pin 1
#define ws2812s2_pin 8


//WS2812 Color Codex
#define RED    0x00FF0000
#define GREEN  0x0000FF00
#define BLUE   0x000000FF
#define YELLOW 0x00FFD000
#define PINK   0x44F00080
#define ORANGE 0x00FF4200
#define WHITE  0xAA000000


// Usable pins:
//   Teensy LC:   1, 4, 5, 24
//   Teensy 3.2:  1, 5, 8, 10, 31   (overclock to 120 MHz for pin 8)
//   Teensy 3.5:  1, 5, 8, 10, 26, 32, 33, 48
//   Teensy 3.6:  1, 5, 8, 10, 26, 32, 33
//   Teensy 4.0:  1, 8, 14, 17, 20, 24, 29, 39
//   Teensy 4.1:  1, 8, 14, 17, 20, 24, 29, 35, 47, 53

Adafruit_ADS1115 ads1, ads2;
static const uint8_t ADS1_ADDR = 0x48;
static const uint8_t ADS2_ADDR = 0x49;
static const int MONITOR_DEFAULT_SET_MV = 500;
static const int MONITOR_DEFAULT_RESET_MV = 500;
static const unsigned long MONITOR_DEFAULT_HOLD_MS = 150;
bool ads1Ok = false;
bool ads2Ok = false;
byte drawingMemory1[4];         //  4 bytes per LED for RGBW
byte drawingMemory2[4];         //  4 bytes per LED for RGBW
DMAMEM byte displayMemory1[16];
DMAMEM byte displayMemory2[16]; // 16 bytes per LED for RGBW
WS2812Serial led1(1, displayMemory1, drawingMemory1, ws2812s1_pin, WS2812_GRBW);
WS2812Serial led2(1, displayMemory2, drawingMemory2, ws2812s2_pin, WS2812_GRBW);
String command = "";

/*.............................GLOBAL PARAMETERS.........................................................*/

float set_vi = 0.1, set_vlim = 2, set_vstep = 0.01; // Initial voltage, final voltage & voltage steps for SET
float reset_vi = 0.1, reset_vlim = 3.5, reset_vstep = 0.01; // Initial voltage, final voltage & voltage steps for RESET
float setv = -3.5, resetv = 4.5; // I dunno what it is...
float setECC_v = 0, resetECC_v = 2.56; // The set & reset ECC voltage pulse amplitude ( ECC --> Error Correcting Code )
float read_v = 1.7; // Vcc_read(min =1.3, max = 1.7)
float reRAM_res = 0, shunt_res = 1000; // Measured resistance of the ReRAM during the last read cycle & Shunt Resistor Ohm
float currentSense_gain = 1.0; // Transimpedance Amp V/mA
float ads_diff_multiplier = 0.0078125F; //mV for ADC count for GAIN_SIXTEEN
//float ads_diff_multiplier = 0.125F; // mV per ADC count for GAIN_ONE
//float ads_diff_multiplier = 0.0625F;  // mV per ADC count for GAIN_TWO
//float ads_diff_multiplier = 0.03125F;  // mV per ADC count for GAIN_FOUR
//float marginHRSL = 0.05, marginHRSH = 0.05; // HRS Low & High limits for a bounded condition checking.
//float marginLRSL = 0.05, marginLRSH = 0.05; // LRS Low & High limits for a bounded condition checking.
float marginHRSL = 0.025, marginHRSH = 0.025; // HRS Low & High limits for a bounded condition checking.
float marginLRSL = 0.025, marginLRSH = 0.025; // LRS Low & High limits for a bounded condition checking.
int setPulsePeriod = 19, resetPulsePeriod = 50705; // Set & Reset Pulse periods.
int setECC_period = 0, resetECC_period = 3121204; // Set & Reset ECC Pulse periods
int setIter = 80, resetIter = 80, setECC_iter = 80, resetECC_iter = 80; //No. of "d_iter" iterations for both set, reset & respective ECCs
int setlimit = 8000, resetlimit = 40000; // LRF & HRF of the test device
int mode = 0; // Mode selector: SET: 0 / RESET: 1 / SMTG: -1
int cBrd1 = 1, cBrd2 = 2, cBrd3 = 3, cBrd4 = 4, steps = 0, d_iter = 0;
int ldac_Pin = 4;

/*..............................DAC - pins..............................................................*/

uint8_t u1bl1 = dac[1], u1bl2 = dac[0], u1wl1 = dac[2], u1sl1 = dac[11];
uint8_t u2bl1 = dac[5], u2bl2 = dac[4], u2wl1 = dac[6], u2sl1 = dac[15];
uint8_t u3bl1 = dac[9], u3bl2 = dac[8], u3wl1 = dac[10], u3sl1 = dac[3];
uint8_t u4bl1 = dac[13], u4bl2 = dac[12], u4wl1 = dac[14], u4sl1 = dac[7];

/*.............................CONVERTED PARAMETERS......................................................*/

int m_setvi = 1000 * set_vi, m_set_vlim = 1000 * set_vlim, m_setvstep = 1000 * set_vstep; // Conversion FP32 to int for faster SET operation.
int m_resetvi = 1000 * reset_vi, m_reset_vlim = 1000 * reset_vlim, m_resetvstep = 1000 * reset_vstep; // Conversion FP32 to int for faster RESET operation.
int read_mv = 1000 * read_v; // Conversion FP32 to int.
int setmv = 1000 * setv, resetmv = 1000 * resetv; // Conversion FP32 to int.
int setECC_mv = 1000 * setECC_v, resetECC_mv = 1000 * resetECC_v;
int shunt_c = 1 / shunt_res; // Shunt conductance value as uC have trouble dividing.
int16_t ads_c0, ads_c1, ads_c2, ads_c3; //ADS1115 16b RAW Transfer Buffer in unsigned int 16b.
float ads_mv0, ads_mv1, ads_mv2, ads_mv3; // ADS1115 Converted Voltage in mV.
float vwl1, vwl2, vwl3, vwl4; // Export variables for U1WL1, U2WL1, U3WL1, U4WL1
float vu1bl1, vu1bl2, vu2bl1, vu2bl2, vu3bl1, vu3bl2, vu4bl1, vu4bl2, vu1wl1, vu2wl1, vu3wl1, vu4wl1, vu1sl1, vu2sl1, vu3sl1, vu4sl1; // Export variables applied set voltages of U1BL1 -> U2BL2 & U1SL1 -> U2SL1
float vSh_u1bl1, vSh_u1bl2, vSh_u2bl1, vSh_u2bl2, vSh_u3bl1, vSh_u3bl2, vSh_u4bl1, vSh_u4bl2,
      vRRAM_u1bl1, vRRAM_u1bl2, vRRAM_u2bl1, vRRAM_u2bl2, vRRAM_u3bl1, vRRAM_u3bl2, vRRAM_u4bl1, vRRAM_u4bl2; // Export variables for Vshunt & VRRAM
float res_u1bl1 = 0, res_u1bl2  = 0, res_u2bl1  = 0, res_u2bl2  = 0, res_u3bl1  = 0, res_u3bl2  = 0, res_u4bl1  = 0, res_u4bl2  = 0; // Export variable for RES of U1BL1 -> U2BL2
float iU1BL1, iU1BL2, iU2BL1, iU2BL2, iU3BL1, iU3BL2, iU4BL1, iU4BL2; // Export variable for current of U1BL1 -> U2BL2
String addr_hex = "SET || LA0: 0xDEADC0DE LA1: 0xFFFFFFFF";

struct ShuntCurrents {
  float readMv;
  float setMv;
  float resetMv;
  float readUa;
  float setUa;
  float resetUa;
};

struct MonitorStats {
  ShuntCurrents first;
  ShuntCurrents last;
  ShuntCurrents minVal;
  ShuntCurrents maxVal;
  ShuntCurrents peakDelta;
  unsigned int samples;
};

float mvToShuntUa(float mv) {
  return (mv * 1000.0f) / shunt_res;
}

int clampInt(int value, int lo, int hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

unsigned long clampUlong(unsigned long value, unsigned long lo, unsigned long hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

float absDelta(float current, float baseline) {
  return fabs(current - baseline);
}

void setPacketDacsZero() {
  dac_write(dac[0], 0);
  dac_write(dac[2], 0);
  dac_write(dac[5], 0);
}

void writeDacMilliVolts(uint8_t channel, int mv) {
  mv = clampInt(mv, 0, 5000);
  dac_write(channel, (mv * 100.0f) / 5000.0f);
}

ShuntCurrents readMonitoredShunts() {
  ShuntCurrents sample;
  sample.readMv = ads1Ok ? ads1.readADC_Differential_0_1() * ads_diff_multiplier : 0.0f;
  sample.setMv = ads1Ok ? ads1.readADC_Differential_2_3() * ads_diff_multiplier : 0.0f;
  sample.resetMv = ads2Ok ? ads2.readADC_Differential_0_1() * ads_diff_multiplier : 0.0f;
  sample.readUa = mvToShuntUa(sample.readMv);
  sample.setUa = mvToShuntUa(sample.setMv);
  sample.resetUa = mvToShuntUa(sample.resetMv);

  vSh_u1bl1 = sample.readMv;
  iU1BL1 = sample.readUa;
  vSh_u2bl1 = sample.setMv;
  iU2BL1 = sample.setUa;
  vSh_u3bl1 = sample.resetMv;
  iU3BL1 = sample.resetUa;
  return sample;
}

void initMonitorStats(MonitorStats &stats, const ShuntCurrents &sample) {
  stats.first = sample;
  stats.last = sample;
  stats.minVal = sample;
  stats.maxVal = sample;
  stats.peakDelta.readUa = 0.0f;
  stats.peakDelta.setUa = 0.0f;
  stats.peakDelta.resetUa = 0.0f;
  stats.samples = 1;
}

void updateOneCurrent(float value, float baseline, float &minValue, float &maxValue, float &peakDeltaValue) {
  if (value < minValue) minValue = value;
  if (value > maxValue) maxValue = value;
  float delta = absDelta(value, baseline);
  if (delta > peakDeltaValue) peakDeltaValue = delta;
}

void updateMonitorStats(MonitorStats &stats, const ShuntCurrents &sample) {
  stats.last = sample;
  stats.samples++;
  updateOneCurrent(sample.readUa, stats.first.readUa, stats.minVal.readUa, stats.maxVal.readUa, stats.peakDelta.readUa);
  updateOneCurrent(sample.setUa, stats.first.setUa, stats.minVal.setUa, stats.maxVal.setUa, stats.peakDelta.setUa);
  updateOneCurrent(sample.resetUa, stats.first.resetUa, stats.minVal.resetUa, stats.maxVal.resetUa, stats.peakDelta.resetUa);
}

void printMonitorSample(const char *packetName, unsigned long elapsedUs, const ShuntCurrents &sample) {
  Serial.print("MONITOR_SAMPLE packet=");
  Serial.print(packetName);
  Serial.print(" t_us=");
  Serial.print(elapsedUs);
  Serial.print(" read_uA=");
  Serial.print(sample.readUa, 4);
  Serial.print(" set_uA=");
  Serial.print(sample.setUa, 4);
  Serial.print(" reset_uA=");
  Serial.println(sample.resetUa, 4);
}

void printShuntSnapshot(const char *label) {
  ShuntCurrents sample = readMonitoredShunts();
  Serial.print("SHUNT_SNAPSHOT label=");
  Serial.print(label);
  Serial.print(" read_uA=");
  Serial.print(sample.readUa, 4);
  Serial.print(" set_uA=");
  Serial.print(sample.setUa, 4);
  Serial.print(" reset_uA=");
  Serial.println(sample.resetUa, 4);
}

void printPacketSummary(const char *packetName, const MonitorStats &stats) {
  Serial.print("PACKET_SUMMARY packet=");
  Serial.print(packetName);
  Serial.print(" samples=");
  Serial.print(stats.samples);

  Serial.print(" read_base_uA=");
  Serial.print(stats.first.readUa, 4);
  Serial.print(" read_end_uA=");
  Serial.print(stats.last.readUa, 4);
  Serial.print(" read_end_delta_uA=");
  Serial.print(stats.last.readUa - stats.first.readUa, 4);
  Serial.print(" read_span_uA=");
  Serial.print(stats.maxVal.readUa - stats.minVal.readUa, 4);
  Serial.print(" read_peak_delta_uA=");
  Serial.print(stats.peakDelta.readUa, 4);

  Serial.print(" set_base_uA=");
  Serial.print(stats.first.setUa, 4);
  Serial.print(" set_end_uA=");
  Serial.print(stats.last.setUa, 4);
  Serial.print(" set_end_delta_uA=");
  Serial.print(stats.last.setUa - stats.first.setUa, 4);
  Serial.print(" set_span_uA=");
  Serial.print(stats.maxVal.setUa - stats.minVal.setUa, 4);
  Serial.print(" set_peak_delta_uA=");
  Serial.print(stats.peakDelta.setUa, 4);

  Serial.print(" reset_base_uA=");
  Serial.print(stats.first.resetUa, 4);
  Serial.print(" reset_end_uA=");
  Serial.print(stats.last.resetUa, 4);
  Serial.print(" reset_end_delta_uA=");
  Serial.print(stats.last.resetUa - stats.first.resetUa, 4);
  Serial.print(" reset_span_uA=");
  Serial.print(stats.maxVal.resetUa - stats.minVal.resetUa, 4);
  Serial.print(" reset_peak_delta_uA=");
  Serial.println(stats.peakDelta.resetUa, 4);
}

void runMonitoredDacPacket(const char *packetName, uint8_t dacChannel, int mv, unsigned long holdMs) {
  ShuntCurrents sample = readMonitoredShunts();
  MonitorStats stats;
  initMonitorStats(stats, sample);

  Serial.print("PACKET_BEGIN packet=");
  Serial.print(packetName);
  Serial.print(" dac=0x");
  Serial.print(dacChannel, HEX);
  Serial.print(" target_mV=");
  Serial.print(mv);
  Serial.print(" hold_ms=");
  Serial.println(holdMs);
  printMonitorSample(packetName, 0, sample);

  writeDacMilliVolts(dacChannel, mv);
  unsigned long startUs = micros();
  while ((micros() - startUs) < (holdMs * 1000UL)) {
    sample = readMonitoredShunts();
    updateMonitorStats(stats, sample);
    printMonitorSample(packetName, micros() - startUs, sample);
    delay(1);
  }

  writeDacMilliVolts(dacChannel, 0);
  sample = readMonitoredShunts();
  updateMonitorStats(stats, sample);
  printMonitorSample(packetName, micros() - startUs, sample);
  printPacketSummary(packetName, stats);
}

void parseRsrrCommand(const String &cmd, int &seqReadMv, int &seqSetMv, int &seqResetMv, unsigned long &holdMs) {
  seqReadMv = read_mv;
  seqSetMv = MONITOR_DEFAULT_SET_MV;
  seqResetMv = MONITOR_DEFAULT_RESET_MV;
  holdMs = MONITOR_DEFAULT_HOLD_MS;

  int readArg = seqReadMv;
  int setArg = seqSetMv;
  int resetArg = seqResetMv;
  unsigned long holdArg = holdMs;
  int parsed = sscanf(cmd.c_str(), "%*s %d %d %d %lu", &readArg, &setArg, &resetArg, &holdArg);
  if (parsed >= 1) seqReadMv = readArg;
  if (parsed >= 2) seqSetMv = setArg;
  if (parsed >= 3) seqResetMv = resetArg;
  if (parsed >= 4) holdMs = holdArg;

  seqReadMv = clampInt(seqReadMv, 0, 5000);
  seqSetMv = clampInt(seqSetMv, 0, 5000);
  seqResetMv = clampInt(seqResetMv, 0, 5000);
  holdMs = clampUlong(holdMs, 20, 5000);
}

void runReadSetReadResetSequence(const String &cmd) {
  int seqReadMv;
  int seqSetMv;
  int seqResetMv;
  unsigned long holdMs;
  parseRsrrCommand(cmd, seqReadMv, seqSetMv, seqResetMv, holdMs);

  mode = 5;
  setPacketDacsZero();
  delay(20);

  Serial.print("RSRR_BEGIN read_mV=");
  Serial.print(seqReadMv);
  Serial.print(" set_mV=");
  Serial.print(seqSetMv);
  Serial.print(" reset_mV=");
  Serial.print(seqResetMv);
  Serial.print(" hold_ms=");
  Serial.print(holdMs);
  Serial.print(" ads1=0x");
  Serial.print(ADS1_ADDR, HEX);
  Serial.print(ads1Ok ? ":OK" : ":FAIL");
  Serial.print(" ads2=0x");
  Serial.print(ADS2_ADDR, HEX);
  Serial.println(ads2Ok ? ":OK" : ":FAIL");

  // Hardware check showed the original read/set DAC packet paths were swapped.
  runMonitoredDacPacket("READ1", dac[2], seqReadMv, holdMs);
  delay(20);
  runMonitoredDacPacket("SET", dac[0], seqSetMv, holdMs);
  delay(20);
  runMonitoredDacPacket("READ2", dac[2], seqReadMv, holdMs);
  delay(20);
  runMonitoredDacPacket("RESET", dac[5], seqResetMv, holdMs);
  delay(20);
  runMonitoredDacPacket("READ3", dac[2], seqReadMv, holdMs);

  setPacketDacsZero();
  delay(20);
  Serial.println("PACKET_DACS_ZEROED dac2=0mV dac0=0mV dac5=0mV");
  printShuntSnapshot("FINAL_ZERO");
  Serial.println("RSRR_DONE");
}


void setup() {
  Serial.begin(115200);
  init_communication();
  Serial.println("communication init done");
  init_dac();
  Serial.println("DAC init done");
  led1.begin();
  led2.begin();
  led1.setBrightness(50);
  led2.setBrightness(50);
//  pinMode(ldacPin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(setLed, OUTPUT);
  pinMode(resetLed, OUTPUT);
  pinMode(readLed, OUTPUT);
  pinMode(activeLed, OUTPUT);
  pinMode(faultLed, OUTPUT);

  // ADS1115 Initialization
  Wire.begin();
  ads1.setGain(GAIN_SIXTEEN);
  ads1.setDataRate(RATE_ADS1115_860SPS);
  ads2.setGain(GAIN_SIXTEEN);
  ads2.setDataRate(RATE_ADS1115_860SPS);
  // ads1.setGain(GAIN_TWO);  // ±1.024 V differential range
  // ads1.setDataRate(RATE_ADS1115_16SPS);

  // ads2.setGain(GAIN_TWO);  // ±1.024 V differential range
  // ads2.setDataRate(RATE_ADS1115_16SPS);
  ads1Ok = ads1.begin(ADS1_ADDR, &Wire);
  if (!ads1Ok) {
    Serial.println("Failed to initialize ADS1.");
    while (1);
  }
  ads2Ok = ads2.begin(ADS2_ADDR, &Wire);
  Serial.print("ADS2 0x");
  Serial.print(ADS2_ADDR, HEX);
  Serial.print(" init: ");
  Serial.println(ads2Ok ? "OK" : "FAIL");

  // --- POWER-UP SEQUENCE ---
  // Option A: Start with all channels at 0V, then apply specific voltages
  // This ensures no unexpected voltages on unused channels
  dac_stndby();  // All channels = 0V
  dac_powerup_continuous();
  d_iter = 0;
  //indicator_test();
}


int flag = 0;
float sum = 0.0;
bool isReset = false, isSet = false, newCmd = false;
int srCycles = 0;

void loop(){
  //dac_powerup_continuous_nonblocking();
  //dac_powerup_continuous();
  if (flag != 0 || Serial.available() <= 0) {
    return;
  }

  command = Serial.readStringUntil('\n'); // Read until newline character
  command.trim(); // Remove any extra whitespace or newline
  if (command.length() == 0) {
    return;
  }

  if (command == "SET") {
            mode = 1;
            //test_set(u2bl1, u2wl1, 0,0.9, 0.1, 0.7, 0.01, 10); //test_set(BL Chanel, WL Channel, BL_Start, BL_Stop, WL_Start, WL_Stop, No. of Iteration);
            for(float wl = 0; wl < 0.9; wl += 0.1){
                for(float set = 0; set < 2; set += 0.1){
                  dac_write(u1sl1, 0);
                  dac_write(u1wl1, map(wl, 0, 5, 0, 100));
                  dac_write(u1bl1, map(set, 0, 5, 0, 100));
                  u1bl1 = map(set, 0, 5, 0, 100);
                  u1wl1 = map(wl, 0, 5, 0, 100);
                  delayMicroseconds(10);
                  dac_write(u1bl1, 0);
                  dac_write(u1wl1, 0);
                  delayMicroseconds(10);
                  read_all(1);
                  steps++;
                  loggerOut();
                  if(res_u1bl1 <= 20000 && res_u1bl1 != 0){
                    dac_write(u1sl1, 0);
                    dac_write(u1wl1, 0);
                    dac_write(u1bl1, 0);
                    digitalWrite(LED_BUILTIN, HIGH);
                    while(1);
                    }
                }
              }
            command = "";
        } else if (command == "CLEAR") {
            mode = 2;
            //test_reset(u2bl1, u2wl1, 0, 1.5, 0.1, 0.7, 0.01, 10);
            
              for(float wl = 1.4; wl < 3; wl += 0.1){
                for(float rst = 0; rst < 3.5; rst += 0.1){
                  dac_write(u1bl1, 0);
                  dac_write(u1wl1, map(wl, 0, 5, 0, 100));
                  dac_write(u1sl1, map(rst, 0, 5, 0, 100));
                  u1bl1 = map(rst, 0, 5, 0, 100);
                  u1wl1 = map(wl, 0, 5, 0, 100);
                  delayMicroseconds(10);
                  dac_write(u1sl1, 0);
                  dac_write(u1wl1, 0);
                  delayMicroseconds(10);
                  read_all(1);
                  steps++;
                  loggerOut();
                  if(res_u1bl1 >= 375000){
                    dac_write(u1sl1, 0);
                    dac_write(u1wl1, 0);
                    dac_write(u1bl1, 0);
                    digitalWrite(LED_BUILTIN, HIGH);
                    while(1);
                    }
                  }
              }
            //dac_write(u1sl1, map(rst, 0, 5, 0, 100));
            command = "";
        } else if (command == "LOOP") {
            //blink(1000);
            mode = 3;
            while(1){

              
			  setting_constant_Pulse();
			  //resetting_constant_Pulse();
              if(srCycles > 1000)break;
            }
            command = "";
        } else if (command == "VERIFY") {
            //digitalWrite(LED_BUILTIN, HIGH);
            mode = 4;
            read_all(1000000);
            command = "";
        } else if (command.startsWith("RSRR") || command.startsWith("PACKETS")) {
            runReadSetReadResetSequence(command);
            command = "";
        } else {
          mode = 0;
          dac_stndby();
        }
}

 String operation_state = "-";




// ===== SET FUNCTION =====
// BL (u1wl1) = 2.6V, WL (u3sl1) = 1.7V, SL (u1bl2) = 0V
void setting_constant_Pulse() {
  bool isSet = false;
  float res_inloop = 0.0;
  String operation_state = "-";

  while (!isSet) {
    read_all_set(1);
    res_inloop = res_u1bl1; 

    // Apply constant SET pulse
    dac_write(u2bl1, 0);                          // SL = 0V (using u1bl2)
    dac_write(u3sl1, map(2.5, 0, 5, 0, 100));     // WL = 1.7V (using u3sl1)
    dac_write(u1wl1, map(3.5, 0, 5, 0, 100));     // BL = 2.6V (using u1wl1)

    // Store voltages for logging
    vu1bl2 = 0;                                   // SL = 0V
    vu3sl1 = map(1.7, 0, 5, 0, 100);              // WL = 1.7V
    vu1wl1 = map(2.6, 0, 5, 0, 100);              // BL = 2.6V

    delayMicroseconds(10);  // Pulse duration

    // Turn off pulses
    dac_write(u1wl1, 0);
    dac_write(u3sl1, 0);
    dac_write(u2bl1, 0);
    delayMicroseconds(10);

    steps++;
    loggerOut();

    // Condition to stop setting (LRS target resistance)
    if (res_inloop <= 20000 && res_inloop > 0) {
      dac_write(u2bl1, 0);
      dac_write(u3sl1, 0);
      dac_write(u1wl1, 0);
      digitalWrite(LED_BUILTIN, HIGH);
      isSet = true;
      operation_state = "1"; // Set successful
      loggerOut();
      operation_state = "-";
    }
  }

  dac_stndby();
  while (1);
}

// ===== RESET FUNCTION =====
// BL (u1wl1) = 0V, WL (u2bl2) = 2.6V, SL (u1bl2) = 3.2V
void resetting_constant_Pulse() {
  bool isReset = false;
  float res_inloop = 0.0;
  String operation_state = "-";

  while (!isReset) {
    read_all_set(1);
    res_inloop = res_u1bl2;  // Reading on BL2 (u1bl2) - SL line

    // Apply constant RESET pulse
    dac_write(u2bl1, 0);                          // BL = 0V (using u1wl1)
    dac_write(u3sl1, map(2.6, 0, 5, 0, 100));     // WL = 2.6V (using u2bl2)
    dac_write(u1wl1, map(3.2, 0, 5, 0, 100));     // SL = 3.2V (using u1bl2)

    // Store voltages for logging
    vu1wl1 = 0;                                   // BL = 0V
    vu2bl2 = map(2.6, 0, 5, 0, 100);              // WL = 2.6V
    vu1bl2 = map(3.2, 0, 5, 0, 100);              // SL = 3.2V

    delayMicroseconds(10);  // Pulse duration

    // Turn off pulses
    dac_write(u2bl1, 0);
    dac_write(u2bl2, 0);
    dac_write(u1wl1, 0);
    delayMicroseconds(10);

    steps++;
    loggerOut();

    // Condition to stop resetting (HRS target resistance)
    // if (res_inloop >= 50000 && res_inloop < 1000000) {
    //   dac_write(u2bl1, 0);
    //   dac_write(u2bl2, 0);
    //   dac_write(u1wl1, 0);
    //   digitalWrite(LED_BUILTIN, HIGH);
    //   isReset = true;
    //   operation_state = "0"; // Reset successful
    //   loggerOut();
    //   operation_state = "-";
    // }
  }

  dac_stndby();
  while (1);
}

const int resistance_values[3] = {20000, 50919, 66289};
int current_resistanceLRS = resistance_values[0];
int current_resistanceHRS = resistance_values[1];



void read_all(int iter){
  dac_write(u1bl1, 0); //U1WL1 0V7
  // dac_write(u2wl1, 14); //U2WL1 0V7
  // dac_write(u3wl1, 14);
  // dac_write(u4wl1, 14);
  // dac_write(u1sl1, 0); // Read happens in the set direction so, SL to low.
  // dac_write(u2sl1, 0);
  // dac_write(u3sl1, 0);
  // dac_write(u4sl1, 0);
   for(int i = 0; i < iter; i++){
     readPulse(u1bl2, read_mv);
    // //readPulse(u1bl2, read_mv);
    // readPulse(u2bl1, read_mv);
    // //readPulse(u2bl2, read_mv);
    // readPulse(u3bl1, read_mv);
    // //readPulse(u3bl2, read_mv);
    // readPulse(u4bl1, read_mv);
    // //readPulse(u4bl2, read_mv);
    d_iter = i;
    delay(100);
    if(i > 496) flag = 1;
    loggerOut();
   // srCycles += 1;
  }
  dac_write(u1bl1, 0); //U1WL1 0V7
  dac_write(u2wl1, 0); //U2WL1 0V7
}

void read_all_rst(int iter){
  dac_write(u2bl2, 34); //U1WL1 0V7
  // dac_write(u2wl1, 14); //U2WL1 0V7
  // dac_write(u3wl1, 14);
  // dac_write(u4wl1, 14);
  // dac_write(u1sl1, 0); // Read happens in the set direction so, SL to low.
  // dac_write(u2sl1, 0);
  // dac_write(u3sl1, 0);
  // dac_write(u4sl1, 0);
   for(int i = 0; i < iter; i++){
     readPulse(u2bl1, read_mv);
    // //readPulse(u1bl2, read_mv);
    // readPulse(u2bl1, read_mv);
    // //readPulse(u2bl2, read_mv);
    // readPulse(u3bl1, read_mv);
    // //readPulse(u3bl2, read_mv);
    // readPulse(u4bl1, read_mv);
    // //readPulse(u4bl2, read_mv);
    d_iter = i;
    delay(100);
    if(i > 496) flag = 1;
    loggerOut();
   // srCycles += 1;
  }
  dac_write(u2bl1, 0); //U1WL1 0V7
  //dac_write(u2wl1, 0); //U2WL1 0V7
}


void read_all_set(int iter){
  dac_write(u3sl1, 34); //U1WL1 0V7
  // dac_write(u2wl1, 14); //U2WL1 0V7
  // dac_write(u3wl1, 14);
  // dac_write(u4wl1, 14);
  // dac_write(u1sl1, 0); // Read happens in the set direction so, SL to low.
  // dac_write(u2sl1, 0);
  // dac_write(u3sl1, 0);
  // dac_write(u4sl1, 0);
   for(int i = 0; i < iter; i++){
     readPulse(u1wl1, read_mv);
    // //readPulse(u1bl2, read_mv);
    // readPulse(u2bl1, read_mv);
    // //readPulse(u2bl2, read_mv);
    // readPulse(u3bl1, read_mv);
    // //readPulse(u3bl2, read_mv);
    // readPulse(u4bl1, read_mv);
    // //readPulse(u4bl2, read_mv);
    d_iter = i;
    delay(100);
    if(i > 496) flag = 1;
    loggerOut();
   // srCycles += 1;
  }
  dac_write(u3sl1, 0); //U1WL1 0V7
  //dac_write(u2wl1, 0); //U2WL1 0V7
}

void readCell(uint8_t dac_ch, int iter){
 for(int i=0; i < iter; i++){
 sum += readPulse(dac_ch, read_mv);
 d_iter = i;
 //Serial.print("Iter: "); Serial.print(i); Serial.print("\tAvg: "); Serial.println(avg);
 delay(100);
 if(i > 496) flag = 1;
 loggerOut();
  }
}

float readPulse(uint8_t dac_ch, float rd_mV){ //Passing the dac channel will also denote which board is being utilized for that fn. call
  dac_write(dac_ch, map(rd_mV, 0, 5000, 0, 100));
  delay(5);
  if(dac_ch == u1wl1 || dac_ch == u2bl1)reRAM_res = readRes(dac_ch, cBrd1); // For set and reset enable this and comment read one below
  //if(dac_ch == u1bl2 || dac_ch == u2bl1)reRAM_res = readRes(dac_ch, cBrd1); // For only read enable this
  else if(dac_ch == u2bl1 || dac_ch == u2bl2)reRAM_res = readRes(dac_ch, cBrd2);
  else if(dac_ch == u3bl1 || dac_ch == u3bl2)reRAM_res = readRes(dac_ch, cBrd3);
  else if(dac_ch == u4bl1 || dac_ch == u4bl2)reRAM_res = readRes(dac_ch, cBrd4);
  //Serial.print("Read Voltage: "); Serial.print(rd_mV, 3); Serial.print("\tRES:"); Serial.println(reRAM_res);
  delay(5);
  dac_write(dac_ch, 0);
  dac_write(u3sl1, 0);
  dac_write(u2bl2, 0);
  // //  Cancellation pulse on SL = 0.3 V 
  // if(dac_ch == u1bl1 || dac_ch == u1bl2)dac_write(u1sl1, map(rd_mV, 0, 5000, 0, 100));
  // else if(dac_ch == u2bl1 || dac_ch == u2bl2)dac_write(u2sl1, map(rd_mV, 0, 5000, 0, 100));
  // else if(dac_ch == u3bl1 || dac_ch == u3bl2)dac_write(u3sl1, map(rd_mV, 0, 5000, 0, 100));
  // else if(dac_ch == u4bl1 || dac_ch == u4bl2)dac_write(u4sl1, map(rd_mV, 0, 5000, 0, 100));
  // delay(74);
  // // SL back to 0
  // if(dac_ch == u1bl1 || dac_ch == u1bl2)dac_write(u1sl1, 0);
  // else if(dac_ch == u2bl1 || dac_ch == u2bl2)dac_write(u2sl1, 0);
  // else if(dac_ch == u3bl1 || dac_ch == u3bl2)dac_write(u3sl1, 0);
  // else if(dac_ch == u4bl1 || dac_ch == u4bl2)dac_write(u4sl1, 0);
  return reRAM_res;
}


float readRes(uint8_t dac_ch, uint8_t adc_ch){ // Ch 1 -> A0-A1 Ch 2 -> A1-A2
  float res = 0.0;
  if( adc_ch == 1 ){
  ads_c0 = ads1.readADC_Differential_0_1(); // ADC Call to Channel A0 Single Ended.
  ads_mv0 = ads_c0 * ads_diff_multiplier; // Voltage across the shunt resistor
  double current = ads_mv0 * (0.001); // Current passing via the shunt = V x 1/R[sh] (mA)
  res = (read_mv - abs(ads_mv0)) / abs(current); // R[ReRAM] (Ohm) = Read Voltage (mV) - Shunt Voltage (mV) / Current Passing via the shunt. (mA)
  if(dac_ch == u1wl1) {
    //dac_pwrdwn(u1bl2);
    iU1BL1 = abs(current) * 1000;
    vRRAM_u1bl1 = read_mv - abs(ads_mv0);
    vSh_u1bl1 = abs(ads_mv0);
    res_u1bl1 = res;}
  else if(dac_ch == u2bl1) {
    //dac_pwrdwn(u1bl1);
    iU1BL2 = abs(current) * 1000; 
    vRRAM_u1bl2 = read_mv - abs(ads_mv0);
    vSh_u1bl2 = abs(ads_mv0);
    res_u1bl2 = res;
    }
  }
    //Serial.print("Vsh:\t"); Serial.print(ads_mv0); Serial.print("mV"); Serial.print("\tIsh:\t"); Serial.print(current); Serial.println("mA");
  else if( adc_ch == 2 ){
    ads_c1 = ads1.readADC_Differential_2_3(); // ADC Call to Channel A0 Single Ended.
    ads_mv1 = ads_c1 * ads_diff_multiplier; // Voltage across the shunt resistor
    double current = ads_mv1 * (0.001); // Current passing via the shunt = V x 1/R[sh] (mA)
    res = (read_mv - abs(ads_mv1)) / abs(current); // R[ReRAM] (Ohm) = Read Voltage (mV) - Shunt Voltage (mV) / Current Passing via the shunt. (mA) 
    if(dac_ch == u2bl1) { //dac_pwrdwn(u2bl2);
      iU2BL1 = abs(current) * 1000; 
      vRRAM_u2bl1 = read_mv - abs(ads_mv1);
      vSh_u2bl1 = abs(ads_mv1);
      res_u2bl1= res;
      }
    else if(dac_ch == u2bl2) { //dac_pwrdwn(u2bl1);
      iU2BL2 = abs(current) * 1000;
      vRRAM_u2bl2 = read_mv - abs(ads_mv1);
      vSh_u2bl2 = abs(ads_mv1);
      res_u2bl2 = res;
    }
  }
  else if( adc_ch == 3 ){
  ads_c2 = ads2.readADC_Differential_0_1(); // ADC Call to Channel A0 Single Ended.
  ads_mv2 = ads_c2 * ads_diff_multiplier; // Voltage across the shunt resistor
  double current = ads_mv2 * (0.001); // Current passing via the shunt = V x 1/R[sh] (mA)
  res = (read_mv - abs(ads_mv2)) / abs(current); // R[ReRAM] (Ohm) = Read Voltage (mV) - Shunt Voltage (mV) / Current Passing via the shunt. (mA)
  if(dac_ch == u1wl1) {  //// for set = u1wl1, read = u1bl2, reset = u2bl1
    //dac_pwrdwn(u1bl2);
    iU3BL1 = abs(current) * 1000;
    vRRAM_u3bl1 = read_mv - abs(ads_mv2);
    vSh_u3bl1 = abs(ads_mv2);
    res_u3bl1 = res;}
  else if(dac_ch == u2bl1) {
    //dac_pwrdwn(u1bl1);
    iU3BL2 = abs(current) * 1000; 
    vRRAM_u3bl2 = read_mv - abs(ads_mv2);
    vSh_u3bl2 = abs(ads_mv2);
    res_u3bl2 = res;
    }
    //Serial.print("Vsh:\t"); Serial.print(ads_mv0); Serial.print("mV"); Serial.print("\tIsh:\t"); Serial.print(current); Serial.println("mA");
  }
  else if( adc_ch == 4 ){
    ads_c3 = ads2.readADC_Differential_2_3(); // ADC Call to Channel A0 Single Ended.
    ads_mv3 = ads_c3 * ads_diff_multiplier; // Voltage across the shunt resistor
    double current = ads_mv3 * (0.001); // Current passing via the shunt = V x 1/R[sh] (mA)
    res = (read_mv - abs(ads_mv3)) / abs(current); // R[ReRAM] (Ohm) = Read Voltage (mV) - Shunt Voltage (mV) / Current Passing via the shunt. (mA) 
    if(dac_ch == u1wl1) { // for set = u1wl1, read = u1bl2, reset = u2bl1
      iU4BL1 = abs(current) * 1000; 
      vRRAM_u4bl1 = read_mv - abs(ads_mv3);
      vSh_u4bl1 = abs(ads_mv3);
      res_u4bl1= res;}
    else if(dac_ch == u2bl1) { //dac_pwrdwn(u2bl1);
      iU4BL2 = abs(current) * 1000;
      vRRAM_u4bl2 = read_mv - abs(ads_mv3);
      vSh_u4bl2 = abs(ads_mv3);
      res_u4bl2 = res;
      }
    //Serial.print("Vsh:\t"); Serial.print(ads_mv1); Serial.print("mV"); Serial.print("\tIsh:\t"); Serial.print(current); Serial.println("mA");
  }
  return res;
}

void dac_pulse(uint8_t dac_ch, int mV, uint8_t period){
  dac_write(dac_ch, map(mV,0,5000,0,100));
  delayMicroseconds(period);
  dac_write(dac_ch, 0);
}

void dac_write(byte channel, float percent_out) {
  uint8_t *txPtr;            // Pointer for SPI transmission
  uint8_t spiTxBuf[3];       // Buffer to hold the data to be transmitted via SPI

  spiTxBuf[0] = channel;     // First byte is the channel number

  // Convert percentage output (0-100%) to a 16-bit value (0-65535)
  uint16_t dacN_data = (uint16_t)(percent_out * 0.01 * 65535);

  // Split the 16-bit value into two 8-bit bytes and store them in the buffer
  spiTxBuf[1] = (dacN_data >> 8) & 0xFF; // High byte
  spiTxBuf[2] = dacN_data & 0xFF;        // Low byte

  txPtr = &spiTxBuf[0]; // Set the pointer to the beginning of the buffer

  // Transfer the data via SPI
  DAC1transfer((uint8_t*)txPtr, (uint8_t)(NUM_BYTES));
}

void dac_pwrdwn(uint8_t dac_no){ //Unsigned int dac no. not dac_channel byte.
  initTx0[0] = 0x09;
  initTx0[1] = 0b00000000;//dac15 to 8 
  initTx0[2] = 0b00000000;//dac7 to 0 
  if(dac_no >= 0 && dac_no <= 7) initTx0[2] = initTx0[2] | (1 << dac_no );
  else if (dac_no >= 8 && dac_no <= 15) initTx0[1] = initTx0[1] | (1 << ( dac_no - 8 )); // For dac 8 -> dac 15, the dac no. is offset by 8 because of the earlier bits.
  ret = DAC2transfer((uint8_t*)initPtr, (uint8_t)NUM_BYTES);
}

void loggerOut(){
    Serial.print("Vsh1: "); Serial.print(vSh_u1bl1); Serial.print("mV");
    Serial.print(" Ish1: "); Serial.print(iU1BL1); Serial.print("uA");
    Serial.print(" Vram1: "); Serial.print(vRRAM_u1bl1); Serial.print("mV");
    Serial.print(" RES1: "); Serial.print(res_u1bl1); Serial.print(" Ohms");
    if(res_u1bl1 > 9999999) res_u1bl1 = 9999999;
    Serial.print(" Vsh2: "); Serial.print(vSh_u2bl1); Serial.print("mV");
    Serial.print(" Ish2: "); Serial.print(iU2BL1); Serial.print("uA");
    Serial.print(" Vram2: "); Serial.print(vRRAM_u2bl1); Serial.print("mV");
    if(res_u2bl1 > 9999999) res_u2bl1 = 9999999;
    Serial.print(" RES2: "); Serial.print(res_u2bl1); Serial.print(" Ohms");
    Serial.print(" Vsh3: "); Serial.print(vSh_u3bl1); Serial.print("mV");
    Serial.print(" Ish3: "); Serial.print(iU3BL1); Serial.print("uA");
    Serial.print(" Vram3: "); Serial.print(vRRAM_u3bl1); Serial.print("mV");
    if(res_u3bl1 > 9999999) res_u3bl1 = 9999999;
    Serial.print(" RES3: "); Serial.print(res_u3bl1); Serial.print(" Ohms");
    Serial.print(" Vsh4: "); Serial.print(vSh_u4bl1); Serial.print("mV");
    Serial.print(" Ish4: "); Serial.print(iU4BL1); Serial.print("uA");
    Serial.print(" Vram4: "); Serial.print(vRRAM_u4bl1); Serial.print("mV");
    if(res_u4bl1 > 9999999) res_u4bl1 = 9999999;
    Serial.print(" RES4: "); Serial.print(res_u4bl1); Serial.print(" Ohms");
    Serial.print(" Iter: "); Serial.print(srCycles); Serial.print(" Steps");
    Serial.print(" Pass: "); Serial.print(steps); Serial.print(" Cycles");
    Serial.print(" Mode: "); Serial.print(mode);
    Serial.print(" U1Vbl1: "); Serial.print(vu1bl1*50); Serial.print("mV");
    Serial.print(" U1Vbl2: "); Serial.print(vu1bl2*50); Serial.print("mV");
    Serial.print(" U2Vbl1: "); Serial.print(vu2bl1*50); Serial.print("mV");
    Serial.print(" U2Vbl2: "); Serial.print(vu2bl2*50); Serial.print("mV");
    Serial.print(" U1Vwl1: "); Serial.print(vu1wl1*50); Serial.print("mV");
    Serial.print(" U2Vwl1: "); Serial.print(vu2wl1*50); Serial.print("mV");
    Serial.print(" U1Vsl1: "); Serial.print(vu1sl1*50); Serial.print("mV");
    Serial.print(" U2Vsl1: "); Serial.print(vu2sl1*50); Serial.print("mV");
    //Serial.print(" stageRes: "); Serial.print(current_resistanceHRS); Serial.println(" Ohms");
    //Serial.print(" Addr: "); Serial.print(addr_hex); Serial.println("h");
    Serial.print(" LRS: "); Serial.print(current_resistanceLRS); Serial.print(" Ohms");
    Serial.print(" HRS: "); Serial.print(current_resistanceHRS); Serial.print(" Ohms");
    Serial.print(" OpState: "); Serial.println(operation_state);
}

 
 // Non-blocking version - call this in loop()
void dac_powerup_continuous_nonblocking() {
    static unsigned long last_update = 0;
    
    if (millis() - last_update >= 10) {
        last_update = millis();
        dac_write(dac[9],  map(0.5, 0, 5, 0, 100));
        dac_write(dac[10], map(0.9, 0, 5, 0, 100));
        dac_write(dac[11], map(0.2, 0, 5, 0, 100));
        dac_write(dac[12], map(1.6, 0, 5, 0, 100));
        dac_write(dac[13], map(1.0, 0, 5, 0, 100));
        dac_write(dac[14], map(5.0, 0, 5, 0, 100));
        dac_write(dac[15], map(2.1, 0, 5, 0, 100));
    }
}


 // Continuous power-up function - applies only specified voltages
void dac_powerup_continuous() {
  //while (1) {
    // Apply only the specified voltages
    dac_write(dac[7],  map(0.6, 0, 5, 0, 100));   // DAC[7]  = 1.3V for VDDIO
    dac_write(dac[6],  map(3.3, 0, 5, 0, 100));   // DAC[6]  = 5.0V for VDDIO
    dac_write(dac[9],  map(0.5, 0, 5, 0, 100));   // DAC[9]  = 0.5V
    dac_write(dac[10], map(0.9, 0, 5, 0, 100));   // DAC[10] = 0.9V
    dac_write(dac[11], map(0.6, 0, 5, 0, 100));   // DAC[11] = 0.6V
    dac_write(dac[12], map(1.6, 0, 5, 0, 100));   // DAC[12] = 1.6V
    dac_write(dac[13], map(1.0, 0, 5, 0, 100));   // DAC[13] = 1.0V
    dac_write(dac[14], map(5.0, 0, 5, 0, 100));   // DAC[14] = 5.0V
    dac_write(dac[15], map(2.1, 0, 5, 0, 100));   // DAC[15] = 2.1V
    
    // All other DAC channels stay at 0V (they're already 0 from initialization)
    
    // Small delay to maintain continuous output
    delay(10);  // Adjust as needed
  //}
}

void dac_stndby(){
  dac_write(dac[0], 0); dac_write(dac[1], 0);  dac_write(dac[2], 0);  dac_write(dac[3], 0);
  dac_write(dac[4], 0); dac_write(dac[5], 0);  //dac_write(dac[6], 0);  dac_write(dac[7], 0);
  //dac_write(dac[8], 0); dac_write(dac[9], 0);  dac_write(dac[10], 0);  dac_write(dac[11], 0);
  //dac_write(dac[12], 0); dac_write(dac[13], 0);  dac_write(dac[14], 0);  dac_write(dac[15], 0);
}

void indicator_test(){
  led1.setPixel(0, RED);
  led2.setPixel(0, BLUE);
  led1.show();
  led2.show();
  delay(150);
  led1.setPixel(0, BLUE);
  led2.setPixel(0, RED);
  led1.show();
  led2.show();
  delay(150);
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(setLed, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(resetLed, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(readLed, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(activeLed, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(faultLed, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(1000);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  delay(100);
  digitalWrite(setLed, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(resetLed, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(readLed, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(activeLed, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(100);
  digitalWrite(faultLed, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(1000);                      // wait for a second
  led1.setPixel(0, 0x00000000);
}
