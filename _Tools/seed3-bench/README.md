# Seed3 load bench

Runs the pedal's real DSP core on an emulated Cortex-M7 (Unicorn) to see how much of the Seed3's time it needs.

    pip install unicorn pyelftools        # once
    ./build.sh                            # builds bench.elf with the firmware's flags (needs the Arm GNU toolchain)
    python3 seed3_load.py                 # worst case: audio load + a screen frame -> seed3-load.txt
    python3 stages.py                     # each tab's cost -> ../../PG-Pedal-1/10-Carla-Plugin/Source/Seed3Costs.h
    python3 profile.py [stage mask]       # where the time goes, by source line

The emulator counts instructions, not clock cycles (it has no timing model) and has no double-precision unit, so the maths
library's calls are costed with typical Cortex-M7 figures. Treat the result as an estimate (1.0 - 1.5 clocks per instruction);
the health tab on the real pedal measures the true load.
