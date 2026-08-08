#!/usr/bin/env python3
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parent
DATA = ROOT / "data"


def finish(fig, name):
    fig.tight_layout()
    fig.savefig(ROOT / name, dpi=180, bbox_inches="tight")
    plt.close(fig)


def plot_read_shunt():
    data = pd.read_csv(DATA / "read_shunt_first500us.csv")
    baselines = {"READ1": 126.591, "READ2": 50.115, "READ3": 77.715}
    colors = {"READ1": "#1769aa", "READ2": "#bd4b00", "READ3": "#16825d"}
    fig, ax = plt.subplots(figsize=(9.2, 4.4))
    for packet, baseline in baselines.items():
        rows = data[(data.packet == packet) & (data.offset_s <= 50e-6)].copy()
        rows["delta_uA"] = rows.diff_v * 1000 - baseline
        rows["smooth_uA"] = rows.delta_uA.rolling(7, center=True, min_periods=1).mean()
        ax.plot(rows.offset_s * 1e6, rows.smooth_uA, label=packet, color=colors[packet], lw=1.3)
    ax.axvspan(0, 7.5, color="#f2c94c", alpha=0.23, label="7.5 us width reference")
    ax.axhline(0, color="#555555", lw=0.8)
    ax.set(xlabel="Time from GPIO23/D8 rising edge (us)", ylabel="A10-A9 change (uA)",
           title="Guarded READ-shunt operation edge, first 50 us")
    ax.grid(alpha=0.22)
    ax.legend(ncol=2, frameon=False)
    finish(fig, "guarded_read_shunt_first50us.png")


def plot_set_shunt():
    data = pd.read_csv(DATA / "set_shunt_first500us.csv")
    colors = {"FORM": "#6f42c1", "SET": "#bd4b00", "READ1": "#1769aa",
              "READ2": "#16825d", "RESET": "#c62828", "READ3": "#4f5b66"}
    fig, axes = plt.subplots(2, 1, figsize=(9.2, 6.4), sharex=True)
    for packet in ("FORM", "SET"):
        rows = data[data.packet == packet].copy()
        rows["uA"] = rows.diff_v * 1000
        rows["smooth_uA"] = rows.uA.rolling(15, center=True, min_periods=1).mean()
        axes[0].plot(rows.offset_s * 1e6, rows.smooth_uA, label=packet, color=colors[packet], lw=1.4)
    for packet in ("READ1", "READ2", "RESET", "READ3"):
        rows = data[data.packet == packet].copy()
        rows["uA"] = rows.diff_v * 1000
        rows["smooth_uA"] = rows.uA.rolling(15, center=True, min_periods=1).mean()
        axes[1].plot(rows.offset_s * 1e6, rows.smooth_uA, label=packet, color=colors[packet], lw=1.15)
    for ax in axes:
        ax.axvspan(0, 7.5, color="#f2c94c", alpha=0.23)
        ax.grid(alpha=0.22)
        ax.legend(frameon=False, ncol=4)
        ax.set_ylabel("A10-A9 current (uA)")
    axes[0].set_title("Guarded SET-shunt waveform, first 500 us")
    axes[1].set_xlabel("Time from GPIO23/D8 rising edge (us)")
    finish(fig, "guarded_set_shunt_first500us.png")


def plot_static_rails():
    runs = ("221606", "222104", "222141")
    fig, axes = plt.subplots(3, 1, figsize=(9.2, 8.0), sharex=True, sharey=True)
    for ax, run in zip(axes, runs):
        data = pd.read_csv(DATA / f"static_rails_{run}_edge_aligned.csv")
        data = data[(data.start_us >= -20) & (data.start_us <= 100)]
        for packet, color in (("FORM", "#6f42c1"), ("READ1", "#1769aa")):
            rows = data[data.packet == packet]
            ax.plot(rows.start_us, rows.delta_uA, color=color, label=packet, lw=1.1)
        ax.axvline(0, color="#222222", lw=0.9, ls="--")
        ax.set_ylabel("Delta (uA)")
        ax.set_title(f"Capture {run}", loc="left", fontsize=10)
        ax.grid(alpha=0.22)
        ax.legend(frameon=False, ncol=2, loc="upper right")
    axes[0].set_ylim(-12, 12)
    axes[-1].set_xlabel("Time from GPIO23/D8 rising edge (us)")
    fig.suptitle("Constant 1.7 V decoder supplies: edge-aligned SET/FORM shunt", y=1.01)
    finish(fig, "constant_rails_edge_aligned.png")


def plot_sweeps():
    sources = (
        ("set-vcc-skew-20260808-summary.csv", "WL 1.7 V", "#1769aa"),
        ("set-vcc-skew-wl2v-20260808-summary.csv", "WL 2.0 V", "#16825d"),
        ("set-vcc-skew-wl2p5v-20260808-summary.csv", "WL 2.5 V", "#bd4b00"),
    )
    fig, ax = plt.subplots(figsize=(9.2, 4.6))
    for filename, label, color in sources:
        rows = pd.read_csv(DATA / filename)
        ax.plot(rows.set_mV / 1000, rows.set_candidate_uA, marker="o", ms=4,
                color=color, label=f"SET candidate, {label}")
        ax.plot(rows.set_mV / 1000, rows.max_non_set_candidate_uA, color=color,
                alpha=0.38, ls="--", label=f"max non-SET, {label}")
    ax.axhline(25, color="#c62828", ls=":", lw=1.2, label="25 uA visibility threshold")
    ax.set(xlabel="Packet Vcc_set (V)", ylabel="7.5 us candidate magnitude (uA)",
           title="Post-guard SET-voltage sweeps")
    ax.set_xticks([x / 10 for x in range(15, 26)])
    ax.grid(alpha=0.22)
    ax.legend(frameon=False, ncol=2, fontsize=8)
    finish(fig, "set_vcc_sweeps.png")


if __name__ == "__main__":
    plot_read_shunt()
    plot_set_shunt()
    plot_static_rails()
    plot_sweeps()
