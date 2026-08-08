#!/usr/bin/env python3
"""Run ngspice READ-vs-RESET DC decoder/control comparison.

This is a simulation-only reference bench for the DC line-control equations
shown in Neuromorphic_X2_RTL_detailed_block_diagram_v2.  It does not use
firmware or hardware.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CIRCUIT = HERE / "read_reset_decoder_dc_compare.cir"
LOG = HERE / "read_reset_decoder_dc_compare.log"

EXPECTED = {
    # READ: same selected row/column decode, sensing/subtractor mode.
    "read_wl_addr_sel": 1.0,
    "read_wl_data_sel": 1.0,
    "read_wl_float_sel": 0.0,
    "read_sl_addr_sel": 1.0,
    "read_sl_data_sel": 1.0,
    "read_sl_float_sel": 0.0,
    "read_bl_addr_sel": 1.0,
    "read_bl_data_sel": 0.0,
    "read_bl_float_sel": 1.0,
    "read_unselected_float": 1.0,
    "read_subtractor_en": 1.0,
    "read_tdc_start": 1.0,
    "read_response_valid": 1.0,

    # RESET: same selected row/column decode, main program/reset drive mode.
    "reset_wl_addr_sel": 1.0,
    "reset_wl_data_sel": 1.0,
    "reset_wl_float_sel": 0.0,
    "reset_sl_addr_sel": 1.0,
    "reset_sl_data_sel": 1.0,
    "reset_sl_float_sel": 0.0,
    "reset_bl_addr_sel": 1.0,
    "reset_bl_data_sel": 0.0,
    "reset_bl_float_sel": 0.0,
    "reset_unselected_float": 1.0,
    "reset_subtractor_en": 0.0,
    "reset_tdc_start": 0.0,
    "reset_response_valid": 0.0,
}

# Pairs that should be identical because READ and RESET use the same selected
# row/column decoder result and the same unselected-line float convention.
SAME_PAIRS = [
    ("read_wl_addr_sel", "reset_wl_addr_sel"),
    ("read_wl_data_sel", "reset_wl_data_sel"),
    ("read_wl_float_sel", "reset_wl_float_sel"),
    ("read_sl_addr_sel", "reset_sl_addr_sel"),
    ("read_sl_data_sel", "reset_sl_data_sel"),
    ("read_sl_float_sel", "reset_sl_float_sel"),
    ("read_bl_addr_sel", "reset_bl_addr_sel"),
    ("read_bl_data_sel", "reset_bl_data_sel"),
    ("read_unselected_float", "reset_unselected_float"),
]

# Pairs that intentionally differ between read sensing and main reset drive.
DIFFERENT_PAIRS = [
    ("read_bl_float_sel", "reset_bl_float_sel"),
    ("read_subtractor_en", "reset_subtractor_en"),
    ("read_tdc_start", "reset_tdc_start"),
    ("read_response_valid", "reset_response_valid"),
]


def parse_meas(log_text: str) -> dict[str, float]:
    values: dict[str, float] = {}

    # Preferred path if a future netlist uses .meas output.
    for name, value in re.findall(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([-+0-9.eE]+)", log_text, flags=re.M):
        if name in EXPECTED:
            values[name] = float(value)

    # Current ngspice-45 operating-point output prints a node-voltage table:
    #     node_name     1.000000e+00
    for name, value in re.findall(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s+([-+0-9.eE]+)\s*$", log_text, flags=re.M):
        if name in EXPECTED:
            values[name] = float(value)

    return values


def main() -> int:
    if not CIRCUIT.exists():
        print(f"ERROR: circuit not found: {CIRCUIT}", file=sys.stderr)
        return 2

    result = subprocess.run(
        ["ngspice", "-b", "-o", str(LOG), str(CIRCUIT)],
        cwd=HERE,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    print(result.stdout, end="")
    if result.returncode != 0:
        print(f"ERROR: ngspice exited with {result.returncode}; see {LOG}", file=sys.stderr)
        return result.returncode

    log_text = LOG.read_text(errors="replace")
    measured = parse_meas(log_text)
    missing = sorted(set(EXPECTED) - set(measured))
    if missing:
        print("ERROR: missing .meas values:", ", ".join(missing), file=sys.stderr)
        print(log_text[-2000:], file=sys.stderr)
        return 3

    tol = 1e-6
    failures: list[str] = []
    for name, expected in EXPECTED.items():
        actual = measured[name]
        if abs(actual - expected) > tol:
            failures.append(f"{name}: expected {expected}, got {actual}")

    for a, b in SAME_PAIRS:
        if abs(measured[a] - measured[b]) > tol:
            failures.append(f"expected same: {a}={measured[a]} {b}={measured[b]}")

    for a, b in DIFFERENT_PAIRS:
        if abs(measured[a] - measured[b]) <= tol:
            failures.append(f"expected different: {a}={measured[a]} {b}={measured[b]}")

    print("\nREAD vs RESET DC ngspice measured values:")
    for name in sorted(EXPECTED):
        print(f"  {name:28s} = {measured[name]:.6g}")

    if failures:
        print("\nFAIL:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print("\nPASS: ngspice READ-vs-RESET DC decoder/control comparison")
    print("  Same: selected WL/SL/BL address decode and unselected float convention")
    print("  Different: READ floats/senses selected BL and enables subtractor/TDC/response")
    print("             RESET main pulse drives selected BL low and has no subtractor/TDC/response")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
