# ADC Shunt Current Monitor RSRR Packet Swap Report

Date: 2026-08-02

Experiment title: ADC shunt current monitoring during read -> set -> read -> reset packets, before and after packet DAC swap

## Purpose

This experiment validated the Teensy ADC setup for monitoring three 1 kOhm shunt currents while sending a read -> set -> read -> reset packet sequence.

The first monitored run showed that the read packets produced their largest response on the set shunt. The read and set packet DAC outputs were then swapped in firmware and the same monitored packet sequence was rerun.

## Remote Setup

- Remote PC: `ubuntu-24`
- Tailscale IP: `100.98.132.51`
- Teensy USB serial: `/dev/serial/by-id/usb-Teensyduino_USB_Serial_8829000-if00`
- Teensy board: Teensy 4.1, detected at `usb1/1-2`
- Logic analyzer: Saleae Logic Pro 16 automation on port `10430`

## ADC And Shunt Setup

ADC devices are ADS1115 parts on the Teensy I2C bus.

| ADC | I2C address | Differential pair | Signal monitored |
|---|---:|---|---|
| ADS1 | `0x48` | `A0-A1` | read shunt |
| ADS1 | `0x48` | `A2-A3` | set shunt |
| ADS2 | `0x49` | `A0-A1` | reset / VDDA2 shunt |

Important finding: ADS2 was expected in older code at `0x4A`, but hardware responded at `0x49`.

ADC configuration:

- Gain: `GAIN_SIXTEEN`
- Data rate: `RATE_ADS1115_475SPS`
- Conversion scale: `0.0078125 mV/count`
- Shunt resistance: `1000 ohm`

For a 1 kOhm shunt:

```text
I_shunt_uA = V_shunt_mV
```

This means 1 mV measured across the shunt corresponds to 1 uA of current.

## Zero-Packet ADC Baseline

To measure the real no-active-packet current, the Teensy was run with all packet DAC targets set to `0 mV`:

```text
RSRR 0 0 0 1000
```

This keeps the read, set, and reset packet DAC outputs at zero while the firmware samples the same three differential ADC shunt channels. The run collected `348` ADC samples.

| Shunt | Mean shunt voltage | Mean current | Min current | Max current | Mean ADS counts |
|---|---:|---:|---:|---:|---:|
| Read | `-0.114 mV` | `-0.114 uA` | `-0.211 uA` | `-0.023 uA` | `-14.6` |
| Set | `-0.066 mV` | `-0.066 uA` | `-0.148 uA` | `+0.023 uA` | `-8.4` |
| Reset | `+0.969 mV` | `+0.969 uA` | `+0.703 uA` | `+1.164 uA` | `+124.0` |

Baseline conclusion:

- read and set shunts are close to zero within small ADC offset/noise
- reset shunt has a repeatable positive baseline near `+1 uA` even with packet DAC targets at zero

## Saleae Probe Validation

Before the packet swap experiment, a DAC smoke test checked the new logic analyzer probe locations.

| Saleae channel | Probed node | Observed max during 0.5 V smoke |
|---|---|---:|
| LA2 / A2 | GPIO33 / Vcc_read probe | `0.510 V` |
| LA3 / A3 | VDDA2 / Vcc_reset probe | `0.521 V` |

This confirmed that the probe locations could see the expected voltage steps.

## Saleae Packet Voltage Measurements

After reconnecting `LA1 / A1` to GPIO27, which is the probe point for the `dac[2]` read path, a post-swap `RSRR` run was captured with Saleae analog channels `A0-A15` at `31.25 kS/s`. The paired Teensy serial log confirmed:

```text
RSRR_BEGIN read_mV=1700 set_mV=500 reset_mV=500 hold_ms=150
```

Packet voltage observations:

| Packet voltage | Firmware target | Firmware DAC path | Saleae observation | Measured Saleae value | Note |
|---|---:|---|---|---:|---|
| READ1 | `1.700 V` | `dac[2]` | `A1 / LA1`, GPIO27 / `dac[2]` read-path probe | mean `1.705 V`, max `1.715 V`, width `153.12 ms` | Direct read-path measurement after moving LA1 to GPIO27. |
| SET | `0.500 V` | `dac[0]` | `A2 / LA2`, GPIO33 set-path probe | mean `0.499 V`, max `0.505 V`, width `153.12 ms` | Direct set-path measurement. |
| READ2 | `1.700 V` | `dac[2]` | `A1 / LA1`, GPIO27 / `dac[2]` read-path probe | mean `1.705 V`, max `1.715 V`, width `153.12 ms` | Second read packet matches READ1. |
| RESET | `0.500 V` | `dac[5]` | `A3 / LA3`, VDDA2 / Vcc_reset probe | mean `0.505 V`, max `0.521 V`, width `153.15 ms` | Direct reset-probe measurement. |

The packet order on Saleae was `A1 read`, `A2 set`, `A1 read`, `A3 reset`, matching the swapped firmware command order. `A11` still sits around `1.66 V` during the full `A0-A15` capture, but it is an unprofiled analog view of the digital/XCLK-connected channel and is not the read packet.

Paired ADC current result from the same LA1/GPIO27 rerun:

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `1.0703 uA` | `0.1328 uA` | `0.5781 uA` | read shunt is the dominant read/set response |
| SET | `0.0937 uA` | `0.3984 uA` | `0.7344 uA` | set shunt is the dominant read/set response |
| READ2 | `1.1406 uA` | `0.1016 uA` | `0.3516 uA` | read shunt is again the dominant read/set response |
| RESET | `0.0703 uA` | `0.1719 uA` | `1.3281 uA` | reset shunt is the dominant response |

## Saleae Analog Voltage Snapshot

The table below uses the full `A0-A15` Saleae capture taken during the LA1/GPIO27 post-swap RSRR run. Values are min / mean / max across the 5.2 s capture, so `A1`, `A2`, and `A3` include the packet pulse windows.

| Saleae channel | Bench label | Min | Mean | Max | Note |
|---|---|---:|---:|---:|---|
| A0 | Profile `Vcc_read` / possible packet path | `-0.077 V` | `0.008 V` | `0.098 V` | No packet pulse seen. |
| A1 | LA1 GPIO27 / `dac[2]` read-path probe | `0.003 V` | `0.107 V` | `1.715 V` | Two read pulse segment means: `1.705 V`, `1.705 V`. |
| A2 | LA2 GPIO33 set-path probe | `-0.010 V` | `0.018 V` | `0.505 V` | Set pulse segment mean `0.499 V`. |
| A3 | LA3 VDDA2 / Vcc_reset probe | `-0.004 V` | `0.023 V` | `0.521 V` | Reset pulse segment mean `0.505 V`. |
| A4 | Iref / Vcc_wl_reset profile-dependent | `0.495 V` | `0.502 V` | `0.510 V` | Steady analog rail. |
| A5 | Unprofiled / digital reset-connected channel | `2.999 V` | `3.026 V` | `3.040 V` | Digital-connected, not treated as an analog bias rail. |
| A6 | VDDc2 / Vccd2 | `2.091 V` | `2.107 V` | `2.122 V` | Steady analog rail. |
| A7 | Vcomp | `0.899 V` | `0.908 V` | `0.915 V` | Steady analog rail. |
| A8 | Unprofiled | `-0.005 V` | `0.015 V` | `0.036 V` | No packet pulse seen. |
| A9 | Unprofiled | `-0.003 V` | `0.017 V` | `0.038 V` | No packet pulse seen. |
| A10 | Unprofiled | `0.020 V` | `0.043 V` | `0.061 V` | No packet pulse seen. |
| A11 | Unprofiled / digital XCLK-connected channel | `1.478 V` | `1.663 V` | `1.846 V` | Analog view of digital activity, not the read packet voltage. |
| A12 | Vbias | `1.590 V` | `1.601 V` | `1.611 V` | Steady analog rail. |
| A13 | Bias_comp2 | `0.605 V` | `0.613 V` | `0.621 V` | Steady analog rail. |
| A14 | dc_bias | `0.989 V` | `0.998 V` | `1.005 V` | Steady analog rail. |
| A15 | VDDa1 | `3.023 V` | `3.037 V` | `3.054 V` | Monitor rail; below the nominal `5.0 V` firmware target. |

## Firmware Method

A new `RSRR` command was added to the Teensy firmware.

```text
RSRR
RSRR <read_mV> <set_mV> <reset_mV> <hold_ms>
```

For each packet:

1. Sample all three shunts before driving the packet.
2. Drive the packet DAC output.
3. Keep sampling all three shunts until the packet window ends.
4. Turn the packet DAC back off.
5. Print a `PACKET_SUMMARY` line.

Firmware update after the history-dependence check:

```text
READ1 -> SET -> READ2 -> RESET -> READ3
```

After `READ3`, the firmware explicitly writes `0 mV` to `dac[2]`, `dac[0]`, and `dac[5]`, waits `20 ms`, prints:

```text
PACKET_DACS_ZEROED dac2=0mV dac0=0mV dac5=0mV
SHUNT_SNAPSHOT label=FINAL_ZERO ...
```

The summary reports:

- baseline current
- final current
- final delta from baseline
- min/max span
- peak absolute delta from baseline

Note: ADS1115 conversion time is milliseconds, so this monitor can characterize packet windows that are held for milliseconds. It cannot resolve a true 10 us pulse waveform directly.

## Wishbone Packet Definitions

The read, set, and reset packet names in this report refer to logical ReRAM/X1 command windows. In the Caravel RISC-V Wishbone firmware, those commands are sent as 32-bit writes to the user-project Wishbone address:

```c
#define NEURO_ADDR 0x30000004
REG32(NEURO_ADDR) = <packet_word>;
```

The DAC swap changed which Teensy DAC rail was driven/monitored during each logical packet window. It did not change the Wishbone command words sent by the Caravel firmware.

Common setup/config writes used by the `set_mode_wb` and `reset_mode_wb` firmware before the operation packet:

| Order | WB write | Purpose |
|---:|---:|---|
| 1 | `REG32(0x30000004) = 0x00036472` | target-set configuration |
| 2 | `REG32(0x30000004) = 0x462B000B` | target-reset configuration |
| 3 | `REG32(0x30000004) = 0x44001405` | timing/configuration |

Operation packet words in the checked-in `Firmware_wishbone` mode files:

| Logical packet | WB transaction | Packet meaning |
|---|---:|---|
| READ | `REG32(0x30000004) = 0x500888FF` | read command used after set/reset operation firmware |
| SET | `REG32(0x30000004) = 0xD00888A2` | program-set command |
| RESET | `REG32(0x30000004) = 0x10088806` | program-reset command |

DAC rail mapping before and after the Teensy swap:

| Logical packet | WB word | Before swap DAC rail | After swap DAC rail | Current READ3 firmware rail |
|---|---:|---:|---:|---:|
| READ | `0x500888FF` | `dac[0]` | `dac[2]` | `dac[2]` for `READ1`, `READ2`, and `READ3` |
| SET | `0xD00888A2` | `dac[2]` | `dac[0]` | `dac[0]` |
| RESET | `0x10088806` | `dac[5]` | `dac[5]` | `dac[5]` |

Current monitored sequence:

```text
READ1 -> SET -> READ2 -> RESET -> READ3
```

In WB terms, using the checked-in `set_mode_wb` / `reset_mode_wb` packet words, that logical sequence is:

```text
0x500888FF -> 0xD00888A2 -> 0x500888FF -> 0x10088806 -> 0x500888FF
```

Important packet-format note: the newer PARTCL/X1 documentation in this repository also describes a corrected row/column packet builder. For row `0`, column `0`, that corrected format gives examples like `READ = 0x400000FF`, `SET = 0xC00000A2`, and `RESET = 0x00000006`. The `Firmware_wishbone` mode files used in this bench history still contain the older fixed words listed above.

## Before Swap

Firmware mapping before the swap:

| Packet | DAC channel driven |
|---|---:|
| READ1 / READ2 | `dac[0]` |
| SET | `dac[2]` |
| RESET | `dac[5]` |

Run command:

```text
RSRR
```

Effective values:

- read packet: `1700 mV`
- set packet: `500 mV`
- reset packet: `500 mV`
- hold time: `150 ms`
- samples per packet: `15`

Peak current delta from packet baseline:

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `0.1172 uA` | `1.0000 uA` | `0.1797 uA` | read packet mostly appeared on set shunt |
| SET | `0.1250 uA` | `0.1250 uA` | `0.3594 uA` | set packet did not dominate set shunt |
| READ2 | `0.0859 uA` | `1.0859 uA` | `0.2187 uA` | read packet again mostly appeared on set shunt |
| RESET | `0.0937 uA` | `0.0937 uA` | `1.1875 uA` | reset packet appeared on reset shunt |

Conclusion before swap:

The reset packet was routed correctly, but read packets were producing their strongest current change on the set shunt. This indicated that the read and set packet DAC routes were swapped.

## After Swap

Firmware mapping after the swap:

| Packet | DAC channel driven |
|---|---:|
| READ1 / READ2 | `dac[2]` |
| SET | `dac[0]` |
| RESET | `dac[5]` |

Run command:

```text
RSRR
```

Effective values:

- read packet: `1700 mV`
- set packet: `500 mV`
- reset packet: `500 mV`
- hold time: `150 ms`
- samples per packet: `15`

Peak current delta from packet baseline:

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `0.1875 uA` | `0.0781 uA` | `0.2266 uA` | read shunt now larger than set shunt |
| SET | `0.0625 uA` | `0.3984 uA` | `0.3594 uA` | set shunt now strongest among read/set shunts |
| READ2 | `0.1641 uA` | `0.1016 uA` | `0.3359 uA` | read shunt remains larger than set shunt |
| RESET | `0.0859 uA` | `0.0703 uA` | `0.9844 uA` | reset packet remains strongest on reset shunt |

Conclusion after swap:

The read/set packet routing is now consistent with the shunt monitor:

- read packets no longer produce their dominant read/set response on the set shunt
- set packet produces the strongest read/set response on the set shunt
- reset packet remains correctly visible on the reset / VDDA2 shunt

The reset shunt still shows background movement during read packets, so treat it as a monitored channel rather than a perfectly isolated indicator.

## READ3 Final-Zero Firmware Update

Concern tested: the high reset baseline could be caused by history from the previous packet/state. Before changing firmware, a read-only active sequence was sent, followed immediately by an all-zero packet baseline:

```text
RSRR 1700 0 0 150
RSRR 0 0 0 1000
```

The zero baseline after the read-only active sequence shifted strongly:

| Condition | Read mean | Set mean | Reset mean | Observation |
|---|---:|---:|---:|---|
| Earlier zero baseline | `-0.114 uA` | `-0.066 uA` | `+0.969 uA` | low baseline |
| Zero baseline after read-only active sequence | `-6.379 uA` | `-4.456 uA` | `+25.764 uA` | strong history-dependent state |

This confirmed that the measured zero-current state depends on the previous packet/device state. It does not prove that the DAC output was electrically stuck, because the firmware was already writing those DAC channels back to zero, but it does show that ending history matters for the observed shunt currents.

The firmware was then changed and flashed so the monitored command now ends with a third read packet:

```text
READ1 -> SET -> READ2 -> RESET -> READ3 -> zero dac[2], dac[0], dac[5]
```

Post-flash default `RSRR` packet peak current deltas:

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `1.1094 uA` | `0.0391 uA` | `0.2891 uA` | read shunt is dominant |
| SET | `0.1406 uA` | `0.4219 uA` | `0.1953 uA` | set shunt is dominant |
| READ2 | `1.0625 uA` | `0.1016 uA` | `0.2969 uA` | read shunt is dominant |
| RESET | `0.0781 uA` | `0.0937 uA` | `1.1484 uA` | reset shunt is dominant |
| READ3 | `1.0938 uA` | `0.0703 uA` | `0.2031 uA` | final active packet is read |

Final ADC snapshot after the default `RSRR` command zeroed `dac[2]`, `dac[0]`, and `dac[5]`:

| Snapshot | Read current | Set current | Reset current |
|---|---:|---:|---:|
| `FINAL_ZERO` | `-0.1016 uA` | `-0.0469 uA` | `+1.0313 uA` |

The longer all-zero baseline immediately after the READ3 firmware run collected `435` samples:

| Shunt | Mean current | Min current | Max current | Mean ADS counts |
|---|---:|---:|---:|---:|
| Read | `-0.116 uA` | `-0.203 uA` | `-0.031 uA` | `-14.8` |
| Set | `-0.065 uA` | `-0.164 uA` | `+0.023 uA` | `-8.3` |
| Reset | `+0.967 uA` | `+0.711 uA` | `+1.242 uA` | `+123.8` |

Conclusion: after the new READ3-ended sequence and explicit final DAC-zero step, the all-zero baseline returned to the earlier low baseline. The reset shunt still has a persistent about `+1 uA` offset/leakage baseline, but the large `+25.8 uA` history-dependent state was not present after the updated sequence.

## Source Logs

Remote source data:

| Run | Path |
|---|---|
| Conservative all-0.5 V smoke before swap | `/home/ubuntu-24-04/saleae-api/captures/rsrr-adc-monitor-20260802-143743/summary.json` |
| Default RSRR before swap | `/home/ubuntu-24-04/saleae-api/captures/rsrr-adc-monitor-default-20260802-143815/summary.json` |
| Default RSRR after swap | `/home/ubuntu-24-04/saleae-api/captures/rsrr-adc-monitor-swapped-20260802-144403/summary.json` |
| Saleae A2/A3 probe validation | `/home/ubuntu-24-04/saleae-api/captures/adc-dac-shunt-smoke-a2a3-only-20260802-142541/summary.json` |
| Post-swap RSRR Saleae A0-A15 voltage capture | `/home/ubuntu-24-04/saleae-api/captures/rsrr-saleae-a0-a15-startedfirst-20260802-153149/summary.json` |
| Post-swap RSRR Saleae A0-A15 serial log | `/home/ubuntu-24-04/saleae-api/captures/rsrr-saleae-a0-a15-startedfirst-20260802-153149/serial.log` |
| Corrected LA1/GPIO27 post-swap RSRR Saleae A0-A15 voltage capture | `/home/ubuntu-24-04/saleae-api/captures/rsrr-saleae-a0-a15-la1-gpio27-20260802-154424/summary.json` |
| Corrected LA1/GPIO27 post-swap RSRR serial log | `/home/ubuntu-24-04/saleae-api/captures/rsrr-saleae-a0-a15-la1-gpio27-20260802-154424/serial.log` |
| Zero-packet ADC baseline | `/home/ubuntu-24-04/saleae-api/captures/adc-baseline-no-packet-zero-dac-20260802-155527/summary.json` |
| Zero-packet ADC baseline serial log | `/home/ubuntu-24-04/saleae-api/captures/adc-baseline-no-packet-zero-dac-20260802-155527/serial.log` |
| Read-only active sequence followed by zero baseline | `/home/ubuntu-24-04/saleae-api/captures/adc-baseline-after-read-packet-20260802-160354/summary.json` |
| READ3 firmware post-flash default RSRR and zero baseline | `/home/ubuntu-24-04/saleae-api/captures/rsrr-read3-finalzero-adc-20260802-160700/summary.json` |
| ADC serial smoke validation | `/home/ubuntu-24-04/saleae-api/captures/adc-dac-shunt-smoke-20260802-142222/serial_adc.log` |

## Current Firmware State

The remote Teensy was flashed with the swapped RSRR mapping after the experiment.

The RSRR command now uses:

```text
READ1 -> dac[2]
SET   -> dac[0]
READ2 -> dac[2]
RESET -> dac[5]
READ3 -> dac[2]
FINAL -> dac[2] = 0 mV, dac[0] = 0 mV, dac[5] = 0 mV
```

ADS2 is initialized at `0x49`.

## cadance simulation observation
<img width="710" height="251" alt="image" src="https://github.com/user-attachments/assets/e45f19a5-3081-4bce-9d63-d67509bb839e" />
