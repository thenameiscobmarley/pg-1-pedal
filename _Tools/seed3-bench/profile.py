#!/usr/bin/env python3
"""Where the Seed3's audio time goes, by source line (stage mask in argv[1], default: safety only)."""
import os, sys, subprocess, collections
mask = int(sys.argv[1], 0) if len(sys.argv) > 1 else (1 << 6)
sys.argv = ["seed3_load.py"]
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "seed3_load.py")).read()
src = src[:src.index('pc = run_until(sym["_start"]')]
g = {"__name__": "lib", "__file__": __file__}
exec(compile(src, "seed3_load.py", "exec"), g)
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR
from unicorn import UC_HOOK_CODE
mu, sym = g["mu"], g["sym"]
for r, v in ((UC_ARM_REG_R0, 200), (UC_ARM_REG_R1, 10), (UC_ARM_REG_R2, 0), (UC_ARM_REG_R3, mask)):
    mu.reg_write(r, v)
mu.reg_write(UC_ARM_REG_SP, 0x20000000 + (1 << 20) - 64), mu.reg_write(UC_ARM_REG_LR, sym["mark_done"] | 1)
mu.emu_start(sym["bench_main"] | 1, sym["mark_audio_start"] & ~1)
hist = collections.Counter()
h = mu.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: hist.update((a,)))
mu.ctl_flush_tb()
mu.emu_start(sym["mark_audio_start"] | 1, sym["mark_audio_end"] & ~1)
addrs = sorted(hist)
tc = os.path.expanduser("~/.local/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-addr2line")
out = subprocess.run([tc, "-e", "bench.elf", "-i"] + [hex(a) for a in addrs], capture_output=True, text=True).stdout.split("\n")
# with -i each address prints its inline chain; take the first line (the innermost source)
lines, i = collections.Counter(), 0
res = subprocess.run([tc, "-e", "bench.elf"] + [hex(a) for a in addrs], capture_output=True, text=True).stdout.split("\n")
for a, l in zip(addrs, res):
    lines[os.path.basename(l.split(" ")[0])] += hist[a]
total = sum(hist.values())
print(f"total {total / 480:.0f} instructions per sample")
for l, n in lines.most_common(30):
    print(f"{n / 480:7.0f}  {l}")
