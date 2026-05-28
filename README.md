# RISC-V Firmware Remote Flash Notes

This repository holds chip firmware and helper scripts for the Caravel board
connected to the remote Ubuntu PC over Tailscale.

## Remote Target

- Remote PC hostname: `ubuntu-24`
- Remote Tailscale IP: `100.98.132.51`
- Remote SSH user: `ubuntu-24-04`
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

## Current Firmware

The active local firmware source is:

```text
cocotb_scan_debug_firmware.c
```

It is a chip-side version of the cocotb `ram_word` scan test. The firmware:

- drives scan GPIOs from the management CPU,
- prints checkpoints over UART with the prefix `[COCOTB-SCAN]`,
- pulses the management GPIO LED at checkpoints and scan bits,
- executes the same scan transaction data as the cocotb test: `0x8000` then
  `0x8822`,
- keeps Wishbone operations as non-blocking placeholders because WB is not
  working on the currently connected chip.

The firmware does not touch user WB address `0x30000004` unless compiled with:

```c
#define ENABLE_WB_TOUCHES 1
```

By default it prints the intended WB actions and continues execution.

## GPIO Mapping

The cocotb scan test uses these Caravel GPIOs:

| GPIO | Signal |
| --- | --- |
| `21` | `ScanInDR` |
| `22` | `ScanInDL` |
| `35` | `ScanInCC`, held low by firmware |
| `36` | `TM` |

The firmware follows the executable cocotb source: `ScanInDR` is driven low
during idle, shift, after-shift, and final idle. If the intended protocol later
requires a high done/idle value, override `SCAN_DR_IDLE_VALUE`,
`SCAN_DR_SHIFT_VALUE`, or `SCAN_DR_DONE_VALUE`.

## Hardware Jumper Rule

The FTDI path is multiplexed between housekeeping SPI programming and UART.

- Remove `J2` before flashing.
- Put `J2` back after flashing to see UART logs.

If `J2` is installed while flashing, `caravel_hkflash.py` can read invalid
Caravel IDs such as `ffff` or `0000`. A good flash setup reads:

```text
mfg        = 0456
product    = 11
project ID = 23097d48
project ID = 12be90c4
```

## Build On The Remote PC

Copy the local firmware into the remote build directory and build:

```bash
scp cocotb_scan_debug_firmware.c ubuntu-24-04@100.98.132.51:/tmp/cocotb_scan_debug_firmware.c

ssh ubuntu-24-04@100.98.132.51 '
  cd ~/caravel_board/firmware/chipignite/scan_debug &&
  cp scan_debug.c scan_debug.c.backup_$(date +%Y%m%d_%H%M%S) &&
  cp /tmp/cocotb_scan_debug_firmware.c scan_debug.c &&
  make clean hex
'
```

The last successful build installed the local firmware as remote
`scan_debug.c` and produced `scan_debug.hex`.

## Flash From This Laptop

The helper script flashes the existing remote `scan_debug.hex`:

```bash
./flash_remote_caravel.sh
```

To copy a local `.hex` to the remote PC as `scan_debug.hex` and then flash it:

```bash
./flash_remote_caravel.sh path/to/your_firmware.hex
```

If the USB device needs remote sudo:

```bash
REMOTE_TTY=1 ./flash_remote_caravel.sh
```

If more than one FTDI device is attached:

```bash
USB_BUSDEV=002/003 ./flash_remote_caravel.sh
```

The exact stock remote command sequence is:

```bash
cd ~/caravel_board/firmware/chipignite/scan_debug

ls -l scan_debug.hex

BUSDEV=$(lsusb -d 0403:6014 | awk '{print $2"/"substr($4,1,3)}')
echo "$BUSDEV"
sudo chmod a+rw "/dev/bus/usb/$BUSDEV"

~/caravel_venv/bin/python3 ../util/caravel_hkflash.py scan_debug.hex
```

## Last Known Flash Result

The latest flashed cocotb-style image verified successfully:

```text
scan_debug.hex size: 25110 bytes
FTDI path: ftdi://ftdi:232h:2:2/1
JEDEC: ef4016
total_bytes = 8192
all read compares successful through addr 0x1f00
pll_trim = b'ffefff03'
```

After flashing:

1. Put `J2` back for UART.
2. Press the Caravel board reset button.
3. Watch for `[COCOTB-SCAN]` UART checkpoints and LED pulses.

## Useful Checks

Check Tailscale reachability:

```bash
tailscale ping --timeout=5s 100.98.132.51
```

Check SSH:

```bash
ssh ubuntu-24-04@100.98.132.51 'hostname && whoami'
```

Check the remote FTDI device:

```bash
ssh ubuntu-24-04@100.98.132.51 'lsusb -d 0403:6014'
```
