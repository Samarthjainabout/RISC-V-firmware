# Frequency skew experiment - FORM + RSRR ADC/DAC report

Date: 2026-08-08

## Scope

This report compares the FORM + RSRR synchronized current-monitoring experiment across Caravel clock settings:

- `50 MHz`
- `10 MHz`
- `2 MHz`
- `1 MHz`

Sequence:

`FORM -> READ1 -> SET -> READ2 -> RESET -> READ3`

ADC:

- ADS1258
- Scaling reference assumption: internal reference with `AVDD - AVSS = 5 V`
- Shunt value: `1 kOhm`
- Because `Rshunt = 1 kOhm`, `1 uA` current delta corresponds to `1 mV` shunt-voltage delta.

## Source artifacts

| Frequency | Artifact | Status |
|---:|---|---|
| 50 MHz | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-50mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-145459` | Completed |
| 10 MHz | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-vddio-vdda1-5v-wl0-allwindows05-20260807-130631` | Completed; pre fixed-count handshake, but sample count is comparable |
| 2 MHz | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-2mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-141950` | Completed |
| 1 MHz | `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-1mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-143624` | Partial; stalls after READ1 monitor samples |

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
| Caravel | `write_config_packets()` | Sends config packets before every operation |
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
Caravel:  config packets
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
| 50 MHz | FORM, READ1, SET, READ2, RESET, READ3 | avg 43.428 ms, range 43.356-43.528 ms | ~2.17 M cycles | Completed |
| 10 MHz | FORM, READ1, SET, READ2, RESET, READ3 | avg 43.007 ms, range 42.193-43.436 ms | ~430 k cycles | Completed |
| 2 MHz | FORM, READ1, SET, READ2, RESET, READ3 | avg 43.399 ms, range 43.356-43.528 ms | ~86.8 k cycles | Completed |
| 1 MHz | FORM + READ1 partial | avg 43.485 ms, range 43.442-43.528 ms | ~43.5 k cycles to ADC done | Stuck high after READ1 |

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

Config writes before each operation:

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

## 50 MHz results

Completed. Fixed-count ADC sampling held: every packet reports `39` samples in the host summary.

| Packet | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| FORM | 39 | 60.7834 uA / 60.7834 mV | 14.6965 uA / 14.6965 mV | 7.6759 uA / 7.6759 mV |
| READ1 | 39 | 509.4640 uA / 509.4640 mV | 3.0833 uA / 3.0833 mV | 9.3855 uA / 9.3855 mV |
| SET | 39 | 15.8699 uA / 15.8699 mV | 13.5231 uA / 13.5231 mV | 19.0259 uA / 19.0259 mV |
| READ2 | 39 | 524.7645 uA / 524.7645 mV | 5.1669 uA / 5.1669 mV | 11.2540 uA / 11.2540 mV |
| RESET | 39 | 9.0529 uA / 9.0529 mV | 2.5504 uA / 2.5504 mV | 437.5357 uA / 437.5357 mV |
| READ3 | 39 | 513.2290 uA / 513.2290 mV | 3.4193 uA / 3.4193 mV | 13.2252 uA / 13.2252 mV |

Monitor-window markers:

| Packet | Teensy monitor samples | Teensy monitor duration |
|---|---:|---:|
| FORM | 38 | 43.442 ms |
| READ1 | 38 | 43.356 ms |
| SET | 38 | 43.357 ms |
| READ2 | 38 | 43.528 ms |
| RESET | 38 | 43.528 ms |
| READ3 | 38 | 43.356 ms |

## 10 MHz results

Completed. This run predates the fixed-count handshake, but the summary still reports comparable sample counts: `38-39` samples per packet. It is used as the `10 MHz` reference because this was the clock before the later 2 MHz/1 MHz/50 MHz changes.

| Packet | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| FORM | 38 | 21.9503 uA / 21.9503 mV | 10.9313 uA / 10.9313 mV | 13.1589 uA / 13.1589 mV |
| READ1 | 39 | 448.5796 uA / 448.5796 mV | 4.0978 uA / 4.0978 mV | 22.4816 uA / 22.4816 mV |
| SET | 39 | 28.2410 uA / 28.2410 mV | 10.4332 uA / 10.4332 mV | 14.9116 uA / 14.9116 mV |
| READ2 | 39 | 490.6697 uA / 490.6697 mV | 2.5057 uA / 2.5057 mV | 8.8394 uA / 8.8394 mV |
| RESET | 39 | 21.4804 uA / 21.4804 mV | 3.6178 uA / 3.6178 mV | 7.6494 uA / 7.6494 mV |
| READ3 | 38 | 481.1500 uA / 481.1500 mV | 4.2650 uA / 4.2650 mV | 11.9525 uA / 11.9525 mV |

## 2 MHz results

Completed. Fixed-count ADC sampling held: every packet reports `39` samples in the host summary.

| Packet | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| FORM | 39 | 39.0847 uA / 39.0847 mV | 9.7580 uA / 9.7580 mV | 12.0683 uA / 12.0683 mV |
| READ1 | 39 | 478.7288 uA / 478.7288 mV | 5.1256 uA / 5.1256 mV | 15.4859 uA / 15.4859 mV |
| SET | 39 | 38.4359 uA / 38.4359 mV | 12.4655 uA / 12.4655 mV | 20.8415 uA / 20.8415 mV |
| READ2 | 39 | 499.9724 uA / 499.9724 mV | 2.3865 uA / 2.3865 mV | 13.8359 uA / 13.8359 mV |
| RESET | 39 | 18.3292 uA / 18.3292 mV | 5.0064 uA / 5.0064 mV | 708.3693 uA / 708.3693 mV |
| READ3 | 39 | 73.9308 uA / 73.9308 mV | 3.6907 uA / 3.6907 mV | 19.3123 uA / 19.3123 mV |

Monitor-window markers:

| Packet | Teensy monitor samples | Teensy monitor duration |
|---|---:|---:|
| FORM | 38 | 43.356 ms |
| READ1 | 38 | 43.442 ms |
| SET | 38 | 43.356 ms |
| READ2 | 38 | 43.356 ms |
| RESET | 38 | 43.528 ms |
| READ3 | 38 | 43.356 ms |

## 1 MHz results

Partial run. FORM completed. READ1 reached the fixed `38` monitor samples, but the sequence timed out before Caravel closed `CAPTURE_ACTIVE`, so there is no final READ1 sample and no SET/READ2/RESET/READ3 data for the clean 1 MHz fixed-sampling attempt.

Observed log point:

`SYNC_MEASURE_DONE packet=READ1 samples=38 t_us=43528`

This means ADC sampling itself completed for READ1; the failure is in the post-sampling handshake / Caravel progress after READ1.

| Packet | Samples | Read peak | Set peak | Reset peak |
|---|---:|---:|---:|---:|
| FORM | 39 | 77.2707 uA / 77.2707 mV | 11.4576 uA / 11.4576 mV | 9.8787 uA / 9.8787 mV |
| READ1 | 38 | 34.0667 uA / 34.0667 mV | 4.0084 uA / 4.0084 mV | 11.3484 uA / 11.3484 mV |
| SET | not reached | - | - | - |
| READ2 | not reached | - | - | - |
| RESET | not reached | - | - | - |
| READ3 | not reached | - | - | - |

## Cross-frequency comparison

READ-window peak current:

| Frequency | READ1 | READ2 | READ3 | Observation |
|---:|---:|---:|---:|---|
| 50 MHz | 509.4640 uA | 524.7645 uA | 513.2290 uA | Stable READ current across all READ windows |
| 10 MHz | 448.5796 uA | 490.6697 uA | 481.1500 uA | Stable READ current; slightly lower than 50 MHz |
| 2 MHz | 478.7288 uA | 499.9724 uA | 73.9308 uA | READ3 is anomalously low |
| 1 MHz | 34.0667 uA partial READ1 | not reached | not reached | READ1 monitor samples completed, but no final window close |

SET / FORM shunt peak current:

| Frequency | FORM set-shunt peak | SET set-shunt peak | Observation |
|---:|---:|---:|---|
| 50 MHz | 14.6965 uA | 13.5231 uA | FORM and SET set-shunt levels match closely |
| 10 MHz | 10.9313 uA | 10.4332 uA | FORM and SET set-shunt levels match closely |
| 2 MHz | 9.7580 uA | 12.4655 uA | FORM and SET close |
| 1 MHz | 11.4576 uA | not reached | FORM completed; SET not reached |

RESET-shunt peak current:

| Frequency | RESET packet reset-shunt peak | Observation |
|---:|---:|---|
| 50 MHz | 437.5357 uA | RESET current dominant |
| 10 MHz | 7.6494 uA | RESET current weak in this reference run |
| 2 MHz | 708.3693 uA | RESET current dominant and strongest measured completed run |
| 1 MHz | not reached | Sequence stalled before RESET |

## Interpretation

- Fixed-count ADC sampling works for `50 MHz` and `2 MHz`: both completed with `39` host summary samples for every packet.
- The `10 MHz` reference was taken before the fixed-count handshake, but its sample count is already close enough for first-order comparison (`38-39` samples).
- The `1 MHz` run does not show an ADC sampling limitation. READ1 reached `SYNC_MEASURE_DONE` with `38` monitor samples. The failure is that Caravel did not close `CAPTURE_ACTIVE` afterward, so the likely issue is Caravel/WB progress after the READ1 packet at `1 MHz`.
- READ behavior is strongest and most stable at `50 MHz`; `2 MHz` has stable READ1/READ2 but anomalously low READ3; `10 MHz` has stable READ windows but a weak RESET event; `1 MHz` does not complete the sequence.
