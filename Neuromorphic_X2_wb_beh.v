`timescale 1ns/1ps

// -----------------------------------------------------------------------------
// Wishbone plus scan/debug behavioral model in Verilog-2001.
//
// Firmware-visible behavior:
//   - First 3 writes after reset are configuration packets.
//   - Normal writes are command packets.
//   - Reads return queued RTL-compatible typed response words.
//   - Empty response reads acknowledge immediately with 32'hA000_0000.
//   - Runtime reconfiguration is triggered by:
//       mode=2'b11, row=0, col=0, bit[17]=1
//     After that trigger, the next 3 writes are configuration packets again.
//
// Result format:
//   [31:28] = 4'hA
//   [27:24] = response class: 0=EMPTY, 1=DATA, 2=TIMEOUT,
//                              3=ERROR, 4=JUNK
//   [23:21] = detail/error
//   [20:19] = reserved
//   [18:14] = column/channel
//   [13:0]  = TDC value
//
// Scan/debug behavior:
//   - i_TM=1 selects the scan/debug outputs.
//   - Like the RTL, shifting is enabled when i_scan_se1=0.
//   - The RTL counter requires one leading dummy clock, a 16-bit word
//     LSB-first, and one final capture clock:
//       {op_set, sl_sel[4:0], bl_sel[4:0], wl_sel[4:0]}
//
// Compute-mode result order:
//   selected compute column first, then all remaining columns from 0 to 31,
//   skipping the selected column because it was already returned first.
// -----------------------------------------------------------------------------

module Neuromorphic_X2_wb_beh #(
  parameter [31:0] ADDR_MATCH        = 32'h3000_0004,
  parameter integer READ_DELAY       = 160,
  parameter integer PROGRAM_DELAY    = 220,
  parameter integer COMPUTE_DELAY    = 180,
  parameter integer CONFIG_WRITES    = 3
)(
  input         wb_clk_i,
  input         wb_rst_i,
  input         wbs_stb_i,
  input         wbs_cyc_i,
  input         wbs_we_i,
  input  [3:0]  wbs_sel_i,
  input  [31:0] wbs_dat_i,
  input  [31:0] wbs_adr_i,
  output reg [31:0] wbs_dat_o,
  output reg        wbs_ack_o,

  // RTL-compatible scan/debug interface.  Unconnected scan inputs are treated
  // as inactive so existing Wishbone-only named-port instantiations still work.
  input             i_scan_si1,
  input             i_scan_se1,
  input             i_TM,
  output wire       o_TM,
  output reg  [1:0] o_mux_sel,
  output reg [31:0] o_sl_float,
  output reg [31:0] o_sl_addr,
  output reg [31:0] o_sl_data,
  output reg [31:0] o_bl_addr,
  output reg [31:0] o_bl_data,
  output reg [31:0] o_bl_float,
  output reg [31:0] o_wl_float,
  output reg [31:0] o_wl_addr,
  output reg [31:0] o_wl_data
);

  localparam [1:0] MODE_RESET   = 2'b00;
  localparam [1:0] MODE_READ    = 2'b01;
  localparam [1:0] MODE_COMPUTE = 2'b10;
  localparam [1:0] MODE_SET     = 2'b11;

  localparam [2:0] STATUS_OK           = 3'b000;
  localparam [2:0] STATUS_BAD_COMMAND  = 3'b010;
  localparam [2:0] STATUS_COMPUTE_WAIT = 3'b100;

  integer r;
  integer c;

  reg [31:0] array_state [0:31];
  reg [13:0] cell_level  [0:1023];

  reg [31:0] command_q  [0:31];
  reg [31:0] response_q [0:31];

  reg [4:0] command_wr_idx;
  reg       command_wr_wrap;
  reg [4:0] command_rd_idx;
  reg       command_rd_wrap;

  reg [4:0] response_wr_idx;
  reg       response_wr_wrap;
  reg [4:0] response_rd_idx;
  reg       response_rd_wrap;

  reg [2:0]  status_code;
  reg [15:0] target_set1;
  reg [15:0] target_set2;
  reg [15:0] target_reset1;
  reg [15:0] target_reset2;
  reg [9:0]  no_of_clk_cycles;
  reg [9:0]  counter_value;
  reg [6:0]  tdc_time_out;
  reg [1:0]  tdc_dead_time;
  integer    config_count;

  reg [31:0] compute_packet0;
  reg [31:0] compute_packet1;
  integer    compute_count;
  reg        normal_operation_seen;

  wire selected;
  wire scan_test_mode;
  wire command_empty;
  wire command_full;
  wire response_empty;
  wire response_full;

  assign selected = wbs_stb_i && wbs_cyc_i &&
                    (wbs_sel_i == 4'hF) &&
                    (wbs_adr_i == ADDR_MATCH);

  // Case equality makes omitted Verilog-2001 named ports (Z) safely inactive.
  assign scan_test_mode = (i_TM === 1'b1);
  assign o_TM = scan_test_mode;

  assign command_empty = (command_wr_idx == command_rd_idx) &&
                         (command_wr_wrap == command_rd_wrap);
  assign command_full  = (command_wr_idx == command_rd_idx) &&
                         (command_wr_wrap != command_rd_wrap);
  assign response_empty = (response_wr_idx == response_rd_idx) &&
                          (response_wr_wrap == response_rd_wrap);
  assign response_full  = (response_wr_idx == response_rd_idx) &&
                          (response_wr_wrap != response_rd_wrap);

  function [9:0] cell_index;
    input [4:0] row_index;
    input [4:0] col_index;
    begin
      cell_index = {row_index, col_index};
    end
  endfunction

  function [31:0] onehot5;
    input [4:0] index;
    begin
      onehot5 = 32'h0000_0000;
      onehot5[index] = 1'b1;
    end
  endfunction

  function is_reconfig_packet;
    input [31:0] packet;
    begin
      is_reconfig_packet = (packet[31:30] == MODE_SET) &&
                           (packet[29:25] == 5'd0) &&
                           (packet[24:20] == 5'd0) &&
                           (packet[17] == 1'b1);
    end
  endfunction

  function [13:0] target_midpoint;
    input [15:0] target_a;
    input [15:0] target_b;
    reg [15:0] low_target;
    reg [15:0] high_target;
    reg [16:0] sum;
    begin
      if (target_a > target_b) begin
        high_target = target_a;
        low_target  = target_b;
      end else begin
        high_target = target_b;
        low_target  = target_a;
      end

      sum = {1'b0, low_target} + {1'b0, high_target};
      target_midpoint = sum[14:1];
    end
  endfunction

  function [13:0] programmed_level;
    input       set_cell;
    input [7:0] program_value;
    begin
      if (set_cell)
        programmed_level = target_midpoint(target_set1, target_set2) +
                           {6'd0, program_value};
      else
        programmed_level = target_midpoint(target_reset1, target_reset2) +
                           {6'd0, program_value};
    end
  endfunction

  function [13:0] cell_value;
    input [4:0] row_index;
    input [4:0] col_index;
    input [7:0] read_value;
    reg [13:0] raw_count;
    reg [13:0] timeout_ceiling;
    begin
      raw_count = cell_level[cell_index(row_index, col_index)] +
                  {6'd0, read_value};
      timeout_ceiling = {tdc_time_out[5:0], 8'hFF};

      if (raw_count > timeout_ceiling)
        cell_value = timeout_ceiling;
      else
        cell_value = raw_count;
    end
  endfunction

  function [31:0] typed_response;
    input        valid_data;
    input [2:0]  detail;
    input [4:0] col_index;
    input [13:0] value;
    reg [3:0] response_class;
    begin
      if (valid_data)
        response_class = 4'h1;
      else if (detail == 3'b001)
        response_class = 4'h2;
      else if (detail != 3'b000)
        response_class = 4'h3;
      else
        response_class = 4'h4;

      typed_response = {
        4'hA,
        response_class,
        detail,
        2'b00,
        col_index,
        value
      };
    end
  endfunction

  function [31:0] result_word;
    input [4:0] col_index;
    input [13:0] value;
    begin
      result_word = typed_response(1'b1, 3'b000, col_index, value);
    end
  endfunction

  function [31:0] timeout_word;
    input [4:0] col_index;
    begin
      timeout_word = typed_response(1'b0, 3'b001, col_index, 14'd0);
    end
  endfunction

  task wait_cycles;
    input integer cycles;
    begin
      repeat (cycles) @(posedge wb_clk_i);
    end
  endtask

  task advance_command_read;
    begin
      if (command_rd_idx == 5'd31) begin
        command_rd_idx  = 5'd0;
        command_rd_wrap = ~command_rd_wrap;
      end else begin
        command_rd_idx = command_rd_idx + 5'd1;
      end
    end
  endtask

  task advance_response_write;
    begin
      if (response_wr_idx == 5'd31) begin
        response_wr_idx  = 5'd0;
        response_wr_wrap = ~response_wr_wrap;
      end else begin
        response_wr_idx = response_wr_idx + 5'd1;
      end
    end
  endtask

  task push_response;
    input [31:0] value;
    begin
      while (response_full && !wb_rst_i)
        @(posedge wb_clk_i);

      if (!wb_rst_i) begin
        response_q[response_wr_idx] = value;
        advance_response_write();
      end
    end
  endtask

  task emit_read_results;
    input [31:0] packet;
    integer col;
    reg [4:0] row_index;
    reg [31:0] col_mask;
    begin
      row_index = packet[29:25];
      col_mask = packet[18] ? 32'hFFFF_FFFF : onehot5(packet[24:20]);

      wait_cycles(READ_DELAY);

      if (!wb_rst_i) begin
        for (col = 0; col < 32; col = col + 1) begin
          if (col_mask[col]) begin
            push_response(result_word(col[4:0],
                                      cell_value(row_index,
                                                 col[4:0],
                                                 packet[7:0])));
          end
        end
        status_code = STATUS_OK;
      end
    end
  endtask

  task run_program;
    input [31:0] packet;
    input        set_cell;
    reg [4:0] row_index;
    reg [4:0] col_index;
    begin
      row_index = packet[29:25];
      col_index = packet[24:20];

      wait_cycles(PROGRAM_DELAY + no_of_clk_cycles);

      if (!wb_rst_i) begin
        array_state[row_index][col_index] = set_cell;
        cell_level[cell_index(row_index, col_index)] =
          programmed_level(set_cell, packet[7:0]);
        status_code = STATUS_OK;
      end
    end
  endtask

  task push_compute_column;
    input [31:0] packet0;
    input [31:0] packet1;
    input [31:0] packet2;
    input [4:0]  col_index;
    reg [13:0] acc;
    begin
      acc = 14'd0;

      if (array_state[packet0[29:25]][col_index])
        acc = acc + {6'd0, packet0[7:0]};
      if (array_state[packet1[29:25]][col_index])
        acc = acc + {6'd0, packet1[7:0]};
      if (array_state[packet2[29:25]][col_index])
        acc = acc + {6'd0, packet2[7:0]};

      push_response(result_word(col_index, acc));
    end
  endtask

  task emit_compute_results;
    input [31:0] packet0;
    input [31:0] packet1;
    input [31:0] packet2;
    integer col;
    reg [31:0] rows;
    reg [4:0] selected_col;
    begin
      rows = onehot5(packet0[29:25]) |
             onehot5(packet1[29:25]) |
             onehot5(packet2[29:25]);

      selected_col = packet2[24:20];

      wait_cycles(COMPUTE_DELAY);

      if (!wb_rst_i) begin
        push_compute_column(packet0, packet1, packet2, selected_col);

        for (col = 0; col < 32; col = col + 1) begin
          if (col[4:0] != selected_col)
            push_response(timeout_word(col[4:0]));
        end

        status_code = (rows == 32'h0000_0000) ? STATUS_BAD_COMMAND : STATUS_OK;
      end
    end
  endtask

  task apply_config;
    input [31:0] packet;
    begin
      case (config_count)
        0: begin
          target_set1 = packet[15:0];
          target_set2 = packet[31:16];
        end
        1: begin
          target_reset1 = packet[15:0];
          target_reset2 = packet[31:16];
        end
        2: begin
          no_of_clk_cycles = packet[9:0];
          counter_value    = packet[19:10];
          tdc_time_out     = packet[26:20];
          tdc_dead_time    = packet[31:30];
        end
      endcase

      config_count = config_count + 1;

      if ((config_count == CONFIG_WRITES) && !normal_operation_seen) begin
        for (r = 0; r < 32; r = r + 1) begin
          for (c = 0; c < 32; c = c + 1)
            cell_level[cell_index(r[4:0], c[4:0])] = programmed_level(1'b0, 8'h00);
        end
      end

      status_code = STATUS_OK;
    end
  endtask

  task execute_packet;
    input [31:0] packet;
    begin
      if (config_count < CONFIG_WRITES) begin
        apply_config(packet);
      end else begin
        if (is_reconfig_packet(packet)) begin
          config_count = 0;
          compute_count = 0;
          status_code = STATUS_OK;
        end else begin
        case (packet[31:30])
          MODE_READ: begin
            normal_operation_seen = 1'b1;
            compute_count = 0;
            emit_read_results(packet);
          end

          MODE_SET: begin
            normal_operation_seen = 1'b1;
            compute_count = 0;
            run_program(packet, 1'b1);
          end

          MODE_RESET: begin
            normal_operation_seen = 1'b1;
            compute_count = 0;
            run_program(packet, 1'b0);
          end

          MODE_COMPUTE: begin
            normal_operation_seen = 1'b1;
            status_code = STATUS_COMPUTE_WAIT;
            if (compute_count == 0) begin
              compute_packet0 = packet;
              compute_count = 1;
            end else if (compute_count == 1) begin
              compute_packet1 = packet;
              compute_count = 2;
            end else begin
              emit_compute_results(compute_packet0, compute_packet1, packet);
              compute_count = 0;
            end
          end

          default: begin
            status_code = STATUS_BAD_COMMAND;
          end
        endcase
        end
      end
    end
  endtask

  always @(posedge wb_clk_i or posedge wb_rst_i) begin
    if (wb_rst_i) begin
      wbs_ack_o        <= 1'b0;
      wbs_dat_o        <= 32'h0000_0000;
      command_wr_idx   <= 5'd0;
      command_wr_wrap  <= 1'b0;
      response_rd_idx  <= 5'd0;
      response_rd_wrap <= 1'b0;
    end else begin
      wbs_ack_o <= 1'b0;

      if (selected && wbs_we_i && !wbs_ack_o) begin
        if (!command_full) begin
          command_q[command_wr_idx] <= wbs_dat_i;

          if (command_wr_idx == 5'd31) begin
            command_wr_idx  <= 5'd0;
            command_wr_wrap <= ~command_wr_wrap;
          end else begin
            command_wr_idx <= command_wr_idx + 5'd1;
          end

          wbs_ack_o <= 1'b1;
        end
      end else if (selected && !wbs_we_i && !wbs_ack_o) begin
        if (response_empty) begin
          // Match Wb_slave.v: an empty read is a completed transaction, not
          // a blocking wait for a future result.
          wbs_dat_o <= 32'hA000_0000;
          wbs_ack_o <= 1'b1;
        end else begin
          wbs_dat_o <= response_q[response_rd_idx];

          if (response_rd_idx == 5'd31) begin
            response_rd_idx  <= 5'd0;
            response_rd_wrap <= ~response_rd_wrap;
          end else begin
            response_rd_idx <= response_rd_idx + 5'd1;
          end

          wbs_ack_o <= 1'b1;
        end
      end
    end
  end

  initial begin
    for (r = 0; r < 32; r = r + 1) begin
      array_state[r] = 32'h0000_0000;
      for (c = 0; c < 32; c = c + 1)
        cell_level[cell_index(r[4:0], c[4:0])] = 14'h0E23;
    end

    command_rd_idx   = 5'd0;
    command_rd_wrap  = 1'b0;
    response_wr_idx  = 5'd0;
    response_wr_wrap = 1'b0;
    status_code      = STATUS_OK;
    target_set1      = 16'hC40F;
    target_set2      = 16'hA203;
    target_reset1    = 16'h0D43;
    target_reset2    = 16'h0F03;
    no_of_clk_cycles = 10'd3;
    counter_value    = 10'd3;
    tdc_time_out     = 7'd32;
    tdc_dead_time    = 2'b01;
    config_count     = 0;
    compute_packet0  = 32'h0000_0000;
    compute_packet1  = 32'h0000_0000;
    compute_count    = 0;
    normal_operation_seen = 1'b0;

    forever begin
      @(posedge wb_clk_i or posedge wb_rst_i);

      if (wb_rst_i) begin
        for (r = 0; r < 32; r = r + 1) begin
          array_state[r] = 32'h0000_0000;
          for (c = 0; c < 32; c = c + 1)
            cell_level[cell_index(r[4:0], c[4:0])] = 14'h0E23;
        end

        command_rd_idx   = 5'd0;
        command_rd_wrap  = 1'b0;
        response_wr_idx  = 5'd0;
        response_wr_wrap = 1'b0;
        status_code      = STATUS_OK;
        target_set1      = 16'hC40F;
        target_set2      = 16'hA203;
        target_reset1    = 16'h0D43;
        target_reset2    = 16'h0F03;
        no_of_clk_cycles = 10'd3;
        counter_value    = 10'd3;
        tdc_time_out     = 7'd32;
        tdc_dead_time    = 2'b01;
        config_count     = 0;
        compute_packet0  = 32'h0000_0000;
        compute_packet1  = 32'h0000_0000;
        compute_count    = 0;
        normal_operation_seen = 1'b0;
      end else if (!command_empty) begin
        execute_packet(command_q[command_rd_idx]);
        advance_command_read();
      end
    end
  end

  // ---------------------------------------------------------------------------
  // Scan/debug capture and output model.
  //
  // This intentionally mirrors top_module.sv, including the active-low
  // i_scan_se1 condition and its counter behavior.  Because the first edge
  // only arms the counter, the useful frame is: dummy, 16 data bits
  // LSB-first, capture edge.
  // ---------------------------------------------------------------------------
  reg [15:0] scan_shift;
  reg [15:0] scan_word;
  reg        scan_word_valid;
  reg        scan_in_progress;
  reg [4:0]  scan_bit_count;
  reg        scan_op_set;
  reg [4:0]  scan_sl_sel;
  reg [4:0]  scan_bl_sel;
  reg [4:0]  scan_wl_sel;
  reg        scan_op_set_d;
  reg [4:0]  scan_sl_sel_d;
  reg [4:0]  scan_bl_sel_d;
  reg [4:0]  scan_wl_sel_d;
  integer    scan_i;

  always @(posedge wb_clk_i) begin
    if (wb_rst_i) begin
      scan_shift       <= 16'd0;
      scan_word        <= 16'd0;
      scan_word_valid  <= 1'b0;
      scan_in_progress <= 1'b0;
      scan_bit_count   <= 5'd0;
    end else begin
      scan_word_valid <= 1'b0;

      if ((i_scan_se1 === 1'b0) && !scan_test_mode) begin
        scan_in_progress <= 1'b0;
        scan_bit_count   <= 5'd0;
      end else if ((i_scan_se1 === 1'b0) && scan_test_mode) begin
        scan_shift <= {i_scan_si1, scan_shift[15:1]};

        if (!scan_in_progress) begin
          scan_in_progress <= 1'b1;
          scan_bit_count   <= 5'd0;
        end else begin
          scan_bit_count <= scan_bit_count + 5'd1;
        end

        if (scan_bit_count == 5'd16) begin
          scan_word        <= scan_shift;
          scan_word_valid  <= 1'b1;
          scan_in_progress <= 1'b0;
          scan_bit_count   <= 5'd0;
        end
      end
    end
  end

  always @(posedge wb_clk_i) begin
    if (wb_rst_i) begin
      scan_op_set   <= 1'b0;
      scan_sl_sel   <= 5'd0;
      scan_bl_sel   <= 5'd0;
      scan_wl_sel   <= 5'd0;
      scan_op_set_d <= 1'b0;
      scan_sl_sel_d <= 5'd0;
      scan_bl_sel_d <= 5'd0;
      scan_wl_sel_d <= 5'd0;
    end else begin
      if (scan_word_valid) begin
        scan_op_set <= scan_word[15];
        scan_sl_sel <= scan_word[14:10];
        scan_bl_sel <= scan_word[9:5];
        scan_wl_sel <= scan_word[4:0];
      end

      scan_op_set_d <= scan_op_set;
      scan_sl_sel_d <= scan_sl_sel;
      scan_bl_sel_d <= scan_bl_sel;
      scan_wl_sel_d <= scan_wl_sel;
    end
  end

  always @(*) begin
    o_wl_float = 32'hFFFF_FFFF;
    o_bl_float = 32'hFFFF_FFFF;
    o_sl_float = 32'hFFFF_FFFF;
    o_wl_data  = 32'h0000_0000;
    o_bl_data  = 32'h0000_0000;
    o_sl_data  = 32'h0000_0000;
    o_wl_addr  = 32'h0000_0000;
    o_bl_addr  = 32'h0000_0000;
    o_sl_addr  = 32'h0000_0000;
    o_mux_sel  = 2'b00;

    if (scan_test_mode) begin
      for (scan_i = 0; scan_i < 32; scan_i = scan_i + 1) begin
        o_wl_addr[scan_i]  = (scan_i == scan_wl_sel_d);
        o_wl_float[scan_i] = ~o_wl_addr[scan_i];
        o_wl_data[scan_i]  = o_wl_addr[scan_i];

        o_bl_addr[scan_i]  = (scan_i == scan_bl_sel_d);
        o_bl_float[scan_i] = ~o_bl_addr[scan_i];
        o_bl_data[scan_i]  =
            o_bl_addr[scan_i] ? scan_op_set_d : 1'b0;

        o_sl_addr[scan_i]  = (scan_i == scan_sl_sel_d);
        o_sl_float[scan_i] = ~o_sl_addr[scan_i];
        o_sl_data[scan_i]  =
            o_sl_addr[scan_i] ? ~scan_op_set_d : 1'b0;
      end
    end
  end

endmodule

`ifdef NEUROMORPHIC_X2_WB_BEH_AS_RTL
module Neuromorphic_X2_wb #(
  parameter [31:0] ADDR_MATCH        = 32'h3000_0004,
  parameter integer READ_DELAY       = 160,
  parameter integer PROGRAM_DELAY    = 220,
  parameter integer COMPUTE_DELAY    = 180,
  parameter integer CONFIG_WRITES    = 3
)(
  input         wb_clk_i,
  input         wb_rst_i,
  input         wbs_stb_i,
  input         wbs_cyc_i,
  input         wbs_we_i,
  input  [3:0]  wbs_sel_i,
  input  [31:0] wbs_dat_i,
  input  [31:0] wbs_adr_i,
  output [31:0] wbs_dat_o,
  output        wbs_ack_o,
  input         i_scan_si1,
  input         i_scan_se1,
  input         i_TM,
  output        o_TM,
  output [1:0]  o_mux_sel,
  output [31:0] o_sl_float,
  output [31:0] o_sl_addr,
  output [31:0] o_sl_data,
  output [31:0] o_bl_addr,
  output [31:0] o_bl_data,
  output [31:0] o_bl_float,
  output [31:0] o_wl_float,
  output [31:0] o_wl_addr,
  output [31:0] o_wl_data
);

  Neuromorphic_X2_wb_beh #(
    .ADDR_MATCH(ADDR_MATCH),
    .READ_DELAY(READ_DELAY),
    .PROGRAM_DELAY(PROGRAM_DELAY),
    .COMPUTE_DELAY(COMPUTE_DELAY),
    .CONFIG_WRITES(CONFIG_WRITES)
  ) wb_black_box_i (
    .wb_clk_i(wb_clk_i),
    .wb_rst_i(wb_rst_i),
    .wbs_stb_i(wbs_stb_i),
    .wbs_cyc_i(wbs_cyc_i),
    .wbs_we_i(wbs_we_i),
    .wbs_sel_i(wbs_sel_i),
    .wbs_dat_i(wbs_dat_i),
    .wbs_adr_i(wbs_adr_i),
    .wbs_dat_o(wbs_dat_o),
    .wbs_ack_o(wbs_ack_o),
    .i_scan_si1(i_scan_si1),
    .i_scan_se1(i_scan_se1),
    .i_TM(i_TM),
    .o_TM(o_TM),
    .o_mux_sel(o_mux_sel),
    .o_sl_float(o_sl_float),
    .o_sl_addr(o_sl_addr),
    .o_sl_data(o_sl_data),
    .o_bl_addr(o_bl_addr),
    .o_bl_data(o_bl_data),
    .o_bl_float(o_bl_float),
    .o_wl_float(o_wl_float),
    .o_wl_addr(o_wl_addr),
    .o_wl_data(o_wl_data)
  );

endmodule
`endif
