# Neuromorphic X2 remote-validated testbench archive

This folder is a local, separate copy of the simulation-only testbenches that were run on the remote Ubuntu/Caravel cocotb environment and with ngspice.

No firmware flashing or hardware execution is required by these tests.

## Contents

```text
Neuromorphic_X2_wb_beh.v                         # DUT behavioral RTL used by cocotb
cocotb/Makefile                                  # cocotb/icarus Makefile
cocotb/tests/test_neuromorphic_x2_wb_beh.py      # 12 cocotb tests
ngspice/read_reset_decoder_dc_compare.cir        # ngspice READ-vs-RESET DC reference bench
ngspice/run_read_reset_dc_ngspice.py             # ngspice runner/checker
ngspice/wl_supply_toggle_leakage_template.cir    # ngspice WL-rail shunt/leakage template
ngspice/run_wl_supply_toggle_leakage_ngspice.py  # ngspice WL-rail runner/checker
remote_results/cocotb_full_stdout.txt            # saved remote cocotb terminal output
remote_results/cocotb_full_results.xml           # saved remote cocotb xUnit results
remote_results/ngspice_read_reset_decoder_dc_compare_stdout.txt
remote_results/ngspice_read_reset_decoder_dc_compare.log
remote_results/ngspice_wl_supply_toggle_leakage_stdout.txt
remote_results/ngspice_wl_supply_set_selected_cell.log
remote_results/ngspice_wl_supply_reset_selected_cell.log
remote_results/ngspice_wl_supply_read_selected_cell.log
README.md                                        # detailed bench list and usage notes
```

## Remote commands used

### Full Caravel cocotb suite

```bash
cd ~/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb
PATH=~/caravel_user_Neuromorphic_X1_32x32/venv-cocotb/bin:$PATH make
```

Saved output:

```text
remote_results/cocotb_full_stdout.txt
remote_results/cocotb_full_results.xml
```

Observed final summary:

```text
TESTS=12 PASS=12 FAIL=0 SKIP=0
```

### ngspice READ-vs-RESET DC comparison

```bash
cd ~/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb/ngspice
./run_read_reset_dc_ngspice.py
```

Observed final summary:

```text
PASS: ngspice READ-vs-RESET DC decoder/control comparison
```

### ngspice WL supply toggle / shunt-current leakage comparison

```bash
cd ~/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb/ngspice
./run_wl_supply_toggle_leakage_ngspice.py
```

Observed final summary:

```text
PASS: WL supply toggle selected-cell/leakage ngspice comparison
```

The WL-supply ngspice bench toggles one modeled rail at a time:

```text
vcc_wl_set
vcc_wl_reset
vcc_wl_read
```

It compares selected-cell shunt current against leakage-only current and checks non-toggled rails remain off.

## Local run commands from this archive

If local cocotb/iverilog are available:

```bash
cd TEST_Bench/neuromorphic_x2_remote_validated_tb/cocotb
make
```

If local ngspice is available:

```bash
cd TEST_Bench/neuromorphic_x2_remote_validated_tb/ngspice
./run_read_reset_dc_ngspice.py
./run_wl_supply_toggle_leakage_ngspice.py
```

## Added WL rail toggle / shunt leakage benches

### Caravel cocotb correlate

```bash
cd ~/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb
PATH=~/caravel_user_Neuromorphic_X1_32x32/venv-cocotb/bin:$PATH \
  make TESTCASE=tb_wl_supply_toggle_selected_cell_mode_correlate
```

The full remote cocotb suite now includes 12 tests and was rerun with:

```text
TESTS=12 PASS=12 FAIL=0 SKIP=0
```

### ngspice WL rail shunt/leakage bench

```bash
cd ~/codex_remote/RISC-V-firmware/neuromorphic_x2_cocotb/ngspice
./run_wl_rail_toggle_shunt_leakage.py
```

Saved output:

```text
remote_results/ngspice_wl_rail_toggle_shunt_leakage_stdout.txt
remote_results/ngspice_wl_rail_toggle_shunt_leakage.log
```

Observed final summary:

```text
PASS: WL rail toggle shunt/leakage DC bench
```
