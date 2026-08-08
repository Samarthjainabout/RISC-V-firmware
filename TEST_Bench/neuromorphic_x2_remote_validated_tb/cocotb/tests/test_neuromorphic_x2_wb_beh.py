import os

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, RisingEdge, Timer, with_timeout


ADDR = 0x3000_0004
EMPTY_WORD = 0xA000_0000
CLK_PERIOD_NS = float(os.environ.get("CLK_PERIOD_NS", "10"))
READ_DELAY = int(os.environ.get("READ_DELAY", "12"))
PROGRAM_DELAY = int(os.environ.get("PROGRAM_DELAY", "16"))
COMPUTE_DELAY = int(os.environ.get("COMPUTE_DELAY", "14"))

MODE_RESET = 0b00
MODE_READ = 0b01
MODE_COMPUTE = 0b10
MODE_SET = 0b11


def pkt(mode, row=0, col=0, value=0, all_cols=False, reconfig=False):
    word = ((mode & 0x3) << 30) | ((row & 0x1F) << 25) | ((col & 0x1F) << 20) | (value & 0xFF)
    if all_cols:
        word |= 1 << 18
    if reconfig:
        word |= 1 << 17
    return word & 0xFFFF_FFFF


def cfg0(set_mid):
    return ((set_mid & 0xFFFF) << 16) | (set_mid & 0xFFFF)


def cfg1(reset_mid):
    return ((reset_mid & 0xFFFF) << 16) | (reset_mid & 0xFFFF)


def cfg2(no_of_clk_cycles=3, tdc_timeout=0x3F):
    return ((tdc_timeout & 0x7F) << 20) | (no_of_clk_cycles & 0x3FF)


def typed_response(valid_data, detail, col, value):
    if valid_data:
        response_class = 0x1
    elif detail == 0b001:
        response_class = 0x2
    elif detail != 0:
        response_class = 0x3
    else:
        response_class = 0x4
    return (
        (0xA << 28)
        | ((response_class & 0xF) << 24)
        | ((detail & 0x7) << 21)
        | ((col & 0x1F) << 14)
        | (value & 0x3FFF)
    ) & 0xFFFF_FFFF


def data_word(col, value):
    return typed_response(True, 0, col, value)


def timeout_word(col):
    return typed_response(False, 0b001, col, 0)


async def init_and_reset(dut):
    cocotb.start_soon(Clock(dut.wb_clk_i, CLK_PERIOD_NS, units="ns").start())
    dut.wbs_stb_i.value = 0
    dut.wbs_cyc_i.value = 0
    dut.wbs_we_i.value = 0
    dut.wbs_sel_i.value = 0
    dut.wbs_adr_i.value = 0
    dut.wbs_dat_i.value = 0
    dut.i_scan_si1.value = 0
    dut.i_scan_se1.value = 1
    dut.i_TM.value = 0
    dut.wb_rst_i.value = 1
    await ClockCycles(dut.wb_clk_i, 5)
    dut.wb_rst_i.value = 0
    await ClockCycles(dut.wb_clk_i, 3)


async def wb_cycle(dut, we, data=0, addr=ADDR, sel=0xF, timeout_cycles=20):
    await RisingEdge(dut.wb_clk_i)
    dut.wbs_adr_i.value = addr
    dut.wbs_dat_i.value = data
    dut.wbs_sel_i.value = sel
    dut.wbs_we_i.value = 1 if we else 0
    dut.wbs_cyc_i.value = 1
    dut.wbs_stb_i.value = 1

    for _ in range(timeout_cycles):
        await RisingEdge(dut.wb_clk_i)
        if int(dut.wbs_ack_o.value) == 1:
            rdata = int(dut.wbs_dat_o.value) & 0xFFFF_FFFF
            dut.wbs_cyc_i.value = 0
            dut.wbs_stb_i.value = 0
            dut.wbs_we_i.value = 0
            await RisingEdge(dut.wb_clk_i)
            return rdata

    dut.wbs_cyc_i.value = 0
    dut.wbs_stb_i.value = 0
    dut.wbs_we_i.value = 0
    raise AssertionError(f"Wishbone transaction did not ACK: we={we} addr=0x{addr:08X} data=0x{data:08X}")


async def wb_write(dut, data, addr=ADDR, sel=0xF):
    return await wb_cycle(dut, True, data=data, addr=addr, sel=sel)


async def wb_read(dut, addr=ADDR, sel=0xF):
    return await wb_cycle(dut, False, addr=addr, sel=sel)


async def expect_no_ack(dut, we, data=0, addr=ADDR, sel=0xF, cycles=8):
    await RisingEdge(dut.wb_clk_i)
    dut.wbs_adr_i.value = addr
    dut.wbs_dat_i.value = data
    dut.wbs_sel_i.value = sel
    dut.wbs_we_i.value = 1 if we else 0
    dut.wbs_cyc_i.value = 1
    dut.wbs_stb_i.value = 1
    for _ in range(cycles):
        await RisingEdge(dut.wb_clk_i)
        assert int(dut.wbs_ack_o.value) == 0
    dut.wbs_cyc_i.value = 0
    dut.wbs_stb_i.value = 0
    dut.wbs_we_i.value = 0
    await RisingEdge(dut.wb_clk_i)


async def apply_config(dut, set_mid=0x0100, reset_mid=0x0020, no_of_clk_cycles=3, tdc_timeout=0x3F):
    await wb_write(dut, cfg0(set_mid))
    await wb_write(dut, cfg1(reset_mid))
    await wb_write(dut, cfg2(no_of_clk_cycles, tdc_timeout))
    await ClockCycles(dut.wb_clk_i, 3)


async def poll_non_empty_read(dut, max_reads=80):
    for _ in range(max_reads):
        value = await wb_read(dut)
        if value != EMPTY_WORD:
            return value
        await ClockCycles(dut.wb_clk_i, 1)
    raise AssertionError("Response queue stayed empty")


async def program_and_wait(dut, packet, extra_cycles=8):
    await wb_write(dut, packet)
    await ClockCycles(dut.wb_clk_i, PROGRAM_DELAY + 8 + extra_cycles)


async def read_cell(dut, row, col, read_value=0):
    await wb_write(dut, pkt(MODE_READ, row=row, col=col, value=read_value))
    await ClockCycles(dut.wb_clk_i, READ_DELAY + 4)
    return await poll_non_empty_read(dut)


@cocotb.test()
async def tb_wishbone_decode_empty_and_config_only(dut):
    """Basic bus/address decode and the three configuration packets.

    Bench correlate: this is the safe packet-send sanity check before using
    DAC/shunt observations. Wrong address or byte-select should not produce a
    Wishbone ACK; empty reads return the firmware-visible EMPTY word.
    """
    await init_and_reset(dut)

    await expect_no_ack(dut, True, data=0xDEAD_BEEF, addr=ADDR + 4)
    await expect_no_ack(dut, True, data=0xDEAD_BEEF, sel=0x7)
    assert await wb_read(dut) == EMPTY_WORD

    await apply_config(dut, set_mid=0x0100, reset_mid=0x0020)
    assert await wb_read(dut) == EMPTY_WORD


@cocotb.test()
async def tb_read_path_single_and_all_columns(dut):
    """READ command decoding, queue ordering, and all-window scan.

    Bench correlate: maps to sending READ packets while looking for read-shunt
    activity. Single-column READ should return one typed DATA response; the
    all-columns flag should return columns 0..31 in order.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0100, reset_mid=0x0020)

    row = 3
    col = 7
    read_value = 5
    assert await read_cell(dut, row, col, read_value) == data_word(col, 0x0020 + read_value)
    assert await wb_read(dut) == EMPTY_WORD

    await wb_write(dut, pkt(MODE_READ, row=4, col=0, value=2, all_cols=True))
    await ClockCycles(dut.wb_clk_i, READ_DELAY + 6)
    for expected_col in range(32):
        assert await poll_non_empty_read(dut) == data_word(expected_col, 0x0020 + 2)
    assert await wb_read(dut) == EMPTY_WORD


@cocotb.test()
async def tb_set_reset_programming_then_readback(dut):
    """SET and RESET program a cell to different expected TDC regions.

    Bench correlate: send SET/RESET packets and check that the behavioral cell
    state changes exactly where read/set/reset shunt-current peaks are expected
    on hardware.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0100, reset_mid=0x0020, no_of_clk_cycles=4)

    row = 2
    col = 4
    await program_and_wait(dut, pkt(MODE_SET, row=row, col=col, value=3))
    assert await read_cell(dut, row, col, 0) == data_word(col, 0x0100 + 3)

    await program_and_wait(dut, pkt(MODE_RESET, row=row, col=col, value=9))
    assert await read_cell(dut, row, col, 0) == data_word(col, 0x0020 + 9)


@cocotb.test()
async def tb_compute_accumulator_and_timeout_order(dut):
    """Three COMPUTE packets produce selected-column DATA then timeout words.

    Bench correlate: this isolates compute-mode packet ordering using only the
    packet interface. DAC/shunt hardware tests can then verify whether the same
    command sequence creates the expected compute/read current signature.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0100, reset_mid=0x0020)

    selected_col = 5
    await program_and_wait(dut, pkt(MODE_SET, row=1, col=selected_col, value=0))
    await program_and_wait(dut, pkt(MODE_SET, row=2, col=selected_col, value=0))

    await wb_write(dut, pkt(MODE_COMPUTE, row=1, col=0, value=10))
    await wb_write(dut, pkt(MODE_COMPUTE, row=2, col=0, value=20))
    await wb_write(dut, pkt(MODE_COMPUTE, row=3, col=selected_col, value=30))
    await ClockCycles(dut.wb_clk_i, COMPUTE_DELAY + 6)

    assert await poll_non_empty_read(dut) == data_word(selected_col, 30)
    for expected_col in list(range(0, selected_col)) + list(range(selected_col + 1, 32)):
        assert await poll_non_empty_read(dut) == timeout_word(expected_col)
    assert await wb_read(dut) == EMPTY_WORD


@cocotb.test()
async def tb_runtime_reconfiguration_without_erasing_existing_cells(dut):
    """Special SET row=0 col=0 bit17 packet reopens the 3-config window.

    Bench correlate: lets you change DAC/threshold interpretation between runs
    without losing previous programmed-cell state in the behavioral model.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0100, reset_mid=0x0020)

    await program_and_wait(dut, pkt(MODE_SET, row=6, col=8, value=1))
    assert await read_cell(dut, 6, 8, 0) == data_word(8, 0x0101)

    await wb_write(dut, pkt(MODE_SET, row=0, col=0, reconfig=True))
    await ClockCycles(dut.wb_clk_i, 2)
    await apply_config(dut, set_mid=0x0300, reset_mid=0x0040)

    # Old cell survives reconfiguration.
    assert await read_cell(dut, 6, 8, 0) == data_word(8, 0x0101)

    # New programming uses new set midpoint.
    await program_and_wait(dut, pkt(MODE_SET, row=6, col=9, value=2))
    assert await read_cell(dut, 6, 9, 0) == data_word(9, 0x0302)


async def send_scan_word(dut, word):
    dut.i_TM.value = 1
    dut.i_scan_se1.value = 0
    dut.i_scan_si1.value = 0
    await RisingEdge(dut.wb_clk_i)  # leading dummy clock
    for bit in range(16):
        dut.i_scan_si1.value = (word >> bit) & 1
        await RisingEdge(dut.wb_clk_i)
    dut.i_scan_si1.value = 0
    await RisingEdge(dut.wb_clk_i)  # final capture clock
    # Stop shifting during the observation delay; keep TM high so decoded
    # WL/BL/SL outputs remain visible.
    dut.i_scan_se1.value = 1
    await ClockCycles(dut.wb_clk_i, 3)  # output pipeline delay


@cocotb.test()
async def tb_scan_debug_word_selects_wl_bl_sl_outputs(dut):
    """Scan/debug path: dummy + 16 LSB-first bits + capture edge.

    Bench correlate: this is the digital counterpart of varying scan pins and
    observing selected WL/BL/SL behavior with Saleae/ADC current probes.
    """
    await init_and_reset(dut)

    assert int(dut.o_TM.value) == 0
    assert int(dut.o_wl_float.value) == 0xFFFF_FFFF
    assert int(dut.o_bl_float.value) == 0xFFFF_FFFF
    assert int(dut.o_sl_float.value) == 0xFFFF_FFFF

    op_set = 1
    sl = 3
    bl = 11
    wl = 19
    scan_word = (op_set << 15) | (sl << 10) | (bl << 5) | wl
    await send_scan_word(dut, scan_word)

    assert int(dut.o_TM.value) == 1
    assert int(dut.o_wl_addr.value) == (1 << wl)
    assert int(dut.o_bl_addr.value) == (1 << bl)
    assert int(dut.o_sl_addr.value) == (1 << sl)
    assert int(dut.o_wl_float.value) == (0xFFFF_FFFF ^ (1 << wl))
    assert int(dut.o_bl_float.value) == (0xFFFF_FFFF ^ (1 << bl))
    assert int(dut.o_sl_float.value) == (0xFFFF_FFFF ^ (1 << sl))
    assert int(dut.o_bl_data.value) == (1 << bl)
    assert int(dut.o_sl_data.value) == 0
    assert int(dut.o_wl_data.value) == (1 << wl)

    op_set = 0
    sl = 4
    bl = 12
    wl = 20
    scan_word = (op_set << 15) | (sl << 10) | (bl << 5) | wl
    await send_scan_word(dut, scan_word)
    assert int(dut.o_wl_addr.value) == (1 << wl)
    assert int(dut.o_bl_addr.value) == (1 << bl)
    assert int(dut.o_sl_addr.value) == (1 << sl)
    assert int(dut.o_bl_data.value) == 0
    assert int(dut.o_sl_data.value) == (1 << sl)


@cocotb.test()
async def tb_same_packets_tolerate_changed_clock_period(dut):
    """Clock-variation smoke test.

    Run this test with different CLK_PERIOD_NS values. The design counts clock
    cycles, so packet semantics must be invariant to absolute ns period.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0180, reset_mid=0x0030, no_of_clk_cycles=2)
    await program_and_wait(dut, pkt(MODE_SET, row=9, col=10, value=7))
    assert await read_cell(dut, 9, 10, 1) == data_word(10, 0x0180 + 7 + 1)


def assert_scan_dc_outputs(dut, op_set, sl, bl, wl):
    """Check stable/DC scan decoder and SET/RESET polarity equations."""
    wl_mask = 1 << wl
    bl_mask = 1 << bl
    sl_mask = 1 << sl
    all_ones = 0xFFFF_FFFF

    assert int(dut.o_TM.value) == 1
    assert int(dut.o_mux_sel.value) == 0

    assert int(dut.o_wl_addr.value) == wl_mask
    assert int(dut.o_bl_addr.value) == bl_mask
    assert int(dut.o_sl_addr.value) == sl_mask

    assert int(dut.o_wl_float.value) == (all_ones ^ wl_mask)
    assert int(dut.o_bl_float.value) == (all_ones ^ bl_mask)
    assert int(dut.o_sl_float.value) == (all_ones ^ sl_mask)

    assert int(dut.o_wl_data.value) == wl_mask
    assert int(dut.o_bl_data.value) == (bl_mask if op_set else 0)
    assert int(dut.o_sl_data.value) == (0 if op_set else sl_mask)


@cocotb.test()
async def tb_dc_scan_decoders_exhaustive_onehot_and_polarity(dut):
    """DC decoder test: every 5-bit WL/BL/SL select gives one-hot output.

    This expands the basic scan test into a stable-output/DC-style decoder and
    polarity sweep.  For each selected index, the bench checks address one-hot,
    float inverse, WL data, and BL/SL SET-vs-RESET data polarity.
    """
    await init_and_reset(dut)

    for idx in range(32):
        for op_set in (0, 1):
            sl = idx
            bl = (idx * 7 + 3) & 0x1F
            wl = (idx * 11 + 5) & 0x1F
            scan_word = (op_set << 15) | (sl << 10) | (bl << 5) | wl
            await send_scan_word(dut, scan_word)
            assert_scan_dc_outputs(dut, op_set, sl, bl, wl)


@cocotb.test()
async def tb_dc_tm_output_mux_static_selects_scan_or_safe_defaults(dut):
    """DC mux-bank test: TM selects scan outputs; TM=0 selects safe defaults.

    In this behavioral source the normal/FSM side of the wide output mux banks
    is represented by safe constants.  This test verifies the mux bank changes
    with TM as a combinational/static select after a scan word has been latched.
    """
    await init_and_reset(dut)

    op_set = 1
    sl = 6
    bl = 13
    wl = 21
    scan_word = (op_set << 15) | (sl << 10) | (bl << 5) | wl
    await send_scan_word(dut, scan_word)
    assert_scan_dc_outputs(dut, op_set, sl, bl, wl)

    # Toggle the TM mux select without clocking.  Outputs should follow the
    # combinational mux bank: scan vector when TM=1, safe default when TM=0.
    dut.i_TM.value = 0
    await Timer(1, units="ns")
    assert int(dut.o_TM.value) == 0
    assert int(dut.o_mux_sel.value) == 0
    assert int(dut.o_wl_addr.value) == 0
    assert int(dut.o_bl_addr.value) == 0
    assert int(dut.o_sl_addr.value) == 0
    assert int(dut.o_wl_data.value) == 0
    assert int(dut.o_bl_data.value) == 0
    assert int(dut.o_sl_data.value) == 0
    assert int(dut.o_wl_float.value) == 0xFFFF_FFFF
    assert int(dut.o_bl_float.value) == 0xFFFF_FFFF
    assert int(dut.o_sl_float.value) == 0xFFFF_FFFF

    dut.i_TM.value = 1
    await Timer(1, units="ns")
    assert_scan_dc_outputs(dut, op_set, sl, bl, wl)

    dut.i_TM.value = 0
    await Timer(1, units="ns")
    assert int(dut.o_wl_addr.value) == 0
    assert int(dut.o_wl_float.value) == 0xFFFF_FFFF


@cocotb.test()
async def tb_dc_timeout_subtractor_compare_and_saturation_mux(dut):
    """DC datapath-equivalent test for read timeout subtract/compare + mux.

    The physical subtractor/current-to-time block is analog and not instantiated
    in this behavioral file.  The digital equivalent visible here is:

        raw_count = cell_level + read_value
        timeout_ceiling = {tdc_time_out[5:0], 8'hFF}
        output = raw_count if raw_count <= timeout_ceiling else timeout_ceiling

    This checks below-ceiling, exact-boundary, and above-ceiling saturation.
    """
    await init_and_reset(dut)

    # tdc_timeout=1 gives timeout_ceiling = 0x01FF.  Initial config fills all
    # cells with reset_mid + 0 because no normal operation has happened yet.
    await apply_config(dut, set_mid=0x0300, reset_mid=0x0180, tdc_timeout=0x01)
    ceiling = 0x01FF

    assert await read_cell(dut, row=0, col=2, read_value=0x10) == data_word(2, 0x0180 + 0x10)
    assert await read_cell(dut, row=0, col=3, read_value=0x7F) == data_word(3, ceiling)
    assert await read_cell(dut, row=0, col=4, read_value=0x90) == data_word(4, ceiling)


@cocotb.test()
async def tb_compare_reset_vs_read_same_decoder_selection(dut):
    """Compare RESET and READ for the same row/column selection.

    In this behavioral model the normal-mode WL/BL/SL decoder wires are not
    exposed; only firmware-visible effects are observable.  For the same
    row/column selection, RESET is a write/program operation with no response
    word, while READ is a measurement operation that queues one typed DATA word
    for the selected column.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0120, reset_mid=0x0040)

    row = 10
    col = 12

    # First set the selected cell so RESET has an observable state change.
    await program_and_wait(dut, pkt(MODE_SET, row=row, col=col, value=0x05))
    assert await read_cell(dut, row, col, 0) == data_word(col, 0x0120 + 0x05)

    # RESET uses the same row/column fields, changes the stored cell state/level,
    # and should not queue a read response by itself.
    await program_and_wait(dut, pkt(MODE_RESET, row=row, col=col, value=0x09))
    assert await wb_read(dut) == EMPTY_WORD

    # READ of the same row/column selection returns a typed DATA response for
    # the selected column, now reflecting the reset level plus read offset.
    assert await read_cell(dut, row, col, 0x03) == data_word(col, 0x0040 + 0x09 + 0x03)

    # READ all-columns is different from RESET: the same row can expand to 32
    # response words, whereas RESET targets only the addressed cell in this model.
    await wb_write(dut, pkt(MODE_READ, row=row, col=0, value=0x01, all_cols=True))
    await ClockCycles(dut.wb_clk_i, READ_DELAY + 6)
    responses = [await poll_non_empty_read(dut) for _ in range(32)]
    assert responses[col] == data_word(col, 0x0040 + 0x09 + 0x01)
    for other_col, response in enumerate(responses):
        if other_col != col:
            assert response == data_word(other_col, 0x0040 + 0x01)

@cocotb.test()
async def tb_wl_supply_toggle_selected_cell_mode_correlate(dut):
    """Digital correlate for toggling Vcc_wl_set/reset/read on one cell.

    The behavioral RTL does not expose analog Vcc_wl_set/Vcc_wl_reset/
    Vcc_wl_read pins, so leakage current is checked by the companion ngspice
    bench.  This cocotb test verifies the digital side that must line up with
    that analog experiment: the same row/column selection can be exercised by
    SET, RESET, and READ packets; SET/RESET change the stored cell without a
    direct response word, while READ of the same selected cell returns a typed
    DATA response.  The rail association checked here is the intended bench
    mapping: SET->Vcc_wl_set, RESET->Vcc_wl_reset, READ->Vcc_wl_read.
    """
    await init_and_reset(dut)
    await apply_config(dut, set_mid=0x0200, reset_mid=0x0060, no_of_clk_cycles=2)

    row = 11
    col = 14
    rails = {
        "set": 1.80,
        "reset": 1.20,
        "read": 0.40,
    }
    assert rails["set"] > rails["reset"] > rails["read"] > 0.0

    # SET phase: would be paired with external Vcc_wl_set toggle in hardware.
    await program_and_wait(dut, pkt(MODE_SET, row=row, col=col, value=0x04))
    assert await wb_read(dut) == EMPTY_WORD
    assert await read_cell(dut, row, col, 0x00) == data_word(col, 0x0200 + 0x04)

    # RESET phase: same decoder selection, reset rail/polarity in hardware.
    await program_and_wait(dut, pkt(MODE_RESET, row=row, col=col, value=0x07))
    assert await wb_read(dut) == EMPTY_WORD
    assert await read_cell(dut, row, col, 0x00) == data_word(col, 0x0060 + 0x07)

    # READ phase: same decoder selection, read rail/sense path in hardware.
    assert await read_cell(dut, row, col, 0x03) == data_word(col, 0x0060 + 0x07 + 0x03)

    # Neighbor column remains at reset baseline plus read offset; this is the
    # digital selection correlate for distinguishing selected-cell current from
    # array/background leakage in the ngspice bench.
    neighbor_col = (col + 1) & 0x1F
    assert await read_cell(dut, row, neighbor_col, 0x03) == data_word(neighbor_col, 0x0060 + 0x03)

