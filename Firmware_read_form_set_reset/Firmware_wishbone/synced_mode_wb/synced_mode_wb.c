#include <defs.h>
#include <stub.h>
#include <stdint.h>
#include <stdbool.h>

// ************************************************
//   *** Hardware Test Case: Synced READ/SET/RESET ***
//   ChipIgnite / Caravel RISC-V firmware
//   Wishbone user-area access, GPIO-handshaked with
//   the Teensy DAC/ADC slave (see synced_mode_wb README.md).
//
//   Caravel is the packet master:
//     1. Present MODE0/MODE1 for the upcoming operation.
//     2. Assert CAPTURE_ACTIVE.
//     3. Wait for Teensy's ADC_READY (DAC settled, baseline
//        ADC sample acquired).
//     4. Issue the single Wishbone packet for the operation.
//        The store instruction itself blocks until wbs_ack_o.
//     5. Wait a fixed, operation-specific delay using the same
//        NOP-loop style as the existing working Wishbone firmwares.
//     6. Deassert CAPTURE_ACTIVE.
//     7. Wait for Teensy to drop ADC_READY (DAC zeroed, final
//        ADC sample captured) before starting the next packet.
// ************************************************

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define NEURO_ADDR 0x30000004u

// --------------------------------------------------------
// Sync GPIOs (Caravel <-> Teensy)
//
//   ADC_READY       Teensy -> Caravel   GPIO18   (Teensy pin 5)
//   MODE0           Caravel -> Teensy   GPIO19   (Teensy pin 7)
//   MODE1           Caravel -> Teensy   GPIO20   (Teensy pin 21)
//   CAPTURE_ACTIVE  Caravel -> Teensy   GPIO23   (Teensy pin 18)
//   GND             both                -        common ground
// --------------------------------------------------------
#define GPIO_ADC_READY       18u
#define GPIO_MODE0           19u
#define GPIO_MODE1           20u
#define GPIO_CAPTURE_ACTIVE  23u

#define BIT(n) (1u << (n))

#define MODE_IDLE   0u
#define MODE_READ   1u
#define MODE_SET    2u
#define MODE_RESET  3u

// --------------------------------------------------------
// Configuration packets (sent once, before the timed sequence)
// and the operation packets, matching the existing
// read_mode_wb.c / set_mode_wb.c / reset_mode_wb.c firmware.
// --------------------------------------------------------
#define CONFIG_PACKET_1  0x00036472u
#define CONFIG_PACKET_2  0x462B000Bu
#define CONFIG_PACKET_3  0x44001405u   // no_of_clk_cycles field = 5

#define FORM_PACKET   0x900888FFu
#define READ_PACKET   0x500888FFu
#define SET_PACKET    0xD00888FFu
#define RESET_PACKET  0x100888FFu

// --------------------------------------------------------
// Post-ACK wait times, in wb_clk_i cycles, derived from
// Neuromorphic_X2_wb_beh.v (READ_DELAY=160, PROGRAM_DELAY=220,
// no_of_clk_cycles=5 from CONFIG_PACKET_3) plus a few cycles of
// command-FIFO dispatch margin. For the fabricated chip, replace
// these with the measured/RTL timing; the behavioral-model values
// are a safe starting point, not a proof of silicon timing.
// --------------------------------------------------------
#define READ_POST_ACK_WB_CYCLES   164u
#define SET_POST_ACK_WB_CYCLES    229u
#define RESET_POST_ACK_WB_CYCLES  229u

// Ratio of CPU clock to wb_clk_i. Update if the PLL/CPU divider
// changes. 1 if the management CPU and Wishbone clock are the same.
#define CPU_CYCLES_PER_WB_CYCLE 1u

static uint32_t sync_output_shadow = 0u;

void print_hex32_local(uint32_t value);

static void wait_wb_clock_cycles(uint32_t wb_cycles)
{
    uint32_t cpu_cycles = wb_cycles * CPU_CYCLES_PER_WB_CYCLE;
    for (volatile uint32_t i = 0; i < cpu_cycles; i++) {
        __asm__ volatile ("nop");
    }
}

// --------------------------------------------------------
// Simple CPU delay for non-timing-critical waits (setup only)
// --------------------------------------------------------
static inline void wait_cycles(uint32_t cycles)
{
    for (volatile uint32_t i = 0; i < cycles; i++) {
        __asm__ volatile ("nop");
    }
}

static void write_config_packets(void)
{
    REG32(NEURO_ADDR) = CONFIG_PACKET_1;
    wait_cycles(500);

    REG32(NEURO_ADDR) = CONFIG_PACKET_2;
    wait_cycles(500);

    REG32(NEURO_ADDR) = CONFIG_PACKET_3;
    wait_cycles(500);
}

// --------------------------------------------------------
// Print 32-bit value as hex over UART. Safe for rv32i + nostdlib.
// --------------------------------------------------------
void print_hex32_local(uint32_t value)
{
    char hex[] = "0123456789ABCDEF";
    char buf[11];

    buf[0] = '0';
    buf[1] = 'x';

    for (int i = 0; i < 8; i++) {
        buf[2 + i] = hex[(value >> (28 - 4 * i)) & 0xF];
    }

    buf[10] = '\0';
    print(buf);
}

uint32_t read_wishbone(uint32_t addr)
{
    return REG32(addr);
}

// --------------------------------------------------------
// Configure Caravel IOs for ChipIgnite hardware, plus the
// four sync GPIOs used to handshake with the Teensy.
// --------------------------------------------------------
void configure_io(void)
{
    reg_mprj_io_0 = GPIO_MODE_MGMT_STD_ANALOG;

    // Keep IO[1:4] flash-safe
    reg_mprj_io_1 = GPIO_MODE_MGMT_STD_OUTPUT;
    reg_mprj_io_2 = GPIO_MODE_MGMT_STD_INPUT_NOPULL;
    reg_mprj_io_3 = GPIO_MODE_MGMT_STD_INPUT_NOPULL;
    reg_mprj_io_4 = GPIO_MODE_MGMT_STD_INPUT_NOPULL;

    // UART pins
    reg_mprj_io_5 = GPIO_MODE_MGMT_STD_INPUT_NOPULL;  // UART RX
    reg_mprj_io_6 = GPIO_MODE_MGMT_STD_OUTPUT;        // UART TX

    reg_mprj_io_7  = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_8  = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_9  = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_10 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_11 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_12 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_13 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_14 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_15 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_16 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_17 = GPIO_MODE_USER_STD_INPUT_NOPULL;

    // Teensy -> Caravel
    reg_mprj_io_18 = GPIO_MODE_MGMT_STD_INPUT_NOPULL;   // ADC_READY

    // Caravel -> Teensy
    reg_mprj_io_19 = GPIO_MODE_MGMT_STD_OUTPUT;         // MODE0
    reg_mprj_io_20 = GPIO_MODE_MGMT_STD_OUTPUT;         // MODE1
    reg_mprj_io_21 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_22 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_23 = GPIO_MODE_MGMT_STD_OUTPUT;         // CAPTURE_ACTIVE

    reg_mprj_io_24 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_25 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_26 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_27 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_28 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_29 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_30 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_31 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_32 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_33 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_34 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_35 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_36 = GPIO_MODE_USER_STD_INPUT_NOPULL;
    reg_mprj_io_37 = GPIO_MODE_USER_STD_INPUT_NOPULL;

    // Apply IO configuration
    reg_mprj_xfer = 1;
    while (reg_mprj_xfer == 1);

    sync_output_shadow = 0u;
    reg_mprj_datal = sync_output_shadow;
}

// --------------------------------------------------------
// Drive MODE0/MODE1 and CAPTURE_ACTIVE, read ADC_READY
// --------------------------------------------------------
static void sync_set_mode(uint8_t mode)
{
    sync_output_shadow &= ~(BIT(GPIO_MODE0) | BIT(GPIO_MODE1));

    if (mode & 0x1u) {
        sync_output_shadow |= BIT(GPIO_MODE0);
    }
    if (mode & 0x2u) {
        sync_output_shadow |= BIT(GPIO_MODE1);
    }

    reg_mprj_datal = sync_output_shadow;
}

static void sync_set_capture_active(bool active)
{
    if (active) {
        sync_output_shadow |= BIT(GPIO_CAPTURE_ACTIVE);
    } else {
        sync_output_shadow &= ~BIT(GPIO_CAPTURE_ACTIVE);
    }

    reg_mprj_datal = sync_output_shadow;
}

static bool sync_adc_ready(void)
{
    return (reg_mprj_datal & BIT(GPIO_ADC_READY)) != 0u;
}

// --------------------------------------------------------
// One fully-synchronized packet: mode select, capture-window
// open, single Wishbone write, timed post-ACK wait, then keep
// CAPTURE_ACTIVE high until Teensy drops ADC_READY after its
// fixed-count ADC monitoring window.
// --------------------------------------------------------
static void run_synced_packet(uint8_t mode, uint32_t packet, uint32_t post_ack_wb_cycles, const char *label)
{
    (void)label;

    sync_set_mode(mode);
    wait_cycles(100000);
    sync_set_capture_active(true);

    while (sync_adc_ready()) {
    }

    while (!sync_adc_ready()) {
    }

    // Blocks until wbs_ack_o.
    REG32(NEURO_ADDR) = packet;

    wait_wb_clock_cycles(post_ack_wb_cycles);

    while (sync_adc_ready()) {
    }

    sync_set_capture_active(false);

    sync_set_mode(MODE_IDLE);
}

// --------------------------------------------------------
// Main firmware
// --------------------------------------------------------
void main(void)
{
    // Management GPIO setup
    reg_gpio_mode1 = 1;
    reg_gpio_mode0 = 0;
    reg_gpio_ien   = 1;
    reg_gpio_oe    = 1;
    reg_gpio_out   = 0;

    configure_io();

    reg_uart_enable = 1;

    reg_wb_enable = 1;
    wait_cycles(1000);

    reg_gpio_out = 1;

    // -----------------------------
    // FORM -> READ1 -> SET -> READ2 -> RESET -> READ3, one synchronized
    // ADC/DAC capture window per packet.
    // -----------------------------
    write_config_packets();
    run_synced_packet(MODE_SET,   FORM_PACKET,  SET_POST_ACK_WB_CYCLES,   "FORM");
    write_config_packets();
    run_synced_packet(MODE_READ,  READ_PACKET,  READ_POST_ACK_WB_CYCLES,  "READ1");
    write_config_packets();
    run_synced_packet(MODE_SET,   SET_PACKET,   SET_POST_ACK_WB_CYCLES,   "SET");
    write_config_packets();
    run_synced_packet(MODE_READ,  READ_PACKET,  READ_POST_ACK_WB_CYCLES,  "READ2");
    write_config_packets();
    run_synced_packet(MODE_RESET, RESET_PACKET, RESET_POST_ACK_WB_CYCLES, "RESET");
    write_config_packets();
    run_synced_packet(MODE_READ,  READ_PACKET,  READ_POST_ACK_WB_CYCLES,  "READ3");

    // -----------------------------
    // Final readback for the UART log / python control script
    // -----------------------------
    reg_gpio_out = 0;

    while (1) {
        wait_cycles(100000);
    }
}
