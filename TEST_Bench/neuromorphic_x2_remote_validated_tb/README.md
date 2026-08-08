# Neuromorphic_X2_wb_beh simulation benches

Simulation-only cocotb benches for `../Neuromorphic_X2_wb_beh.v`.
They do not program firmware or touch hardware.

Run locally if cocotb/iverilog are installed:

```bash
cd neuromorphic_x2_cocotb
make
```

Run on the remote Ubuntu/Caravel cocotb environment:

```bash
cd ~/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb
PATH=~/caravel_user_Neuromorphic_X1_32x32/venv-cocotb/bin:$PATH make
```

Clock variation example on the remote:

```bash
PATH=~/caravel_user_Neuromorphic_X1_32x32/venv-cocotb/bin:$PATH \
  make TESTCASE=tb_same_packets_tolerate_changed_clock_period CLK_PERIOD_NS=25
```

Bench list:

| Cocotb test | Hardware-controllable correlate |
|---|---|
| `tb_wishbone_decode_empty_and_config_only` | Basic packet write/read sanity before applying DAC/current observations. |
| `tb_read_path_single_and_all_columns` | READ packet path; single-column and all-window reads. |
| `tb_set_reset_programming_then_readback` | SET/RESET packets; expected active set/reset shunt windows and readback state. |
| `tb_compute_accumulator_and_timeout_order` | COMPUTE packet ordering and selected-column result/timeout ordering. |
| `tb_runtime_reconfiguration_without_erasing_existing_cells` | Reconfiguration packet plus next 3 config writes; checks programmed cells survive. |
| `tb_scan_debug_word_selects_wl_bl_sl_outputs` | Scan/debug pin sequence: dummy + 16 LSB-first bits + capture edge. |
| `tb_same_packets_tolerate_changed_clock_period` | Same packets at alternate clock periods. |
| `tb_dc_scan_decoders_exhaustive_onehot_and_polarity` | DC/static decoder sweep: every WL/BL/SL select, one-hot address, float inverse, SET/RESET polarity. |
| `tb_dc_tm_output_mux_static_selects_scan_or_safe_defaults` | DC/static TM mux-bank check: scan vectors when TM=1, safe defaults when TM=0, no clock required for mux change. |
| `tb_dc_timeout_subtractor_compare_and_saturation_mux` | Digital equivalent of subtract/compare/saturation mux path: below timeout, exact boundary, above-timeout clamp. |
| `tb_compare_reset_vs_read_same_decoder_selection` | RESET vs READ on the same row/column: same selection fields, but RESET programs/no response while READ queues typed data. |
| `tb_wl_supply_toggle_selected_cell_mode_correlate` | Digital correlate for toggling `vcc_wl_set`, `vcc_wl_reset`, `vcc_wl_read`: same selected cell through SET/RESET/READ and neighbor remains baseline. |

## READ vs RESET DC comparison benches

Cocotb comparison:

```bash
cd neuromorphic_x2_cocotb
make TESTCASE=tb_compare_reset_vs_read_same_decoder_selection
```

ngspice DC comparison:

```bash
cd neuromorphic_x2_cocotb/ngspice
./run_read_reset_dc_ngspice.py
```

The ngspice bench checks the DC/reference line-control difference after the common decoder selection:

- READ and RESET use the same selected WL/BL/SL address decode.
- Unselected lines remain floated.
- READ floats/senses the selected BL and enables subtractor/TDC/response behavior.
- Main RESET drives selected BL low, selected SL high, selected WL high, and does not enable subtractor/TDC/response directly.

## WL supply toggle / shunt-current leakage benches

Cocotb digital correlate:

```bash
cd neuromorphic_x2_cocotb
make TESTCASE=tb_wl_supply_toggle_selected_cell_mode_correlate
```

ngspice DC shunt-current/leakage comparison:

```bash
cd neuromorphic_x2_cocotb/ngspice
./run_wl_supply_toggle_leakage_ngspice.py
```

The ngspice bench toggles one modeled WL rail at a time:

- `vcc_wl_set`
- `vcc_wl_reset`
- `vcc_wl_read`

For each rail it compares selected-cell shunt current against leakage-only current and checks non-toggled rails remain off.  This is a reference DC/leakage model, not an extracted array netlist.

## WL rail toggle / shunt leakage ngspice comparison

This bench models your proposed DC experiment: toggle `VCC_WL_SET`, `VCC_WL_RESET`, and `VCC_WL_READ` one at a time and read the shunt values connected to those rails.

```bash
cd neuromorphic_x2_cocotb/ngspice
./run_wl_rail_toggle_shunt_leakage.py
```

It checks:

- mux behavior: inactive WL rails have approximately zero shunt current;
- decoder behavior: selected-cell branch current dominates aggregate unselected-array leakage;
- read/subtractor behavior: `read_ishunt - read_leakage` reconstructs the selected read-cell current.

The reference ngspice model is compact and behavioral, not a PDK-extracted array. Its purpose is to define the expected current-signature checks before applying the same idea to hardware shunt measurements.
