#!/usr/bin/env python3
"""Run WL rail toggle shunt-current/leakage ngspice DC bench.

Simulation-only checker for the proposed hardware experiment:
  toggle VCC_WL_SET, VCC_WL_RESET, VCC_WL_READ and read shunt values.

The circuit is a compact DC reference model for mux/decoder/subtractor intent:
  mux: inactive rails have ~0 shunt current;
  decoder: selected-cell branch dominates aggregate unselected leakage;
  subtractor/read: read selected current minus leakage is positive.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
CIRCUIT = HERE / "wl_rail_toggle_shunt_leakage.cir"
LOG = HERE / "wl_rail_toggle_shunt_leakage.log"
RSHUNT = 100.0

RAILS = {
    "set": {
        "vcc_node": "vcc_wl_set",
        "wl_node": "wl_set",
        "vcc": 1.8,
        "r_selected": 20e3,
        "r_leak": 5e6,
        "off_vcc_node": "vcc_wl_set_off",
        "off_wl_node": "wl_set_off",
    },
    "reset": {
        "vcc_node": "vcc_wl_reset",
        "wl_node": "wl_reset",
        "vcc": 1.2,
        "r_selected": 30e3,
        "r_leak": 5e6,
        "off_vcc_node": "vcc_wl_reset_off",
        "off_wl_node": "wl_reset_off",
    },
    "read": {
        "vcc_node": "vcc_wl_read",
        "wl_node": "wl_read",
        "vcc": 0.4,
        "r_selected": 200e3,
        "r_leak": 5e6,
        "off_vcc_node": "vcc_wl_read_off",
        "off_wl_node": "wl_read_off",
    },
}


def parse_op_voltages(log_text: str) -> dict[str, float]:
    values: dict[str, float] = {}
    for name, value in re.findall(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s+([-+0-9.eE]+)\s*$", log_text, flags=re.M):
        values[name] = float(value)
    return values


def main() -> int:
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
    v = parse_op_voltages(log_text)

    required = []
    for cfg in RAILS.values():
        required += [cfg["vcc_node"], cfg["wl_node"], cfg["off_vcc_node"], cfg["off_wl_node"]]
    missing = [name for name in required if name not in v]
    if missing:
        print("ERROR: missing operating-point nodes:", ", ".join(missing), file=sys.stderr)
        print(log_text[-2000:], file=sys.stderr)
        return 2

    rows = {}
    failures: list[str] = []
    for name, cfg in RAILS.items():
        vcc = v[cfg["vcc_node"]]
        wl = v[cfg["wl_node"]]
        i_shunt = (vcc - wl) / RSHUNT
        i_selected = wl / cfg["r_selected"]
        i_leak = wl / cfg["r_leak"]
        v_shunt = vcc - wl
        selected_minus_leak = i_shunt - i_leak

        off_i = (v[cfg["off_vcc_node"]] - v[cfg["off_wl_node"]]) / RSHUNT

        rows[name] = {
            "vcc": vcc,
            "wl": wl,
            "v_shunt": v_shunt,
            "i_shunt": i_shunt,
            "i_selected": i_selected,
            "i_leak": i_leak,
            "selected_minus_leak": selected_minus_leak,
            "off_i": off_i,
        }

        # Decoder/current visibility: selected-cell branch should be clearly
        # above aggregate leakage for each mode.
        if i_selected / max(i_leak, 1e-30) < 20.0:
            failures.append(f"{name}: selected/leakage ratio too small: {i_selected/i_leak:.2f}")

        # Mux sanity: inactive rail current is effectively zero.
        if abs(off_i) > 1e-12:
            failures.append(f"{name}: inactive rail current not ~0: {off_i}")

        # Shunt must see selected+leakage current.
        if abs(i_shunt - (i_selected + i_leak)) > 5e-9:
            failures.append(f"{name}: shunt current mismatch")

    # Expected rail-current order for this reference sizing.
    if not (rows["set"]["i_shunt"] > rows["reset"]["i_shunt"] > rows["read"]["i_shunt"] > 0):
        failures.append("rail current order expected set > reset > read > 0")

    # Read subtractor equivalent: selected-minus-leak reconstructs selected branch.
    read = rows["read"]
    if abs(read["selected_minus_leak"] - read["i_selected"]) > 5e-9:
        failures.append("read subtractor equivalent does not reconstruct selected read-cell current")
    if read["selected_minus_leak"] <= 0:
        failures.append("read selected-minus-leak should be positive")

    print("\nWL rail toggle shunt/leakage ngspice results:")
    print("mode     vcc(V)   wl(V)    vshunt(mV)  ishunt(uA)  selected(uA)  leakage(uA)  sel/leak  offrail(pA)")
    for name in ("set", "reset", "read"):
        r = rows[name]
        ratio = r["i_selected"] / r["i_leak"]
        print(
            f"{name:5s}  {r['vcc']:7.4f}  {r['wl']:7.4f}"
            f"  {1e3*r['v_shunt']:10.4f}  {1e6*r['i_shunt']:10.4f}"
            f"  {1e6*r['i_selected']:12.4f}  {1e6*r['i_leak']:11.4f}"
            f"  {ratio:8.1f}  {1e12*r['off_i']:10.4f}"
        )

    print(
        f"\nRead subtractor equivalent: ishunt - ileak = "
        f"{1e6*rows['read']['selected_minus_leak']:.4f} uA"
    )

    if failures:
        print("\nFAIL:")
        for f in failures:
            print(f"  - {f}")
        return 1

    print("\nPASS: WL rail toggle shunt/leakage DC bench")
    print("  mux: inactive WL rails are ~0 current")
    print("  decoder: selected-cell branch dominates aggregate leakage")
    print("  subtractor/read: read selected current remains after leakage subtraction")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
