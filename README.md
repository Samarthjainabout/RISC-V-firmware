# BM Labs X1 IP Debug Firmware

This repository contains RISC-V firmware for debugging the BM Labs X1 IP through
the Caravel management SoC.

The current chip image is a firmware-driven version of the cocotb scan test. It
drives the same scan pins and scan words that the cocotb testbench drives, while
printing progress checkpoints over UART and pulsing the management GPIO LED so
the run can be tracked visually on the board.

## Automated Debug Platform

![Automated debug platform](docs/automated-debug-platform.png)

The debug setup connects the same scan intent to two targets:

- Caravel + cocotb simulation for automated regression and waveform inspection.
- Actual chip over UART for silicon validation using firmware checkpoints.

The remote Ubuntu PC has the Caravel board attached over FTDI. This laptop talks
to that PC through Tailscale and SSH.

## Current State

As of May 28, 2026, the chip is flashed with the cocotb-style scan firmware
from `cocotb_scan_debug_firmware.c`.

- Flash/programming mode: remove `J2`.
- UART/logging mode: reinstall `J2`, then reset the board.
- Remote PC: `ubuntu-24-04@100.98.132.51`.
- Remote serial device:
  `/dev/serial/by-id/usb-FTDI_Single_RS232-HS-if00-port0`.
- Wishbone is intentionally skipped in firmware because the current chip WB
  path is not working.

## Current Firmware

Primary firmware:

- `cocotb_scan_debug_firmware.c`

This is the file to edit for the current chip flow. It was copied to the remote
Caravel firmware directory as:

```text
~/caravel_board/firmware/chipignite/scan_debug/scan_debug.c
```

The firmware implements the cocotb `ram_word` scan flow:

1. Configure management GPIO, UART, and project GPIOs.
2. Drive initial scan idle state.
3. Print/pulse a firmware-ready checkpoint.
4. Log simulation-only steps such as `release_csb`.
5. Log Wishbone operations as non-blocking placeholders.
6. Drive scan transaction `0x8000`, LSB first.
7. Wait between transactions.
8. Drive scan transaction `0x8822`, LSB first.
9. Print completion and enter an LED heartbeat loop.

Important: the current chip Wishbone path is not working. The firmware therefore
does not touch `0x30000004` by default. WB actions are printed as
`[COCOTB-SCAN][WB-PLACEHOLDER]` messages and execution continues.

To actually execute those WB accesses in a future chip/build, compile with:

```c
#define ENABLE_WB_TOUCHES 1
```

or pass `-DENABLE_WB_TOUCHES=1` in the Makefile.

## Legacy Firmware

Older reset-sequence firmware:

- `top_tb_scan_debug_reset_sequence_riscv.c`

That file is a firmware translation of `top_tb_scan_debug_reset_sequence.sv`.
Keep it for reference, but it is not the image currently flashed on the chip.

## GPIO Map

The current firmware uses this Caravel GPIO mapping:

| GPIO | Signal | Direction | Notes |
| --- | --- | --- | --- |
| GPIO21 | `ScanInDR` | Output | Driven like the cocotb source. Default is low throughout the transaction. |
| GPIO22 | `ScanInDL` | Output | Serial scan data, shifted LSB first. |
| GPIO35 | `ScanInCC` | Output | Held low for compatibility with the older scan-debug RTL path. |
| GPIO36 | `TM` | Output | High during scan transaction, low in idle. |

The cocotb source comments mention `ScanInDR` going high after completion, but
the executable cocotb code drives it low. The firmware follows the executable
cocotb behavior. If the hardware protocol needs a high done/idle value, override:

```c
SCAN_DR_IDLE_VALUE
SCAN_DR_SHIFT_VALUE
SCAN_DR_DONE_VALUE
```

## Remote Target

- Remote PC hostname: `ubuntu-24`
- Remote Tailscale IP: `100.98.132.51`
- SSH user: `ubuntu-24-04`
- Remote firmware directory:
  `~/caravel_board/firmware/chipignite/scan_debug`
- Remote flash image:
  `~/caravel_board/firmware/chipignite/scan_debug/scan_debug.hex`
- Flash utility:
  `~/caravel_board/firmware/chipignite/util/caravel_hkflash.py`
- Python environment:
  `~/caravel_venv/bin/python3`

Do not commit passwords or private keys. Enter the remote sudo password only
when SSH/sudo prompts for it.

## Hardware Jumper Rule

J2 multiplexes UART and the housekeeping SPI programming path.

- Remove `J2` before flashing.
- Reinstall `J2` after flashing if you want UART logs.

If `J2` is installed during flashing, the programmer can see the FTDI device but
Caravel ID reads can fail with values like:

```text
mfg        = ffff
product    = ff
project ID = 00000000
Incorrect MFG value, expected 0x0456.
```

Expected ID when the board is correctly connected for flashing:

```text
mfg        = 0456
product    = 11
project ID = 23097d48
project ID = 12be90c4
```

## Build On Remote PC

Copy the current local firmware to the remote build directory:

```bash
scp cocotb_scan_debug_firmware.c \
  ubuntu-24-04@100.98.132.51:/tmp/cocotb_scan_debug_firmware.c

ssh ubuntu-24-04@100.98.132.51 \
  'cd ~/caravel_board/firmware/chipignite/scan_debug &&
   cp scan_debug.c scan_debug.c.backup_$(date +%Y%m%d_%H%M%S) &&
   cp /tmp/cocotb_scan_debug_firmware.c scan_debug.c &&
   make clean hex'
```

The known-good build produced:

```text
scan_debug.hex
```

with size `25110` bytes on May 28, 2026.

## Flash From This Laptop

This repo includes a helper:

```bash
./flash_remote_caravel.sh
```

To flash the already-built remote `scan_debug.hex`:

```bash
REMOTE_TTY=1 ./flash_remote_caravel.sh
```

To copy a local `.hex` to the remote PC as `scan_debug.hex` and then flash:

```bash
REMOTE_TTY=1 ./flash_remote_caravel.sh path/to/your_firmware.hex
```

If more than one FTDI device is attached, select the bus/device manually:

```bash
USB_BUSDEV=002/002 REMOTE_TTY=1 ./flash_remote_caravel.sh
```

## Stock Remote Flash Commands

Known-good sequence on the remote PC:

```bash
cd ~/caravel_board/firmware/chipignite/scan_debug

ls -l scan_debug.hex

BUSDEV=$(lsusb -d 0403:6014 | awk '{print $2"/"substr($4,1,3)}')
echo "$BUSDEV"
sudo chmod a+rw "/dev/bus/usb/$BUSDEV"

~/caravel_venv/bin/python3 ../util/caravel_hkflash.py scan_debug.hex
```

Known-good one-command version from this laptop:

```bash
ssh -tt ubuntu-24-04@100.98.132.51 \
  'cd ~/caravel_board/firmware/chipignite/scan_debug &&
   ls -l scan_debug.hex &&
   BUSDEV=$(lsusb -d 0403:6014 | awk '\''{print $2"/"substr($4,1,3)}'\'') &&
   echo "$BUSDEV" &&
   sudo chmod a+rw "/dev/bus/usb/$BUSDEV" &&
   ~/caravel_venv/bin/python3 ../util/caravel_hkflash.py scan_debug.hex'
```

## Latest Flash Result

The cocotb-style firmware was built and flashed successfully on May 28, 2026.

Flash summary:

```text
Success: Found one matching FTDI device at ftdi://ftdi:232h:2:2/1
Caravel data:
   mfg        = 0456
   product    = 11
   project ID = 23097d48
   project ID = 12be90c4

JEDEC = ef4016
total_bytes = 8192
addr 0x0 ... addr 0x1f00: read compare successful
pll_trim = b'ffefff03'
```

Previous remote source backup:

```text
~/caravel_board/firmware/chipignite/scan_debug/scan_debug.c.backup_20260528_123418
```

## Expected UART Output

After flashing:

1. Reinstall `J2` for UART.
2. Press the board reset button.
3. Watch UART output from the remote PC.

Use the remote venv's pyserial:

```bash
ssh -tt ubuntu-24-04@100.98.132.51 \
  '~/caravel_venv/bin/python3 -m serial.tools.miniterm \
   /dev/serial/by-id/usb-FTDI_Single_RS232-HS-if00-port0 9600'
```

Quit `miniterm` with `Ctrl+]`.

Expected log prefixes:

```text
[COCOTB-SCAN] firmware start: chip-side version of cocotb ram_word scan test
[COCOTB-SCAN][CP ...]
[COCOTB-SCAN][WB-PLACEHOLDER] ... SKIPPED_current_chip_wb_not_working
[COCOTB-SCAN][TXN] start scan_transaction_0 data=0x00008000
[COCOTB-SCAN][TXN] start scan_transaction_1 data=0x00008822
[COCOTB-SCAN][DONE] complete flow executed; entering LED heartbeat
```

## Debug Notes

- If flashing fails with invalid Caravel ID, remove `J2`, check board power, and
  retry.
- If flashing succeeds but there is no UART, reinstall `J2` and reset the board.
- If UART logs show WB placeholders, that is expected on the current chip.
- If scan timing needs adjustment, tune `SCAN_EDGE_DELAY`, `SCAN_IDLE_DELAY`,
  and `LED_PULSE_DELAY` in `cocotb_scan_debug_firmware.c`.
