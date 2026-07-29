# DAC Bias Sweep Report

Date: 2026-07-29

Firmware: `Firmware_read_form_set_reset/Firmware_wishbone/read_mode_wb/read_mode_wb.c`

Remote setup:

- Remote PC: `ubuntu-24`
- Tailscale IP: `100.98.132.51`
- Caravel UART: `/dev/serial/by-id/usb-FTDI_Single_RS232-HS-if00-port0`
- DAC Teensy: `/dev/serial/by-id/usb-Teensyduino_USB_Serial_8829000-if00`
- Logic analyzer: Saleae Logic Pro 16 automation on port `10430`

## Decode Method

Readback values were decoded from the final repeated x4 UART readback value.

- Decoded column: `(readback >> 14) & 0x1F`
- TDC raw: `readback & 0x3FFF`
- Decoded TDC coarse: `(TDC raw >> 8) & 0x3F`
- Decoded TDC fine: `TDC raw & 0xFF`

## DAC And Saleae Channel Map

| Signal | DAC channel | Saleae channel |
|---|---:|---:|
| Iref | DAC[9] | A4 |
| Vcomp | DAC[10] | A7 |
| Bias_comp2 | DAC[11] | A13 |
| Vbias | DAC[12] | A12 |
| dc_bias | DAC[13] | A14 |
| VDDa1 | DAC[14] | A15 |
| VDDc2/Vccd2 | DAC[15] | A6 |

Note: Iref was measured on raw Saleae channel 4. The saved Saleae profile label may still show an older label on A4, but the channel was identified by toggling the Iref DAC rail.

## Constant Values Used

For each sweep, the target signal was varied and all other rails were reset to the constants shown here before each run.

| Sweep experiment | Swept values | Iref | Vcomp | Bias_comp2 | Vbias | dc_bias | VDDa1 | VDDc2/Vccd2 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Bias_comp2 sweep | 0.0, 0.6, 1.2, 1.8, 2.0 V | 0.5 V | 2.0 V | swept | 1.6 V | 1.0 V | 5.0 V | 2.1 V |
| dc_bias sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | 0.5 V | 2.0 V | 0.6 V | 1.6 V | swept | 5.0 V | 2.1 V |
| Iref sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | swept | 2.0 V | 0.6 V | 1.6 V | 1.0 V | 5.0 V | 2.1 V |
| Vbias sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | 0.5 V | 2.0 V | 0.6 V | swept | 1.0 V | 5.0 V | 2.1 V |
| Vcomp sweep | 0.0, 0.5, 1.0, 1.5, 2.0 V | 0.5 V | swept | 0.6 V | 1.6 V | 1.0 V | 5.0 V | 2.1 V |

## Combined Results

| Target variable | Saleae variable mean | x4 readback value | Decoded column | Decoded TDC coarse | Decoded TDC fine |
|---|---:|---|---:|---:|---:|
| Bias_comp2 = 0.0 V | 0.013585 V | 0x0007FF26 | 31 | 63 | 38 |
| Bias_comp2 = 0.6 V | 0.612807 V | 0x0007D041 | 31 | 16 | 65 |
| Bias_comp2 = 1.2 V | 1.210924 V | 0x0007CF0B | 31 | 15 | 11 |
| Bias_comp2 = 1.8 V | 1.809534 V | 0x0006FF00 | 27 | 63 | 0 |
| Bias_comp2 = 2.0 V | 2.009476 V | 0x0005FF0D | 23 | 63 | 13 |
| dc_bias = 0.0 V | 0.001686 V | 0x0007BF26 | 30 | 63 | 38 |
| dc_bias = 0.5 V | 0.500723 V | 0x0007CC14 | 31 | 12 | 20 |
| dc_bias = 1.0 V | 0.999024 V | 0x0007D03B | 31 | 16 | 59 |
| dc_bias = 1.5 V | 1.497955 V | 0x0007D605 | 31 | 22 | 5 |
| dc_bias = 2.0 V | 1.997170 V | 0x0007DE41 | 31 | 30 | 65 |
| Iref = 0.0 V | 0.007364 V | 0x0007CE10 | 31 | 14 | 16 |
| Iref = 0.5 V | 0.504591 V | 0x0007D018 | 31 | 16 | 24 |
| Iref = 1.0 V | 1.004847 V | 0x0007CC16 | 31 | 12 | 22 |
| Iref = 1.5 V | 1.504119 V | 0x0007CA05 | 31 | 10 | 5 |
| Iref = 2.0 V | 2.001700 V | 0x0007C906 | 31 | 9 | 6 |
| Vbias = 0.0 V | 0.005723 V | 0x0007C805 | 31 | 8 | 5 |
| Vbias = 0.5 V | 0.503597 V | 0x0007C805 | 31 | 8 | 5 |
| Vbias = 1.0 V | 1.002897 V | 0x0007C90A | 31 | 9 | 10 |
| Vbias = 1.5 V | 1.501428 V | 0x0007CF04 | 31 | 15 | 4 |
| Vbias = 2.0 V | 2.000419 V | 0x0007CB05 | 31 | 11 | 5 |
| Vcomp = 0.0 V | 0.008932 V | 0x0007FF4D | 31 | 63 | 77 |
| Vcomp = 0.5 V | 0.506800 V | 0x0007FF44 | 31 | 63 | 68 |
| Vcomp = 1.0 V | 1.008558 V | 0x0007DA04 | 31 | 26 | 4 |
| Vcomp = 1.5 V | 1.506867 V | 0x0007D005 | 31 | 16 | 5 |
| Vcomp = 2.0 V | 2.004667 V | 0x0007D017 | 31 | 16 | 23 |

## Source Data

Current multi-rail sweep CSV on the remote:

`/home/ubuntu-24-04/saleae-api/captures/multi-rail-sweep-continued-20260729-144933/results.csv`

Bias_comp2 Saleae captures on the remote:

- `/home/ubuntu-24-04/saleae-api/captures/editedfw-bias-comp2-A13-0p0-20260729-130607`
- `/home/ubuntu-24-04/saleae-api/captures/editedfw-bias-comp2-A13-0p6-20260729-130711`
- `/home/ubuntu-24-04/saleae-api/captures/editedfw-bias-comp2-A13-1p2-20260729-130850`
- `/home/ubuntu-24-04/saleae-api/captures/editedfw-bias-comp2-A13-1p8-20260729-130949`
- `/home/ubuntu-24-04/saleae-api/captures/editedfw-bias-comp2-A13-2p0-20260729-131119`

