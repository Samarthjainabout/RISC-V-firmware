# Synced ADS1258 FORM + RSRR run - 2 MHz Caravel, fixed ADC sample count

Date: 2026-08-08

## Reason for this run

The first 2 MHz run reduced the ADC summary samples from about `38-39` samples per packet to `11-12` samples per packet. Since the previous frequency was `10 MHz`, this was not a valid simple clock-scaling effect. The issue was protocol ownership: Caravel was ending `CAPTURE_ACTIVE` based on firmware/WB timing, so Teensy only sampled for however long Caravel kept the window open.

For comparable ADC sampling, the handshake was changed so Teensy owns the measurement length:

- Teensy samples a fixed `38` monitor samples per packet window.
- The host summary also includes the final sample, so each packet reports `39` samples.
- Teensy drops `ADC_READY` after the fixed monitor sample count.
- Caravel keeps `CAPTURE_ACTIVE` high until it sees `ADC_READY` drop, then closes the window.

Packet order and voltage settings were unchanged.

## Run setup

- Sequence: `FORM -> READ1 -> SET -> READ2 -> RESET -> READ3`
- Caravel frequency: `2 MHz`
- Host control: `tools/sync_control/sync_control.py`
- Teensy serial: `/dev/serial/by-id/usb-Teensyduino_USB_Serial_8829000-if00`
- Caravel UART/J2: disconnected for this run; used `--no-caravel`
- Artifact directory: `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-2mhz-fixed-adc-vddio-vdda1-5v-wl0-allwindows05-20260808-141950`
- ADS1258 reference assumption for scaling: internal reference with `AVDD - AVSS = 5 V`
- Shunt value: `1 kOhm`

## DAC constants used

Static DAC rails:

| DAC | Signal | Voltage |
|---:|---|---:|
| 1 | `Vcc_wl_read` | 0.0 V |
| 3 | `Vcc_wl_set` | 0.0 V |
| 4 | `Vcc_wl_reset` | 0.0 V |
| 7 | Caravel `VDDIO` | 5.0 V |
| 14 | `VDDa1` | 5.0 V |
| 6 | constant rail | 3.3 V |
| 9 | constant rail | 0.5 V |
| 10 | constant rail | 0.9 V |
| 11 | constant rail | 0.6 V |
| 12 | constant rail | 1.6 V |
| 13 | constant rail | 1.0 V |
| 15 | constant rail | 2.1 V |

Synced packet-window DACs:

| Packet window | DAC | Voltage |
|---|---:|---:|
| FORM | `dac[0]` | 0.5 V |
| READ | `dac[2]` | 0.5 V |
| SET | `dac[0]` | 0.5 V |
| RESET | `dac[5]` | 0.5 V |

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
| FORM | `0x900888FF` | SET | set shunt, `dac[0]` |
| READ1 | `0x500888FF` | READ | read shunt, `dac[2]` |
| SET | `0xD00888FF` | SET | set shunt, `dac[0]` |
| READ2 | `0x500888FF` | READ | read shunt, `dac[2]` |
| RESET | `0x100888FF` | RESET | reset shunt, `dac[5]` |
| READ3 | `0x500888FF` | READ | read shunt, `dac[2]` |

## Fixed sampling verification

| Packet | Teensy monitor samples | Teensy monitor duration |
|---|---:|---:|
| FORM | 38 | 43.356 ms |
| READ1 | 38 | 43.442 ms |
| SET | 38 | 43.356 ms |
| READ2 | 38 | 43.356 ms |
| RESET | 38 | 43.528 ms |
| READ3 | 38 | 43.356 ms |

The summary table below reports `39` samples per packet because the Python parser includes the final sample after the capture window closes.

## Result

Raw ADS1258 current polarity was negative in this run, so peak deltas below are reported as magnitudes.

| Packet | Samples | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---:|---|
| FORM | 39 | 39.0847 uA | 9.7580 uA | 12.0683 uA | FORM used SET path |
| READ1 | 39 | 478.7288 uA | 5.1256 uA | 15.4859 uA | READ current present |
| SET | 39 | 38.4359 uA | 12.4655 uA | 20.8415 uA | SET shunt around 12.5 uA |
| READ2 | 39 | 499.9724 uA | 2.3865 uA | 13.8359 uA | READ repeat matches expected range |
| RESET | 39 | 18.3292 uA | 5.0064 uA | 708.3693 uA | RESET shunt response is dominant |
| READ3 | 39 | 73.9308 uA | 3.6907 uA | 19.3123 uA | READ3 is lower than READ1/READ2 in this run |

## Comparison to first 2 MHz run before fixed sampling

Previous 2 MHz run: `/home/ubuntu-24-04/saleae-api/captures/sync-control-form-rsrr-2mhz-vddio-vdda1-5v-wl0-allwindows05-20260808-141418`

| Packet | Previous samples | Fixed samples | Previous read delta | Fixed read delta | Previous set delta | Fixed set delta | Previous reset delta | Fixed reset delta |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| FORM | 12 | 39 | 15.9940 uA | 39.0847 uA | 9.8109 uA | 9.7580 uA | 12.4672 uA | 12.0683 uA |
| READ1 | 11 | 39 | 540.5450 uA | 478.7288 uA | 2.1979 uA | 5.1256 uA | 19.1799 uA | 15.4859 uA |
| SET | 12 | 39 | 18.5427 uA | 38.4359 uA | 12.6293 uA | 12.4655 uA | 23.5292 uA | 20.8415 uA |
| READ2 | 12 | 39 | 434.6279 uA | 499.9724 uA | 2.6878 uA | 2.3865 uA | 12.6426 uA | 13.8359 uA |
| RESET | 12 | 39 | 10.6516 uA | 18.3292 uA | 4.4205 uA | 5.0064 uA | 248.4777 uA | 708.3693 uA |
| READ3 | 12 | 39 | 481.6433 uA | 73.9308 uA | 2.1945 uA | 3.6907 uA | 17.6208 uA | 19.3123 uA |

## Notes

- Console timestamps are host receive times and are affected by USB serial buffering. Use the Teensy `t_us` values for measurement-window timing.
- The fixed-count handshake now makes ADC sampling count comparable across Caravel frequency changes.
- RESET current increased substantially with the longer fixed measurement window in this 2 MHz run.
