# ADS1258 RSRR READ3 Final-Zero Rerun

Date: 2026-08-05

Source experiment: `TEST_Bench/adc_shunt_current_rsrr_swap_report_2026-08-02.md`, section `READ3 Final-Zero Firmware Update`.

## Setup

- Remote PC: `ubuntu-24`, Tailscale `100.98.132.51`
- Teensy USB serial: `/dev/serial/by-id/usb-Teensyduino_USB_Serial_8829000-if00`
- Teensy USB ID: `16c0:0483`
- ADC: ADS1258, ID register `0x8B`, ID check `PASS`
- Shunt value: `1 kOhm`
- Current conversion used by firmware: `I_shunt_uA = V_shunt_mV`
- Runtime command: `RSRR`
- Effective packet targets: read `1700 mV`, set `500 mV`, reset `500 mV`, hold `150 ms`

## ADS1258 Channel Map Finding

The first ADS1258 rerun did not match the ADS1115 trend because the assumed ADS1258 differential channel order was wrong. Runtime isolation commands showed:

- read packet response is on ADS1258 `DIFF1`
- set packet response is smaller but best aligned to ADS1258 `DIFF0`
- reset packet response is on ADS1258 `DIFF2`

The main Teensy sketch was updated to use this logical map:

| Logical shunt | ADS1258 differential channel |
|---|---|
| Read shunt | `DIFF1` |
| Set shunt | `DIFF0` |
| Reset shunt | `DIFF2` |

No DAC packet target or packet order was changed.

## Default RSRR Packet Result

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `566.3859 uA` | `1.1107 uA` | `4.0874 uA` | read shunt is dominant |
| SET | `5.0538 uA` | `5.5180 uA` | `5.8797 uA` | set is not clearly dominant at the old `500 mV` target |
| READ2 | `563.7468 uA` | `1.2976 uA` | `4.9591 uA` | read shunt is dominant |
| RESET | `4.9852 uA` | `1.2938 uA` | `196.6941 uA` | reset shunt is dominant |
| READ3 | `563.9642 uA` | `1.0955 uA` | `4.1943 uA` | final active packet is read |

Final snapshot after default `RSRR` zeroed `dac[2]`, `dac[0]`, and `dac[5]`:

| Snapshot | Read current | Set current | Reset current |
|---|---:|---:|---:|
| `FINAL_ZERO` | `405.7744 uA` | `9.6715 uA` | `6.8887 uA` |

## All-Zero Baseline After READ3 Run

Command:

```text
RSRR 0 0 0 1000
```

Collected `4180` monitor samples.

| Shunt | Mean current | Min current | Max current |
|---|---:|---:|---:|
| Read | `402.3006 uA` | `397.6835 uA` | `408.4396 uA` |
| Set | `9.2829 uA` | `7.5397 uA` | `10.8891 uA` |
| Reset | `5.0645 uA` | `3.0193 uA` | `10.2812 uA` |

Final snapshot after the all-zero command:

| Snapshot | Read current | Set current | Reset current |
|---|---:|---:|---:|
| `FINAL_ZERO` | `400.7708 uA` | `8.8183 uA` | `4.5179 uA` |

## Conclusion

The ADS1258 rerun preserves the qualitative READ and RESET dominance after correcting the logical ADC channel map, but it does not numerically match the old ADS1115 table.

The largest differences are:

- read shunt deltas are hundreds of microamps on ADS1258 versus about `1 uA` on ADS1115
- all-zero read baseline is about `402 uA` on ADS1258 versus near zero on ADS1115
- set packet at `500 mV` is not clearly dominant over the reset channel in the ADS1258 peak-delta table

This points to an ADS1258 analog channel wiring, polarity, scaling, or common-mode/reference difference. It does not look like a DAC packet-order regression: read packets still drive the logical read response and reset drives the logical reset response after the map fix.

## Source Logs

| Run | Path |
|---|---|
| First ADS1258 rerun before channel-map fix | `/home/ubuntu-24-04/saleae-api/captures/rsrr-read3-finalzero-ads1258-20260805-155422/summary.json` |
| ADS1258 isolation commands | `/home/ubuntu-24-04/saleae-api/captures/rsrr-read3-finalzero-ads1258-20260805-155422/isolation_summary.json` |
| ADS1258 rerun after channel-map fix | `/home/ubuntu-24-04/saleae-api/captures/rsrr-read3-finalzero-ads1258-remap-20260805-155658/summary.json` |

## Polarity-Reversal Rerun

After reversing the ADS1258 shunt polarity, the same firmware and same runtime commands were rerun. No firmware change was made for this rerun.

Pre-run `ADC?` status:

| ADS1258 ID | Check | Read current | Set current | Reset current |
|---|---|---:|---:|---:|
| `0x8B` | `PASS` | `-414.4802 uA` | `-10.1077 uA` | `-4.3278 uA` |

Default `RSRR` packet peak current deltas:

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `583.7504 uA` | `1.5462 uA` | `6.7959 uA` | read shunt is dominant |
| SET | `18.0760 uA` | `4.7404 uA` | `4.2426 uA` | read shunt movement dominates at old `500 mV` set target |
| READ2 | `569.1833 uA` | `1.3014 uA` | `4.1746 uA` | read shunt is dominant |
| RESET | `18.4078 uA` | `1.4159 uA` | `195.8262 uA` | reset shunt is dominant |
| READ3 | `593.3049 uA` | `1.4782 uA` | `4.7932 uA` | final active packet is read |

Final snapshot after default `RSRR`:

| Snapshot | Read current | Set current | Reset current |
|---|---:|---:|---:|
| `FINAL_ZERO` | `-401.0474 uA` | `-9.6531 uA` | `-6.4042 uA` |

All-zero baseline after the polarity-reversal run:

| Shunt | Mean current | Min current | Max current |
|---|---:|---:|---:|
| Read | `-401.6500 uA` | `-422.4192 uA` | `-365.2795 uA` |
| Set | `-9.4288 uA` | `-11.0944 uA` | `-7.5582 uA` |
| Reset | `-5.3359 uA` | `-11.6876 uA` | `-3.3830 uA` |

Conclusion after polarity reversal:

- polarity reversal flipped the sign of the idle shunt currents
- the read baseline magnitude stayed around `400 uA`
- READ and RESET still show the expected dominant logical channels
- SET still does not reproduce the old ADS1115 trend at the prior `500 mV` set target

This confirms the main mismatch is not just a firmware sign convention. The ADS1258 is seeing a real differential voltage/current offset on the read channel with the opposite polarity after rewiring.

| Run | Path |
|---|---|
| ADS1258 polarity-reversal rerun | `/home/ubuntu-24-04/saleae-api/captures/rsrr-read3-finalzero-ads1258-polarity-rerun-20260805-160706/summary.json` |

## Saleae A9/A10 Set-Shunt Scaling Check

User wiring update: Saleae analog channels `A9` and `A10` were connected across the set shunt.

The firmware was still using:

```text
ADS1258_VREF_VOLTS = 5.000
```

An A9/A10 sweep was captured while sending set-only packet commands:

```text
RSRR 0 <set_mV> 0 150
```

The Saleae differential was calculated as:

```text
Vset_shunt = A9 - A10
Iset_uA = Vset_shunt_mV / 1 kOhm
```

Before the scaling fix:

| Set target | Saleae set-window delta | ADS1258 set peak delta | Observation |
|---:|---:|---:|---|
| `0 mV` | `0.5247 uA` | `1.4140 uA` | near noise floor |
| `500 mV` | `1.9842 uA` | `4.6927 uA` | ADS1258 too high |
| `1000 mV` | `3.8858 uA` | `8.0427 uA` | ADS1258 too high |
| `1700 mV` | `7.4904 uA` | `13.0374 uA` | ADS1258 too high |

The error was the reference voltage used by the ADS1258 conversion code. The hardware/reference behavior matched a `2.5 V` ADC reference, not `AVDD-AVSS = 5 V`.

Firmware correction:

```text
ADS1258_VREF_VOLTS = 2.500
```

Post-fix check with `RSRR 0 1700 0 150`:

| Measurement | Result |
|---|---:|
| Saleae `A9-A10` set-window delta | `8.7699 uA` |
| ADS1258 set peak delta | `6.5152 uA` |
| Ratio `ADS1258 / Saleae` | `0.743` |

The remaining difference is expected to include Saleae single-ended subtraction noise, window alignment, and the large DC differential baseline seen on `A9-A10` around `255 mV`. The scale is now in the correct range; with the previous `5 V` reference, the ADS1258 result was about 2x too high.

Final serial check after flashing the `2.5 V` scaling firmware:

| Packet | Read shunt peak delta | Set shunt peak delta | Reset shunt peak delta | Observation |
|---|---:|---:|---:|---|
| READ1 | `311.6016 uA` | `0.8024 uA` | `1.6034 uA` | read shunt is dominant |
| SET | `15.2264 uA` | `2.6954 uA` | `2.1973 uA` | set shunt is now slightly above reset, but read-channel movement remains present |
| READ2 | `308.0641 uA` | `0.6030 uA` | `1.8835 uA` | read shunt is dominant |
| RESET | `15.4638 uA` | `0.6148 uA` | `98.2459 uA` | reset shunt is dominant |
| READ3 | `312.7435 uA` | `0.6221 uA` | `2.2882 uA` | final active packet is read |

| Run | Path |
|---|---|
| Saleae A9/A10 set-shunt single capture after scaling fix | `/home/ubuntu-24-04/saleae-api/captures/set-shunt-a9-a10-ads1258-20260805-161919/summary.json` |
| Saleae A9/A10 set-shunt sweep before scaling fix | `/home/ubuntu-24-04/saleae-api/captures/set-shunt-a9-a10-sweep-ads1258-20260805-161747/summary.json` |
