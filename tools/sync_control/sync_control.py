#!/usr/bin/env python3
"""
sync_control.py
================

Host-side front end for the GPIO-synchronized READ/SET/RESET run
implemented by:

  - Caravel firmware: Firmware_wishbone/synced_mode_wb/synced_mode_wb.c
  - Teensy sketch:     Arduino_Code_DAC_ADC_SCANPINS/DAC_analog_vltgs/
                        (main .ino + sync_slave.ino, "SYNC" command)

Caravel drives the actual READ1 -> SET -> READ2 -> RESET -> READ3
sequence over the GPIO handshake by itself once it is flashed and
reset; this script does not control Caravel's timing at all. Its job
is to:

  1. Open the Teensy's USB-serial port and send "SYNC", which arms
     the Teensy slave loop (see sync_slave.ino / runSyncSlaveSequence).
  2. Open the Caravel debug-UART port (an FTDI "Single RS232-HS"
     interface, same one used by the existing client.py monitors)
     and read its print() output.
  3. Merge both streams with host-side timestamps into one readable
     log, and mirror the same lines into a CSV for the MONITOR_SAMPLE
     / SYNC_* lines coming from the Teensy.
  4. Stop when the Teensy reports SYNC_SEQUENCE_DONE / _ABORTED, on
     Ctrl-C, or after --timeout seconds.

Requires: pyserial (both ports), pyftdi (only if --caravel-port is
left to auto-detect via FTDI; not needed if you pass a plain serial
device path for --caravel-port).

Usage
-----
    python sync_control.py --teensy-port /dev/ttyACM0
    python sync_control.py --teensy-port COM5 --caravel-port COM7
    python sync_control.py --teensy-port /dev/ttyACM0 --log run1.log --csv run1.csv
"""

import argparse
import csv
import json
import queue
import re
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Missing dependency: pip install pyserial", file=sys.stderr)
    raise


DEFAULT_DONE_MARKERS = ("SYNC_SEQUENCE_DONE", "SYNC_SEQUENCE_ABORTED")

# Matches the MONITOR_SAMPLE lines already emitted by
# printMonitorSample() in the Teensy sketch, e.g.:
#   MONITOR_SAMPLE packet=READ t_us=1234 read_uA=1.2345 set_uA=0.0000 reset_uA=0.0000
MONITOR_SAMPLE_RE = re.compile(
    r"MONITOR_SAMPLE packet=(?P<packet>\S+) t_us=(?P<t_us>\d+) "
    r"read_uA=(?P<read_uA>-?\d+\.\d+) set_uA=(?P<set_uA>-?\d+\.\d+) "
    r"reset_uA=(?P<reset_uA>-?\d+\.\d+)"
)

SYNC_FINAL_SAMPLE_RE = re.compile(
    r"SYNC_FINAL_SAMPLE (?:packet=(?P<packet>\S+) mode=\S+|mode=(?P<mode_only>\S+)) "
    r"read_uA=(?P<read_uA>-?\d+\.\d+) set_uA=(?P<set_uA>-?\d+\.\d+) "
    r"reset_uA=(?P<reset_uA>-?\d+\.\d+)"
)


def find_teensy_port():
    """Best-effort auto-detect: first USB-serial port whose
    description mentions Teensy/USB Serial."""
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        if "teensy" in desc or "usb serial" in desc:
            return p.device
    return None


def find_caravel_ftdi_port():
    """Auto-detect the FTDI 'Single RS232-HS' interface used for the
    Caravel debug UART, mirroring the device-selection logic in the
    existing client.py monitor scripts. Falls back to None if pyftdi
    isn't installed or no matching device is found."""
    try:
        import pyftdi.serialext
        from pyftdi.ftdi import Ftdi
        from io import StringIO
    except ImportError:
        return None

    buf = StringIO()
    Ftdi.show_devices(out=buf)
    devlist = buf.getvalue().splitlines()[1:-1]
    matches = []
    for dev in devlist:
        url = dev.split("(")[0].strip()
        name = "(" + dev.split("(")[1]
        if name == "(Single RS232-HS)":
            matches.append(url)

    if len(matches) == 1:
        return matches[0]
    return None


def open_caravel_port(port_arg, baudrate):
    """Open the Caravel UART. If port_arg looks like a normal serial
    device path, use pyserial directly. Otherwise (or if not given),
    try the FTDI auto-detect used by client.py."""
    if port_arg and not port_arg.startswith("ftdi://"):
        return serial.Serial(port_arg, baudrate=baudrate, timeout=0.1)

    if port_arg and port_arg.startswith("ftdi://"):
        import pyftdi.serialext
        return pyftdi.serialext.serial_for_url(port_arg, baudrate=baudrate, timeout=0.1)

    url = find_caravel_ftdi_port()
    if url is None:
        print(
            "Could not auto-detect the Caravel FTDI UART. Pass it explicitly "
            "with --caravel-port (either a serial device path or an ftdi:// URL "
            "from `python -m pyftdi.list`).",
            file=sys.stderr,
        )
        return None

    import pyftdi.serialext
    print(f"Caravel UART auto-detected at {url}")
    return pyftdi.serialext.serial_for_url(url, baudrate=baudrate, timeout=0.1)


def sample_from_match(match, timestamp, source):
    packet = match.groupdict().get("packet") or match.groupdict().get("mode_only")
    return {
        "host_time": timestamp,
        "source": source,
        "packet": packet,
        "t_us": int(match.groupdict().get("t_us") or 0),
        "read_uA": float(match.group("read_uA")),
        "set_uA": float(match.group("set_uA")),
        "reset_uA": float(match.group("reset_uA")),
    }


def write_summary(samples, summary_path):
    packets = []
    for sample in samples:
        packet = sample["packet"]
        if not packets or packets[-1]["packet"] != packet:
            packets.append({"packet": packet, "samples": []})
        packets[-1]["samples"].append(sample)

    rows = []
    for group in packets:
        values = group["samples"]
        first = values[0]
        last = values[-1]
        row = {
            "packet": group["packet"],
            "samples": len(values),
            "read_base_uA": first["read_uA"],
            "read_end_uA": last["read_uA"],
            "read_end_delta_uA": last["read_uA"] - first["read_uA"],
            "read_span_uA": max(v["read_uA"] for v in values) - min(v["read_uA"] for v in values),
            "read_peak_delta_uA": max(abs(v["read_uA"] - first["read_uA"]) for v in values),
            "set_base_uA": first["set_uA"],
            "set_end_uA": last["set_uA"],
            "set_end_delta_uA": last["set_uA"] - first["set_uA"],
            "set_span_uA": max(v["set_uA"] for v in values) - min(v["set_uA"] for v in values),
            "set_peak_delta_uA": max(abs(v["set_uA"] - first["set_uA"]) for v in values),
            "reset_base_uA": first["reset_uA"],
            "reset_end_uA": last["reset_uA"],
            "reset_end_delta_uA": last["reset_uA"] - first["reset_uA"],
            "reset_span_uA": max(v["reset_uA"] for v in values) - min(v["reset_uA"] for v in values),
            "reset_peak_delta_uA": max(abs(v["reset_uA"] - first["reset_uA"]) for v in values),
        }
        rows.append(row)

    summary_path = Path(summary_path)
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    if summary_path.suffix.lower() == ".json":
        summary_path.write_text(json.dumps({"packet_summary": rows}, indent=2) + "\n")
    else:
        fieldnames = [
            "packet", "samples",
            "read_base_uA", "read_end_uA", "read_end_delta_uA", "read_span_uA", "read_peak_delta_uA",
            "set_base_uA", "set_end_uA", "set_end_delta_uA", "set_span_uA", "set_peak_delta_uA",
            "reset_base_uA", "reset_end_uA", "reset_end_delta_uA", "reset_span_uA", "reset_peak_delta_uA",
        ]
        with summary_path.open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
    return rows


class LineReader(threading.Thread):
    """Reads a serial port line-by-line in the background and pushes
    (timestamp, source_tag, line) tuples onto a shared queue."""

    def __init__(self, ser, tag, out_queue):
        super().__init__(daemon=True)
        self.ser = ser
        self.tag = tag
        self.out_queue = out_queue
        self._stop = threading.Event()
        self._buf = b""

    def run(self):
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(256)
            except Exception as exc:
                self.out_queue.put((time.time(), self.tag, f"<port error: {exc}>"))
                return
            if not chunk:
                continue
            self._buf += chunk
            while b"\n" in self._buf:
                line, self._buf = self._buf.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                if text:
                    self.out_queue.put((time.time(), self.tag, text))

    def stop(self):
        self._stop.set()


def run(args):
    teensy_port = args.teensy_port or find_teensy_port()
    if teensy_port is None:
        print(
            "Could not auto-detect the Teensy port. Pass it explicitly with --teensy-port.",
            file=sys.stderr,
        )
        return 1

    print(f"Teensy port: {teensy_port}")
    teensy = serial.Serial(teensy_port, baudrate=args.teensy_baud, timeout=0.1)

    caravel = None
    if not args.no_caravel:
        caravel = open_caravel_port(args.caravel_port, args.caravel_baud)
        if caravel is not None:
            print("Caravel UART: connected")
        else:
            print("Continuing without the Caravel UART stream (Teensy log only).")

    out_q = queue.Queue()
    readers = [LineReader(teensy, "TEENSY", out_q)]
    if caravel is not None:
        readers.append(LineReader(caravel, "CARAVEL", out_q))
    for r in readers:
        r.start()

    log_fh = open(args.log, "a") if args.log else None
    csv_fh = open(args.csv, "a", newline="") if args.csv else None
    csv_writer = None
    samples = []
    if csv_fh is not None:
        csv_writer = csv.writer(csv_fh)
        if csv_fh.tell() == 0:
            csv_writer.writerow(["timestamp", "packet", "t_us", "read_uA", "set_uA", "reset_uA"])

    def emit(ts, tag, text):
        stamp = datetime.fromtimestamp(ts).strftime("%H:%M:%S.%f")[:-3]
        line = f"[{stamp}][{tag}] {text}"
        is_sample = MONITOR_SAMPLE_RE.search(text) is not None
        if not (args.quiet_samples and is_sample):
            print(line)
        if log_fh:
            log_fh.write(line + "\n")
            log_fh.flush()
        if csv_writer:
            m = MONITOR_SAMPLE_RE.search(text)
            if m:
                samples.append(sample_from_match(m, ts, tag))
                csv_writer.writerow([
                    stamp, m.group("packet"), m.group("t_us"),
                    m.group("read_uA"), m.group("set_uA"), m.group("reset_uA"),
                ])
                csv_fh.flush()
        elif tag == "TEENSY":
            m = MONITOR_SAMPLE_RE.search(text)
            if m:
                samples.append(sample_from_match(m, ts, tag))

        if tag == "TEENSY":
            m = SYNC_FINAL_SAMPLE_RE.search(text)
            if m:
                samples.append(sample_from_match(m, ts, tag))

    done_markers = tuple(args.done_marker) if args.done_marker else DEFAULT_DONE_MARKERS

    print(f"Sending Teensy command: {args.command}")
    teensy.write((args.command + "\n").encode())

    start = time.time()
    done = False
    try:
        while True:
            try:
                ts, tag, text = out_q.get(timeout=0.2)
                emit(ts, tag, text)
                if tag == "TEENSY" and any(marker in text for marker in done_markers):
                    done = True
                    drain_until = time.time() + args.post_done_drain
                    while args.post_done_drain and time.time() < drain_until:
                        try:
                            drain_ts, drain_tag, drain_text = out_q.get(timeout=0.1)
                            emit(drain_ts, drain_tag, drain_text)
                        except queue.Empty:
                            pass
                    break
            except queue.Empty:
                pass

            if args.timeout and (time.time() - start) > args.timeout:
                print(f"Timed out after {args.timeout}s without a completion marker.")
                break
    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    finally:
        for r in readers:
            r.stop()
        teensy.close()
        if caravel is not None:
            caravel.close()
        if log_fh:
            log_fh.close()
        if csv_fh:
            csv_fh.close()

    if args.summary and samples:
        rows = write_summary(samples, args.summary)
        print(f"Wrote packet summary: {args.summary}")
        for row in rows:
            print(
                "SUMMARY "
                f"packet={row['packet']} samples={row['samples']} "
                f"read_peak_delta_uA={row['read_peak_delta_uA']:.4f} "
                f"set_peak_delta_uA={row['set_peak_delta_uA']:.4f} "
                f"reset_peak_delta_uA={row['reset_peak_delta_uA']:.4f}"
            )

    return 0 if done else 1


def build_parser():
    p = argparse.ArgumentParser(description="Arm and log a synced READ/SET/RESET run.")
    p.add_argument("--teensy-port", help="Teensy USB-serial device (auto-detected if omitted)")
    p.add_argument("--teensy-baud", type=int, default=115200)
    p.add_argument("--caravel-port", help="Caravel UART device path, or ftdi:// URL; auto-detected if omitted")
    p.add_argument("--caravel-baud", type=int, default=9600)
    p.add_argument("--no-caravel", action="store_true", help="Skip the Caravel UART stream entirely")
    p.add_argument("--log", help="Append the merged, timestamped log to this file")
    p.add_argument("--csv", help="Append parsed MONITOR_SAMPLE rows to this CSV")
    p.add_argument("--summary", help="Write packet peak-delta summary as .csv or .json")
    p.add_argument("--command", default="SYNC", help="Command to send to the Teensy, default: SYNC")
    p.add_argument("--done-marker", action="append", help="Completion marker; repeat for multiple markers")
    p.add_argument("--quiet-samples", action="store_true", help="Do not echo MONITOR_SAMPLE rows to stdout")
    p.add_argument("--post-done-drain", type=float, default=0.0, help="Keep reading ports for this many seconds after Teensy completion")
    p.add_argument("--timeout", type=float, default=60.0, help="Give up after this many seconds (0 = no timeout)")
    return p


if __name__ == "__main__":
    sys.exit(run(build_parser().parse_args()))
