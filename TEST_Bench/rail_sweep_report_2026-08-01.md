# Corrected DAC Rail Sweep Report

Date: 2026-08-01

Firmware: `Firmware_read_form_set_reset/Firmware_wishbone/read_mode_wb/read_mode_wb.c`

This rerun corrects the earlier baseline mistake where Vcomp was held at `2.0 V`. In this run, Vcomp was held at `0.9 V` whenever Vcomp was not the swept variable.

## Firmware Checks

Before running the sweep:

- DAC Teensy firmware was checked over USB serial.
- Teensy reported `DAC[10] Vcomp = 0.900 V code=11796`.
- Caravel read firmware was checked with the corrected constants.
- Caravel produced a valid repeated x4 sanity readback: `0x0007F444`.
- After the sweep, DAC rails were restored to the corrected constants.

Remote setup:

- Remote PC: `ubuntu-24`
- Tailscale IP: `100.98.132.51`
- DAC Teensy: `/dev/serial/by-id/usb-Teensyduino_USB_Serial_8829000-if00`
- Caravel FTDI: `ftdi://ftdi:232h/1`
- Logic analyzer: Saleae Logic Pro 16 automation on port `10430`

## Decode Method

Readback values were decoded from the final repeated x4 UART readback value.

- Decoded column: `(readback >> 14) & 0x1F`
- TDC raw: `readback & 0x3FFF`
- Decoded TDC coarse: `(TDC raw >> 8) & 0x3F`
- Decoded TDC fine: `TDC raw & 0xFF`

## DAC And Saleae Channel Map

If the Saleae UI shows raw channel labels, use this map.

| Signal | DAC channel | Saleae channel |
|---|---:|---:|
| Iref | DAC[9] | A4 |
| Vcomp | DAC[10] | A7 |
| Bias_comp2 | DAC[11] | A13 |
| Vbias | DAC[12] | A12 |
| dc_bias | DAC[13] | A14 |
| VDDa1 | DAC[14] | A15 |
| VDDc2/Vccd2 | DAC[15] | A6 |

Digital Saleae channels:

| Signal | Saleae channel |
|---|---:|
| Reset | D5 |
| ScanInDL | D8 |
| ScanInDR | D9 |
| TM | D10 |
| Xclk | D11 |

Note: VDDa1 was commanded to `5.0 V`, but Saleae A15 measured about `3.03 V` in the setup check. It was treated as monitor-only, consistent with the previous run.

## Constant Values Used

For each sweep, the target signal was varied and all other rails were reset to the constants shown here before each run.

| Sweep experiment | Swept values | Iref | Vcomp | Bias_comp2 | Vbias | dc_bias | VDDa1 | VDDc2/Vccd2 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Bias_comp2 sweep | 0.0, 0.6, 1.2, 1.8, 2.0 V | 0.5 V | 0.9 V | swept | 1.6 V | 1.0 V | 5.0 V | 2.1 V |
| dc_bias sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | 0.5 V | 0.9 V | 0.6 V | 1.6 V | swept | 5.0 V | 2.1 V |
| Iref sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | swept | 0.9 V | 0.6 V | 1.6 V | 1.0 V | 5.0 V | 2.1 V |
| Vbias sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | 0.5 V | 0.9 V | 0.6 V | swept | 1.0 V | 5.0 V | 2.1 V |
| Vcomp sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | 0.5 V | swept | 0.6 V | 1.6 V | 1.0 V | 5.0 V | 2.1 V |

Initial all-rail Saleae setup check:

| Signal | Saleae channel | Expected | Saleae mean | Status |
|---|---:|---:|---:|---|
| Iref | A4 | 0.500000 V | 0.505874 V | OK |
| VDDc2/Vccd2 | A6 | 2.100000 V | 2.111624 V | OK |
| Vcomp | A7 | 0.900000 V | 0.910413 V | OK |
| Vbias | A12 | 1.600000 V | 1.603785 V | OK |
| Bias_comp2 | A13 | 0.600000 V | 0.614260 V | OK |
| dc_bias | A14 | 1.000000 V | 1.000594 V | OK |
| VDDa1 | A15 | 5.000000 V | 3.029238 V | MONITOR |

## Combined Results

| Target variable | Saleae mean | x4 readback value | Decoded column | Decoded TDC coarse | Decoded TDC fine |
|---|---:|---|---:|---:|---:|
| Bias_comp2 = 0.0 V | 0.013931 V | 0x0007FF26 | 31 | 63 | 38 |
| Bias_comp2 = 0.6 V | 0.614826 V | 0x0007F541 | 31 | 53 | 65 |
| Bias_comp2 = 1.2 V | 1.212882 V | 0x0007E81A | 31 | 40 | 26 |
| Bias_comp2 = 1.8 V | 1.810688 V | 0x0007FF43 | 31 | 63 | 67 |
| Bias_comp2 = 2.0 V | 2.010245 V | 0x0007FF0C | 31 | 63 | 12 |
| dc_bias = 0.0 V | 0.004734 V | 0x0007E304 | 31 | 35 | 4 |
| dc_bias = 0.5 V | 0.503665 V | 0x0007F444 | 31 | 52 | 68 |
| dc_bias = 1.0 V | 1.001437 V | 0x0007F43D | 31 | 52 | 61 |
| dc_bias = 1.5 V | 1.501006 V | 0x0007F459 | 31 | 52 | 89 |
| dc_bias = 2.0 V | 1.998713 V | 0x0007F443 | 31 | 52 | 67 |
| Iref = 0.0 V | 0.007800 V | 0x0007F733 | 31 | 55 | 51 |
| Iref = 0.5 V | 0.505293 V | 0x0007F437 | 31 | 52 | 55 |
| Iref = 1.0 V | 1.005536 V | 0x0007CE05 | 31 | 14 | 5 |
| Iref = 1.5 V | 1.502784 V | 0x0007C902 | 31 | 9 | 2 |
| Iref = 2.0 V | 2.001799 V | 0x0007CA46 | 31 | 10 | 70 |
| Vbias = 0.0 V | 0.007675 V | 0x0007BF00 | 30 | 63 | 0 |
| Vbias = 0.5 V | 0.506235 V | 0x0007BF00 | 30 | 63 | 0 |
| Vbias = 1.0 V | 1.005078 V | 0x0007BF00 | 30 | 63 | 0 |
| Vbias = 1.5 V | 1.503429 V | 0x0007F743 | 31 | 55 | 67 |
| Vbias = 2.0 V | 2.002596 V | 0x0007CC46 | 31 | 12 | 70 |
| Vcomp = 0.0 V | 0.010511 V | 0x0007FF60 | 31 | 63 | 96 |
| Vcomp = 0.5 V | 0.509093 V | 0x0007FF61 | 31 | 63 | 97 |
| Vcomp = 1.0 V | 1.009011 V | 0x0007E91C | 31 | 41 | 28 |
| Vcomp = 1.5 V | 1.508444 V | 0x0007D005 | 31 | 16 | 5 |
| Vcomp = 2.0 V | 2.005915 V | 0x0007D108 | 31 | 17 | 8 |

## Source Data

Corrected sweep root on the remote:

`/home/ubuntu-24-04/saleae-api/captures/corrected-vcomp0p9-auto-20260801-161908`

Main results CSV:

`/home/ubuntu-24-04/saleae-api/captures/corrected-vcomp0p9-auto-20260801-161908/results.csv`

Channel legend:

`/home/ubuntu-24-04/saleae-api/captures/corrected-vcomp0p9-auto-20260801-161908/channel_legend.txt`

Each sweep point directory contains:

- `analog.csv`
- `digital.csv`
- `profile_metadata.json`
- `riscv_profile_capture.sal`
- `spi_3_export.csv`
- per-point UART log in the sweep root

