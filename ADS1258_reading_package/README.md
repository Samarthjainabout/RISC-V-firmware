# ADS1258 Arduino Reading Package

This folder separates the ADS1258 library from the test sketch:

```text
ADS1258_reading_package/
  library/
    ADS1258_Simple/          Arduino library files
  code/
    ADS1258_basic_read/      Example Arduino sketch
  README.md                  Wiring, power, and startup notes
```

The included library is intentionally small: it resets/configures the ADS1258, reads/writes registers, reads the 4-byte channel-data command result, sign-extends the 24-bit ADC code, decodes the status byte, and converts raw codes to volts.

Source reference: Texas Instruments ADS1258 datasheet, Rev. G: <https://www.ti.com/lit/gpn/ADS1258>

## Install the library

Copy this folder:

```text
Arduino/ADS1258_reading_package/library/ADS1258_Simple
```

into your Arduino sketchbook libraries folder:

```text
Arduino/libraries/ADS1258_Simple
```

Then restart the Arduino IDE if it was open.

After that, open:

```text
Arduino/ADS1258_reading_package/code/ADS1258_basic_read/ADS1258_basic_read.ino
```

## SPI connections

The ADS1258 uses an SPI-compatible interface with four main SPI signals plus `DRDY` and conversion-control pins.

| ADS1258 pin | ADS1258 signal | Connect to MCU / Arduino | Notes |
|---:|---|---|---|
| 22 | `SCLK` | Hardware `SCK` | Library default is `SPI_MODE3`, MSB first, 4 MHz. |
| 23 | `DIN` | Hardware `MOSI` | Data into ADS1258. |
| 24 | `DOUT` | Hardware `MISO` | Data out of ADS1258. |
| 27 | `CS` | `CS_PIN`, default Arduino D10 | Active low chip select. |
| 25 | `DRDY` | `DRDY_PIN`, default Arduino D2 | Active low; use an interrupt-capable pin if possible. |
| 26 | `START` | `START_PIN`, default Arduino D9 | Active high. Code drives it low while configuring, then high to convert. |
| 11 | `RESET` | `RESET_PIN`, default Arduino D8, or tie high | Active low reset. |
| 10 | `PWDN` | `PWDN_PIN`, default Arduino D7, or tie high | Active low power-down. |
| 29 | `DGND` | MCU ground | Digital ground. Must share ground with MCU. |
| 28 | `DVDD` | MCU logic supply, 2.7 V to 5.25 V | Match MCU logic level, or use level shifting. |

Typical Arduino Uno hardware SPI pins:

| Arduino Uno | SPI function |
|---|---|
| D13 | SCK |
| D12 | MISO |
| D11 | MOSI |
| D10 | CS, as used by the example |

Typical ESP32 note: set the SPI pins for your board in the sketch before using the ADC if you are not using the default ESP32 SPI pins.

## Power connections

| ADS1258 pin | Signal | Connect / design note |
|---:|---|---|
| 6 | `AVDD` | `+5 V` for single-supply operation, or `+2.5 V` for bipolar operation. |
| 5 | `AVSS` | `AGND` for single-supply operation, or `-2.5 V` for bipolar operation. The exposed thermal pad is internally connected to `AVSS`; connect it to `AVSS`. |
| 28 | `DVDD` | Digital supply: 2.7 V to 5.25 V. Use the same logic level as the MCU, or level shift digital inputs. |
| 29 | `DGND` | Digital ground. Tie to the board ground with a low-impedance ground scheme. |
| 31 | `VREFP` | Positive reference input. Use a stable low-noise reference. |
| 30 | `VREFN` | Negative reference input. Often `AGND` in single-supply systems. |
| 32 | `AINCOM` | Common node for single-ended inputs. Often `AGND` in simple unipolar tests. |
| 7 | `PLLCAP` | 22 nF capacitor to `AVSS` when using the crystal oscillator. |
| 8, 9 | `XTAL1`, `XTAL2` | 32.768 kHz crystal and load capacitors when using the internal PLL clock. |
| 12 | `CLKSEL` | Low: use crystal oscillator. High: provide external clock on `CLKIO`. |
| 13 | `CLKIO` | Clock input/output depending on `CLKSEL`. |

Decoupling:

- Put a 0.1 uF ceramic capacitor close to each supply pin.
- TI recommends a 10 uF capacitor plus 0.1 uF ceramic near the supply pins.
- Put reference decoupling close to `VREFP` and `VREFN` such as 10 uF plus 0.1 uF.
- Keep analog/reference wiring quiet. Avoid sharing the analog supply with relays, motors, LED strips, or noisy switching loads.

## Analog input notes

For single-ended readings, the ADS1258 measures:

```text
AINx - AINCOM
```

For a first smoke test:

1. Use single-supply mode: `AVDD = +5 V`, `AVSS = AGND`.
2. Connect `AINCOM` to `AGND`.
3. Connect `AIN0` to a known safe voltage such as 0 V, 1.25 V, or 2.5 V.
4. Set `VREF_VOLTS` in the sketch to your actual reference voltage.
5. Leave the sketch's `CHANNEL_MASK` at `0x0001` for AIN0 only.

Do not let analog inputs go outside the analog rails. TI specifies analog pins should stay within about 100 mV beyond `AVSS` and `AVDD`.

The library uses the internal multiplexer-to-ADC path by default (`BYPAS = 0`). If you add external filtering or an amplifier between `MUXOUTP/MUXOUTN` and `ADCINP/ADCINN`, you must set `BYPAS = 1` yourself in `CONFIG0`.

## Output scaling

ADS1258 channel data is 24-bit two's-complement. The voltage conversion used here follows the datasheet scaling:

```text
1 LSB = VREF / 0x780000
volts = raw_code * VREF / 7864320
```

If `VREF_VOLTS = 2.500`, then a raw code near `0x780000` corresponds to approximately `+2.5 V` differential input.

## Data-ready and status byte

The example waits for `DRDY` to go low, then issues the Channel Data Read Command (`0x30`). That command returns:

```text
status byte + 3 data bytes
```

The status byte includes:

- `NEW`: new conversion data is available.
- `OVF`: input over-range.
- `SUPPLY`: analog supply below the monitor threshold.
- `CHID[4:0]`: channel ID. In auto-scan mode, single-ended AIN0 starts at channel ID `0x08`.

## Common problems

- **ID reads `0x00` or `0xFF`:** SPI wiring, chip-select wiring, power, clock, or reset/power-down pin is wrong.
- **Timeout waiting for DRDY:** no ADC clock, `START` not high, `PWDN` low, `RESET` low, or DRDY wired to the wrong pin.
- **All voltages float/random:** the selected analog input or `AINCOM` is floating.
- **Wrong voltage scale:** update `VREF_VOLTS` to match the actual `VREFP - VREFN`.
- **Using 3.3 V ADS1258 digital supply with a 5 V Arduino:** use level shifting on MCU outputs into the ADS1258.

