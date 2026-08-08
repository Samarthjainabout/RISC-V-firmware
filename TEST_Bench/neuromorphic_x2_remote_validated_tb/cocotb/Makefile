SIM ?= icarus
TOPLEVEL_LANG = verilog

PWD := $(shell pwd)
REPO_ROOT := $(PWD)/..

VERILOG_SOURCES = $(REPO_ROOT)/Neuromorphic_X2_wb_beh.v
TOPLEVEL = Neuromorphic_X2_wb_beh
MODULE = test_neuromorphic_x2_wb_beh

# Keep simulations short but preserve relative latency behavior.
READ_DELAY ?= 12
PROGRAM_DELAY ?= 16
COMPUTE_DELAY ?= 14
CONFIG_WRITES ?= 3
CLK_PERIOD_NS ?= 10

COMPILE_ARGS += -g2012
COMPILE_ARGS += -P$(TOPLEVEL).READ_DELAY=$(READ_DELAY)
COMPILE_ARGS += -P$(TOPLEVEL).PROGRAM_DELAY=$(PROGRAM_DELAY)
COMPILE_ARGS += -P$(TOPLEVEL).COMPUTE_DELAY=$(COMPUTE_DELAY)
COMPILE_ARGS += -P$(TOPLEVEL).CONFIG_WRITES=$(CONFIG_WRITES)

export PYTHONPATH := $(PWD)/tests
export READ_DELAY PROGRAM_DELAY COMPUTE_DELAY CONFIG_WRITES CLK_PERIOD_NS

include $(shell cocotb-config --makefiles)/Makefile.sim
