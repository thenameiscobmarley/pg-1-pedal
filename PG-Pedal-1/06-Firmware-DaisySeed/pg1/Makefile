# PG-1 firmware. libDaisy and DaisySP are cloned next to this folder (see ../README.md).
TARGET = pg1

CPP_SOURCES = src/main.cpp src/ili9341.cpp core/PgCore.cpp core/PgPages.cpp core/PgHealth.cpp core/PgSplash.cpp core/PgLearn.cpp core/PgState.cpp core/PgUi.cpp

# Runs from the Daisy bootloader: the program sits in the 8 MB QSPI flash and is copied into the
# 480 KB SRAM at power-on (same speed as internal flash, ~4x the room). One-time setup: README.md.
# For the old way (128 KB internal flash, no bootloader): make APP_TYPE=BOOT_NONE
APP_TYPE ?= BOOT_SRAM

# the core's big screen-only scratch buffers go to SDRAM (see PG_BIG_BSS in core/PgCore.h)
C_DEFS += "-DPG_BIG_BSS=__attribute__((section(\".sdram_bss\")))"

LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
