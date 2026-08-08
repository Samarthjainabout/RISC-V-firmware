#!/usr/bin/env python3
"""ngspice WL rail toggle / shunt-current leakage bench.

Simulation-only reference model for the proposed experiment:
  toggle vcc_wl_set, vcc_wl_reset, vcc_wl_read one at a time and read shunt
  currents connected to those pins.

The model is intentionally simple and parameterized: each rail has a selected
cell path plus a background leakage path.  The checks verify that the selected
rail current is much larger than leakage-only current, and that non-toggled
rails remain near zero.
"""

from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEMPLATE = HERE / "wl_supply_toggle_leakage_template.cir"
OUTDIR = HERE / "wl_supply_toggle_runs"
OPEN_R = "1e12"

# Nominal DC reference values.  These are not claims about the chip; they are a
# compact SPICE reference for expected shunt-current relationships.
PARAMS_BASE = {
    "RSET_LEAK": "20e6",
    "RRESET_LEAK": "20e6",
    "RREAD_LEAK": "50e6",
}


@dataclass(frozen=True)
class Case:
    name: str
    vset: float
    vreset: float
    vread: float
    rset_selected: str = OPEN_R
    rreset_selected: str = OPEN_R
    rread_selected: str = OPEN_R


CASES = [
    Case("all_rails_off", 0.0, 0.0, 0.0),
    Case("vcc_wl_set_leakage_only", 1.8, 0.0, 0.0),
    Case("vcc_wl_set_selected_cell", 1.8, 0.0, 0.0, rset_selected="100e3"),
    Case("vcc_wl_reset_leakage_only", 0.0, 1.2, 0.0),
    Case("vcc_wl_reset_selected_cell", 0.0, 1.2, 0.0, rreset_selected="150e3"),
    Case("vcc_wl_read_leakage_only", 0.0, 0.0, 0.4),
    Case("vcc_wl_read_selected_cell", 0.0, 0.0, 0.4, rread_selected="1e6"),
]

SOURCE_TO_RAIL = {
    "vset_rail": "set",
    "vreset_rail": "reset",
    "vread_rail": "read",
}


def render(case: Case) -> str:
    s = TEMPLATE.read_text()
    replacements = {
        "VSET": f"{case.vset:g}",
        "VRESET": f"{case.vreset:g}",
        "VREAD": f"{case.vread:g}",
        "RSET_SELECTED": case.rset_selected,
        "RRESET_SELECTED": case.rreset_selected,
        "RREAD_SELECTED": case.rread_selected,
        **PARAMS_BASE,
    }
    # Replace longer keys first so RSET_SELECTED is not partially matched by VSET.
    for key in sorted(replacements, key=len, reverse=True):
        s = s.replace("{" + key + "}", replacements[key])
    return s


def parse_branch_currents(log_text: str) -> dict[str, float]:
    currents: dict[str, float] = {}
    for name, value in re.findall(r"^\s*([A-Za-z_][A-Za-z0-9_]*#branch)\s+([-+0-9.eE]+)\s*$", log_text, flags=re.M):
        src = name.replace("#branch", "").lower()
        if src in SOURCE_TO_RAIL:
            currents[SOURCE_TO_RAIL[src]] = abs(float(value))
    return currents


def run_case(case: Case) -> dict[str, float]:
    OUTDIR.mkdir(exist_ok=True)
    cir = OUTDIR / f"{case.name}.cir"
    log = OUTDIR / f"{case.name}.log"
    cir.write_text(render(case))
    result = subprocess.run(
        ["ngspice", "-b", "-o", str(log), str(cir)],
        cwd=HERE,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        print(result.stdout, end="")
        print(f"ERROR: ngspice failed for {case.name}; see {log}", file=sys.stderr)
        raise SystemExit(result.returncode)
    currents = parse_branch_currents(log.read_text(errors="replace"))
    missing = sorted(set(SOURCE_TO_RAIL.values()) - set(currents))
    if missing:
        print(log.read_text(errors="replace")[-2000:], file=sys.stderr)
        raise SystemExit(f"ERROR: missing branch currents for {case.name}: {missing}")
    return currents


def main() -> int:
    if not TEMPLATE.exists():
        print(f"ERROR: template missing: {TEMPLATE}", file=sys.stderr)
        return 2

    results = {case.name: run_case(case) for case in CASES}

    print("WL supply toggle ngspice shunt-current results:")
    print("  currents are absolute source currents in A")
    for name, currents in results.items():
        print(
            f"  {name:32s} "
            f"I_set={currents['set']:.6e} "
            f"I_reset={currents['reset']:.6e} "
            f"I_read={currents['read']:.6e}"
        )

    failures: list[str] = []
    off = results["all_rails_off"]
    for rail, current in off.items():
        if current > 1e-12:
            failures.append(f"{rail} rail off current too high: {current:.3e} A")

    checks = [
        ("set", "vcc_wl_set_leakage_only", "vcc_wl_set_selected_cell", 20.0),
        ("reset", "vcc_wl_reset_leakage_only", "vcc_wl_reset_selected_cell", 20.0),
        ("read", "vcc_wl_read_leakage_only", "vcc_wl_read_selected_cell", 20.0),
    ]
    for rail, leak_case, selected_case, min_ratio in checks:
        leak = results[leak_case][rail]
        selected = results[selected_case][rail]
        ratio = selected / leak if leak > 0 else float("inf")
        if ratio < min_ratio:
            failures.append(
                f"{rail} selected/leakage ratio too small: {ratio:.2f} "
                f"({selected:.3e}/{leak:.3e})"
            )

        # Non-toggled rails should remain effectively off for this simple model.
        for other in ("set", "reset", "read"):
            if other != rail and results[selected_case][other] > 1e-12:
                failures.append(
                    f"{selected_case}: non-toggled {other} current not zero: "
                    f"{results[selected_case][other]:.3e} A"
                )

    if failures:
        print("\nFAIL:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print("\nPASS: WL supply toggle selected-cell/leakage ngspice comparison")
    print("  selected-cell shunt current is >20x leakage-only for SET, RESET, READ")
    print("  non-toggled WL rails remain off in the DC reference model")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
