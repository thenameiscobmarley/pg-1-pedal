#!/bin/bash
# builds bench.elf from the pedal's core with the firmware's own compiler flags
set -e
cd "$(dirname "$0")"
TC=${ARM_TC:-$HOME/.local/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin}
FW=../../PG-Pedal-1/06-Firmware-DaisySeed
# the emulator has no double-precision unit, so the maths library is the single-precision one (see seed3_load.py)
FLAGS="-mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -O2 -g -fdata-sections -ffunction-sections -DPG_BIG_BSS="
"$TC/arm-none-eabi-g++" $FLAGS -std=gnu++14 -fno-exceptions -fno-rtti -I$FW/pg1/core -I$FW/libDaisy/src -c bench.cpp $FW/pg1/core/*.cpp
"$TC/arm-none-eabi-gcc" $FLAGS -I$FW/libDaisy/src -c $FW/libDaisy/src/util/oled_fonts.c
"$TC/arm-none-eabi-g++" $FLAGS -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -T link.ld *.o -lm -o bench.elf
rm -f *.o
"$TC/arm-none-eabi-size" bench.elf
