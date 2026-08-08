# Frequency skew experiment - FORM + RSRR ADC/DAC report

Date: 2026-08-08

## Scope

This report originally compared the FORM + RSRR synchronized current-monitoring experiment across Caravel clock settings:

- `50 MHz`
- `10 MHz`
- `2 MHz`
- `1 MHz`

Intended sequence:

`FORM -> READ1 -> SET -> READ2 -> RESET -> READ3`

## Erratum - WB config resend issue

The original frequency-skew experiment was run with the wrong Caravel WB packet sequence.

The built Caravel source used for the 2026-08-08 frequency runs at:

`/home/ubuntu-24-04/caravel_board/firmware/chipignite/reram_prog/synced_mode_wb/synced_mode_wb.c`

showed `write_config_packets()` before every operation. The Teensy labels still showed `FORM`, `READ1`, `SET`, `READ2`, `RESET`, `READ3`, but the WB stream sent to the user design was not only those six operation packets:

```text
write_config_packets();
FORM
write_config_packets();
READ1
write_config_packets();
SET
write_config_packets();
READ2
write_config_packets();
RESET
write_config_packets();
READ3
```

For `Neuromorphic_X2_wb_beh.v`, only the first three writes after reset are config. Re-sending the same words after that decodes them as normal operations:

| Word | Intended use | Behavioral-model decode after config phase |
|---:|---|---|
| `0x00036472` | config word | RESET row 0, col 0, value `0x72` |
| `0x462B000B` | config word | READ row 3, col 2, value `0x0B` |
| `0x44001405` | config word | READ row 2, col 0, value `0x05` |

Remote cocotb check:

```sh
cd /home/ubuntu-24-04/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb
PATH=/home/ubuntu-24-04/caravel_user_Neuromorphic_X1_32x32/venv-cocotb/bin:$PATH \
TESTCASE=tb_repeated_config_full_form_rsrr_injects_extra_read_responses make
```

Result:

- `TESTS=1`
- `PASS=1`
- The test passes by proving the repeated-config sequence injects extra responses: the model produces 13 READ responses instead of the 3 intended READ responses.

Therefore, the data below is valid as a hardware trace of the sequence that was actually sent, but it is not a clean frequency-skew comparison for the intended packet sequence.

What was actually happening before:

```text
initial config:      CONFIG, CONFIG, CONFIG
FORM window:         FORM
before READ1:        RESET(row0,col0), READ(row3,col2), READ(row2,col0)
READ1 window:        READ1
before SET:          RESET(row0,col0), READ(row3,col2), READ(row2,col0)
SET window:          SET
before READ2:        RESET(row0,col0), READ(row3,col2), READ(row2,col0)
READ2 window:        READ2
before RESET:        RESET(row0,col0), READ(row3,col2), READ(row2,col0)
RESET window:        RESET
before READ3:        RESET(row0,col0), READ(row3,col2), READ(row2,col0)
READ3 window:        READ3
```

So the earlier labels `READ1`, `READ2`, and `READ3` are only the ADC/DAC capture-window labels. They were not isolated WB operations because repeated config words were disguising themselves as extra RESET/READ/READ packets between the labeled windows.

Cocotb-corrected labels used for the historical repeated-config tables below:

| Old table label | Actual WB activity from behavioral-model decode |
|---|---|
| `~~FORM~~` | initial `CONFIG, CONFIG, CONFIG`, then `COMPUTE/FORM(row8,col0,0xFF)` |
| `~~READ1~~` | injected `RESET(row0,col0,0x72) -> READ(row3,col2,0x0B) -> READ(row2,col0,0x05)`, then `READ(row8,col0,0xFF)` |
| `~~SET~~` | injected `RESET(row0,col0,0x72) -> READ(row3,col2,0x0B) -> READ(row2,col0,0x05)`, then `SET(row8,col0,0xFF)` |
| `~~READ2~~` | injected `RESET(row0,col0,0x72) -> READ(row3,col2,0x0B) -> READ(row2,col0,0x05)`, then `READ(row8,col0,0xFF)` |
| `~~RESET~~` | injected `RESET(row0,col0,0x72) -> READ(row3,col2,0x0B) -> READ(row2,col0,0x05)`, then `RESET(row8,col0,0xFF)` |
| `~~READ3~~` | injected `RESET(row0,col0,0x72) -> READ(row3,col2,0x0B) -> READ(row2,col0,0x05)`, then `READ(row8,col0,0xFF)` |

Clean 2 MHz rerun with the config-once firmware:

- `TEST_Bench/form_rsrr_2mhz_config_once_adc_dac_report_2026-08-08.md`
- artifact: `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-2mhz-configonce-vddio-vdda1-5v-wl0-allwindows05-20260808-161607`

ADC:

- ADS1258
- Scaling reference assumption: internal reference with `AVDD - AVSS = 5 V`
- Shunt value: `1 kOhm`
- Because `Rshunt = 1 kOhm`, `1 uA` current delta corresponds to `1 mV` shunt-voltage delta.

## Source artifacts

| Frequency | Artifact | Status |
|---:|---|---|
| 50 MHz historical repeated-config | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-50mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-145459` | Completed; wrong WB stream, needs clean rerun |
| 10 MHz historical repeated-config | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-vddio-vdda1-5v-wl0-allwindows05-20260807-130631` | Completed; wrong WB stream and pre fixed-count handshake |
| 2 MHz clean config-once | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-2mhz-configonce-vddio-vdda1-5v-wl0-allwindows05-20260808-161607` | Completed; corrected sequence |
| 2 MHz historical repeated-config | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-2mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-141950` | Completed; wrong WB stream, kept only for comparison |
| 1 MHz historical repeated-config | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-1mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-143624` | Partial; wrong WB stream, stalls after READ1 monitor samples |

## Sync GPIO mapping

| Signal | Direction | Teensy pin | Caravel GPIO | Meaning |
|---|---|---:|---:|---|
| `ADC_READY` | Teensy -> Caravel | 5 | GPIO18 | Teensy has baseline / later drops when fixed sample count is complete |
| `MODE0` | Caravel -> Teensy | 7 | GPIO19 | Operation mode bit 0 |
| `MODE1` | Caravel -> Teensy | 21 | GPIO20 | Operation mode bit 1 |
| `CAPTURE_ACTIVE` | Caravel -> Teensy | 18 | GPIO23 | Caravel opens/closes packet measurement window |

Mode encoding:

| MODE1 | MODE0 | Mode |
|---:|---:|---|
| 0 | 1 | READ |
| 1 | 0 | SET / FORM |
| 1 | 1 | RESET |
| 0 | 0 | IDLE |

## Sync code diagram

Code locations:

| Side | Function | Role |
|---|---|---|
| Caravel | `write_config_packets()` | Corrected firmware sends the required config phase once after reset |
| Caravel | `run_synced_packet()` | Opens sync window, sends WB packet, closes window |
| Teensy | `syncServiceOnePacket()` | Applies DAC, samples ADC, drops `ADC_READY` after fixed sample count |
| Teensy | `syncDacForPacket()` | Maps packet name to DAC channel |

Per-transaction timing:

```text
time  -------------------------------------------------------------------->

Caravel MODE[1:0]      IDLE |==== FORM/READ/SET/RESET mode ====| IDLE

CAPTURE_ACTIVE         _____/'''''''''''''''''''''''''''''''''''\_____

Packet DAC             0 V__/'''''''''''''''''''''''''''\________0 V__
                            ^ apply DAC                 ^ zero DAC

ADC samples                 B  S  S  S  S ... S  S      F
                            |  <--- 38 monitor samples -->|
                            |                              |
                            baseline                      final

ADC_READY              ________/'''''''''''''''''''\___________________
                               ^                  ^
                               |                  |
                               ADC ready          Teensy sample count done

WB packet write                       REG32(NEURO_ADDR) = packet
WB ACK                                             returns here
```

Minimal flow:

```text
Caravel:  config packets once after reset
Caravel:  MODE = operation, CAPTURE_ACTIVE = 1
Teensy:   apply packet DAC, wait 2 ms
Teensy:   baseline ADC sample, ADC_READY = 1
Caravel:  REG32(NEURO_ADDR) = packet
Teensy:   collect 38 ADC monitor samples
Teensy:   ADC_READY = 0
Caravel:  CAPTURE_ACTIVE = 0, MODE = IDLE
Teensy:   DAC = 0 V, final ADC sample
Host:     summary samples = 38 monitor + 1 final = 39
```

Packet map:

| Transaction | Mode | DAC during window | WB packet |
|---|---|---|---:|
| FORM | SET | DAC[0] = 0.5 V | `0x900888FF` |
| READ1 | READ | DAC[2] = 0.5 V | `0x500888FF` |
| SET | SET | DAC[0] = 0.5 V | `0xD00888FF` |
| READ2 | READ | DAC[2] = 0.5 V | `0x500888FF` |
| RESET | RESET | DAC[5] = 0.5 V | `0x100888FF` |
| READ3 | READ | DAC[2] = 0.5 V | `0x500888FF` |

1 MHz note: READ1 reached `SYNC_MEASURE_DONE`, so ADC sampling completed; Caravel did not drop `CAPTURE_ACTIVE` afterward.

## CAPTURE_ACTIVE clock scale

Approximation method:

```text
CAPTURE_ACTIVE high ~= Teensy ADC monitor window
clock cycles ~= frequency * measured_window_seconds
```

Historical captures did not log the exact `CAPTURE_ACTIVE` falling-edge timestamp. The table uses Teensy `SYNC_MEASURE_DONE t_us` where present; for the 10 MHz reference it uses the last ADC sample timestamp in each packet.

| Frequency | Packets used | Measured ADC window | Approx Caravel cycles during window | CAPTURE_ACTIVE result |
|---:|---|---:|---:|---|
| 50 MHz historical repeated-config | FORM, READ1, SET, READ2, RESET, READ3 labels | avg 43.428 ms, range 43.356-43.528 ms | ~2.17 M cycles | Completed, wrong WB stream |
| 10 MHz historical repeated-config | FORM, READ1, SET, READ2, RESET, READ3 labels | avg 43.007 ms, range 42.193-43.436 ms | ~430 k cycles | Completed, wrong WB stream |
| 2 MHz clean config-once | FORM, READ1, SET, READ2, RESET, READ3 | avg 43.385 ms, range 43.356-43.442 ms | ~86.8 k cycles | Completed |
| 1 MHz historical repeated-config | FORM + READ1 partial labels | avg 43.485 ms, range 43.442-43.528 ms | ~43.5 k cycles to ADC done | Stuck high after READ1, wrong WB stream |

For the 1 MHz run, the intended ADC monitor window still completed at about `43.5 ms`, but `CAPTURE_ACTIVE` stayed high afterward because Caravel did not close the transaction.

## DAC / rail voltages

Static rails used for all four frequency runs:

| DAC | Signal / name | Voltage | Notes |
|---:|---|---:|---|
| DAC[1] | `Vcc_wl_read` | 0.0 V | Static WL read rail |
| DAC[3] | `Vcc_wl_set` | 0.0 V | Static WL set rail |
| DAC[4] | `Vcc_wl_reset` | 0.0 V | Static WL reset rail |
| DAC[7] | Caravel `VDDIO` | 5.0 V | I/O rail |
| DAC[14] | `VDDa1` | 5.0 V | Analog rail |
| DAC[6] | constant rail | 3.3 V | Existing setup rail |
| DAC[9] | constant rail | 0.5 V | Existing setup rail |
| DAC[10] | constant rail | 0.9 V | Existing setup rail |
| DAC[11] | constant rail | 0.6 V | Existing setup rail |
| DAC[12] | constant rail | 1.6 V | Existing setup rail |
| DAC[13] | constant rail | 1.0 V | Existing setup rail |
| DAC[15] | constant rail | 2.1 V | Existing setup rail |

Packet-window DACs:

| Packet / window | DAC | Applied during window | Idle after window | Shunt path |
|---|---:|---:|---:|---|
| FORM | DAC[0] | 0.5 V | 0.0 V | SET shunt |
| SET | DAC[0] | 0.5 V | 0.0 V | SET shunt |
| READ | DAC[2] | 0.5 V | 0.0 V | READ shunt |
| RESET | DAC[5] | 0.5 V | 0.0 V | RESET shunt |

## Wishbone packets

Corrected config phase, sent once after reset:

| Order | WB packet |
|---:|---:|
| 1 | `0x00036472` |
| 2 | `0x462B000B` |
| 3 | `0x44001405` |

Operation packets:

| Operation | WB packet | Sync mode | Shunt/DAC path |
|---|---:|---|---|
| FORM | `0x900888FF` | SET | SET shunt, DAC[0] |
| READ1 | `0x500888FF` | READ | READ shunt, DAC[2] |
| SET | `0xD00888FF` | SET | SET shunt, DAC[0] |
| READ2 | `0x500888FF` | READ | READ shunt, DAC[2] |
| RESET | `0x100888FF` | RESET | RESET shunt, DAC[5] |
| READ3 | `0x500888FF` | READ | READ shunt, DAC[2] |

## 50 MHz historical repeated-config results

Completed. Fixed-count ADC sampling held: every packet reports `39` samples in the host summary. This run used the wrong repeated-config WB stream.

| Packet label / actual WB activity | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| ~~FORM~~ config x3 -> FORM/COMPUTE | 39 | 60.7834 uA / 60.7834 mV | 14.6965 uA / 14.6965 mV | 7.6759 uA / 7.6759 mV |
| ~~READ1~~ injected RESET/READ/READ -> READ | 39 | 509.4640 uA / 509.4640 mV | 3.0833 uA / 3.0833 mV | 9.3855 uA / 9.3855 mV |
| ~~SET~~ injected RESET/READ/READ -> SET | 39 | 15.8699 uA / 15.8699 mV | 13.5231 uA / 13.5231 mV | 19.0259 uA / 19.0259 mV |
| ~~READ2~~ injected RESET/READ/READ -> READ | 39 | 524.7645 uA / 524.7645 mV | 5.1669 uA / 5.1669 mV | 11.2540 uA / 11.2540 mV |
| ~~RESET~~ injected RESET/READ/READ -> RESET | 39 | 9.0529 uA / 9.0529 mV | 2.5504 uA / 2.5504 mV | 437.5357 uA / 437.5357 mV |
| ~~READ3~~ injected RESET/READ/READ -> READ | 39 | 513.2290 uA / 513.2290 mV | 3.4193 uA / 3.4193 mV | 13.2252 uA / 13.2252 mV |

Monitor-window markers:

| Packet | Teensy monitor samples | Teensy monitor duration |
|---|---:|---:|
| FORM | 38 | 43.442 ms |
| READ1 | 38 | 43.356 ms |
| SET | 38 | 43.357 ms |
| READ2 | 38 | 43.528 ms |
| RESET | 38 | 43.528 ms |
| READ3 | 38 | 43.356 ms |

## 10 MHz historical repeated-config results

Completed. This run predates the fixed-count handshake and used the wrong repeated-config WB stream. The summary still reports comparable sample counts: `38-39` samples per packet.

| Packet label / actual WB activity | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| ~~FORM~~ config x3 -> FORM/COMPUTE | 38 | 21.9503 uA / 21.9503 mV | 10.9313 uA / 10.9313 mV | 13.1589 uA / 13.1589 mV |
| ~~READ1~~ injected RESET/READ/READ -> READ | 39 | 448.5796 uA / 448.5796 mV | 4.0978 uA / 4.0978 mV | 22.4816 uA / 22.4816 mV |
| ~~SET~~ injected RESET/READ/READ -> SET | 39 | 28.2410 uA / 28.2410 mV | 10.4332 uA / 10.4332 mV | 14.9116 uA / 14.9116 mV |
| ~~READ2~~ injected RESET/READ/READ -> READ | 39 | 490.6697 uA / 490.6697 mV | 2.5057 uA / 2.5057 mV | 8.8394 uA / 8.8394 mV |
| ~~RESET~~ injected RESET/READ/READ -> RESET | 39 | 21.4804 uA / 21.4804 mV | 3.6178 uA / 3.6178 mV | 7.6494 uA / 7.6494 mV |
| ~~READ3~~ injected RESET/READ/READ -> READ | 38 | 481.1500 uA / 481.1500 mV | 4.2650 uA / 4.2650 mV | 11.9525 uA / 11.9525 mV |

## 2 MHz clean config-once results

Completed. Fixed-count ADC sampling held: every packet reports `39` samples in the host summary.

| Packet | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| FORM | 39 | 24.9227 uA / 24.9227 mV | 9.7331 uA / 9.7331 mV | 13.0994 uA / 13.0994 mV |
| READ1 | 39 | 467.0924 uA / 467.0924 mV | 4.6373 uA / 4.6373 mV | 21.3297 uA / 21.3297 mV |
| SET | 39 | 14.6385 uA / 14.6385 mV | 7.8944 uA / 7.8944 mV | 13.7613 uA / 13.7613 mV |
| READ2 | 39 | 486.7937 uA / 486.7937 mV | 4.5049 uA / 4.5049 mV | 13.3691 uA / 13.3691 mV |
| RESET | 39 | 9.1340 uA / 9.1340 mV | 3.0734 uA / 3.0734 mV | 585.9699 uA / 585.9699 mV |
| READ3 | 39 | 477.2112 uA / 477.2112 mV | 6.3387 uA / 6.3387 mV | 14.6402 uA / 14.6402 mV |

Monitor-window markers:

| Packet | Teensy monitor samples | Teensy monitor duration |
|---|---:|---:|
| FORM | 38 | 43.357 ms |
| READ1 | 38 | 43.356 ms |
| SET | 38 | 43.356 ms |
| READ2 | 38 | 43.442 ms |
| RESET | 38 | 43.442 ms |
| READ3 | 38 | 43.356 ms |

## 2 MHz historical repeated-config results

These are the original 2 MHz values from the wrong repeated-config WB stream. They are retained to explain the earlier anomaly, but they should not be used as the clean 2 MHz FORM+RSRR result.

| Packet label / actual WB activity | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| ~~FORM~~ config x3 -> FORM/COMPUTE | 39 | 39.0847 uA / 39.0847 mV | 9.7580 uA / 9.7580 mV | 12.0683 uA / 12.0683 mV |
| ~~READ1~~ injected RESET/READ/READ -> READ | 39 | 478.7288 uA / 478.7288 mV | 5.1256 uA / 5.1256 mV | 15.4859 uA / 15.4859 mV |
| ~~SET~~ injected RESET/READ/READ -> SET | 39 | 38.4359 uA / 38.4359 mV | 12.4655 uA / 12.4655 mV | 20.8415 uA / 20.8415 mV |
| ~~READ2~~ injected RESET/READ/READ -> READ | 39 | 499.9724 uA / 499.9724 mV | 2.3865 uA / 2.3865 mV | 13.8359 uA / 13.8359 mV |
| ~~RESET~~ injected RESET/READ/READ -> RESET | 39 | 18.3292 uA / 18.3292 mV | 5.0064 uA / 5.0064 mV | 708.3693 uA / 708.3693 mV |
| ~~READ3~~ injected RESET/READ/READ -> READ | 39 | 73.9308 uA / 73.9308 mV | 3.6907 uA / 3.6907 mV | 19.3123 uA / 19.3123 mV |

The old `READ3` label was not an isolated third READ operation. Before that labeled window, the repeated config words had already injected another `RESET(row0,col0)`, `READ(row3,col2)`, and `READ(row2,col0)`. The low `READ3` value is therefore consistent with the run being contaminated by extra WB activity, not with a clean 2 MHz frequency-skew effect.

## 1 MHz historical repeated-config partial results

Partial run with the wrong repeated-config WB stream. FORM completed. READ1 reached the fixed `38` monitor samples, but the sequence timed out before Caravel closed `CAPTURE_ACTIVE`, so there is no final READ1 sample and no SET/READ2/RESET/READ3 data.

Observed log point:

`SYNC_MEASURE_DONE packet=READ1 samples=38 t_us=43528`

This means ADC sampling itself completed for READ1; the failure is in the post-sampling handshake / Caravel progress after READ1.

| Packet label / actual WB activity | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| ~~FORM~~ config x3 -> FORM/COMPUTE | 39 | 77.2707 uA / 77.2707 mV | 11.4576 uA / 11.4576 mV | 9.8787 uA / 9.8787 mV |
| ~~READ1~~ injected RESET/READ/READ -> READ | 38 | 34.0667 uA / 34.0667 mV | 4.0084 uA / 4.0084 mV | 11.3484 uA / 11.3484 mV |
| SET | not reached | - | - | - |
| READ2 | not reached | - | - | - |
| RESET | not reached | - | - | - |
| READ3 | not reached | - | - | - |

## Cross-frequency comparison

READ-window peak current:

| Frequency | READ1 | READ2 | READ3 | Observation |
|---:|---:|---:|---:|---|
| 50 MHz historical | 509.4640 uA | 524.7645 uA | 513.2290 uA | Repeated-config run; stable labels, but not clean WB sequence |
| 10 MHz historical | 448.5796 uA | 490.6697 uA | 481.1500 uA | Repeated-config run; stable labels, but not clean WB sequence |
| 2 MHz clean config-once | 467.0924 uA | 486.7937 uA | 477.2112 uA | Clean run; stable READ across all three READ windows |
| 1 MHz historical partial | 34.0667 uA partial READ1 | not reached | not reached | Repeated-config run; no clean conclusion |

SET / FORM shunt peak current:

| Frequency | FORM set-shunt peak | SET set-shunt peak | Observation |
|---:|---:|---:|---|
| 50 MHz historical | 14.6965 uA | 13.5231 uA | Repeated-config run |
| 10 MHz historical | 10.9313 uA | 10.4332 uA | Repeated-config run |
| 2 MHz clean config-once | 9.7331 uA | 7.8944 uA | Clean run; FORM and SET are both small at 0.5 V |
| 1 MHz historical partial | 11.4576 uA | not reached | Repeated-config run; SET not reached |

RESET-shunt peak current:

| Frequency | RESET packet reset-shunt peak | Observation |
|---:|---:|---|
| 50 MHz historical | 437.5357 uA | Repeated-config run; RESET label was dominant |
| 10 MHz historical | 7.6494 uA | Repeated-config run; RESET label was weak |
| 2 MHz clean config-once | 585.9699 uA | Clean run; RESET current is dominant |
| 1 MHz historical partial | not reached | Repeated-config run stalled before RESET |

## Interpretation

- The clean 2 MHz config-once run completed all six packets with `39` host summary samples for every packet.
- The corrected 2 MHz data shows stable READ current across READ1/READ2/READ3 and dominant RESET-shunt current during RESET.
- The earlier 2 MHz repeated-config run had an anomalously low `READ3` label because extra WB commands were injected before that window. That value should not be interpreted as a frequency effect.
- The 50 MHz, 10 MHz, and 1 MHz entries remain useful only as historical hardware traces of the wrong repeated-config stream. They need clean config-once reruns before making a true frequency-skew conclusion.
