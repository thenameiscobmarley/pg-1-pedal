#!/usr/bin/env python3
"""Seed3 load check: runs the pedal's real DSP core (bench.elf, built by build.sh with the firmware's own
flags) on an emulated Cortex-M7 and counts the instructions in the audio callback and in a screen frame.

    ./build.sh && python3 seed3_load.py       (needs: pip install unicorn pyelftools)

The emulator counts instructions, not cycles. The STM32H750 on the Seed3 runs at 480 MHz and issues up to
2 instructions per clock; floating-point code with the caches on runs at about 1 clock per instruction,
with library calls and divides a bit slower. The estimate is shown for 1.0 (typical) and 1.5 (pessimistic)
clocks per instruction. Writes the result to seed3-load.txt (the plugin shows the same numbers)."""
import os, sys, time
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE, UcError
from unicorn.arm_const import UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_CPU_ARM_CORTEX_M7

HERE = os.path.dirname(os.path.abspath(__file__))
F_CPU, FS, BLOCK, FPS = 480e6, 48000, 48, 60
elf = ELFFile(open(os.path.join(HERE, "bench.elf"), "rb"))
sym = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols() if s.name}

mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
try:
    mu.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M7)
except Exception:
    pass
mu.mem_map(0x24000000, 16 << 20)            # program + data (AXI SRAM)
mu.mem_map(0x20000000, 1 << 20)             # stack (DTCM)
try:
    mu.mem_map(0xE0000000, 1 << 20)         # system control space (FPU enable)
except UcError:
    pass
for seg in elf.iter_segments():
    if seg["p_type"] == "PT_LOAD" and seg["p_filesz"]:
        mu.mem_write(seg["p_paddr"], seg.data())
mu.reg_write(UC_ARM_REG_SP, 0x20000000 + (1 << 20) - 64)
for reg, val in ((UC_ARM_REG_C1_C0_2, 0xF << 20), (UC_ARM_REG_FPEXC, 0x40000000)): # FPU on (CPACR CP10/CP11, FPEXC.EN)
    try:
        mu.reg_write(reg, mu.reg_read(reg) | val)
    except UcError:
        pass
mu.reg_write(UC_ARM_REG_LR, sym["mark_done"] | 1)

# the maths library: on the pedal it runs on the H750's double-precision unit; the emulator only has single
# precision, so its software-double code is counted separately and re-costed with typical Cortex-M7 figures
LIBM = {"expf": 40, "exp2f": 35, "logf": 40, "log10f": 45, "log2f": 40, "powf": 80, "sinf": 50, "cosf": 50, "tanf": 70,
        "tanhf": 60, "log1pf": 50, "asinhf": 80, "atan2f": 70, "fmodf": 40, "lroundf": 12, "sqrtf": 14, "floorf": 6, "ceilf": 6}
ranges = []
for s_ in elf.get_section_by_name(".symtab").iter_symbols():
    if s_["st_info"]["type"] == "STT_FUNC" and s_["st_size"] and (s_.name in LIBM or s_.name.startswith("__ieee754") or
            s_.name.startswith("__math") or s_.name.startswith("__kernel") or s_.name.startswith("__aeabi_d") or s_.name.startswith("__aeabi_f2d")
            or s_.name in ("__exp2f_data", "__muldf3", "__adddf3", "__subdf3", "__divdf3", "__extendsfdf2", "__truncdfsf2")):
        a = s_["st_value"] & ~1
        ranges.append((a, a + s_["st_size"], s_.name))
ranges.sort()
entry = {r[0]: r[2] for r in ranges if r[2] in LIBM}
import bisect
starts = [r[0] for r in ranges]
t0 = time.time()
count = {"on": False, "n": 0, "lib": 0, "calls": {}}
def hook(uc, addr, size, user):
    if not count["on"]:
        return
    i = bisect.bisect_right(starts, addr) - 1
    if i >= 0 and addr < ranges[i][1]:
        count["lib"] += 1
        if addr in entry:
            count["calls"][entry[addr]] = count["calls"].get(entry[addr], 0) + 1
    else:
        count["n"] += 1
def stop_at(name):
    return sym[name] & ~1

def run_until(start, name):
    print(f"  [{time.time() - t0:5.0f} s] running to {name}", flush=True)
    try:
        mu.emu_start(start | 1, stop_at(name))
    except UcError as e:
        pc = mu.reg_read(UC_ARM_REG_PC)
        near = max((v for v in sym.items() if v[1] <= pc), key=lambda v: v[1])
        sys.exit(f"emulation stopped: {e} at 0x{pc:08x} ({near[0]}+0x{pc - near[1]:x})")
    return mu.reg_read(UC_ARM_REG_PC)

pc = run_until(sym["_start"], "mark_audio_start")              # init + 300 warm-up blocks, full speed
def counted(name):
    """run to `name` with the instruction counter on (only then: a Python call per instruction is slow).
    The emulator caches translated code, so the cache is flushed for the counter to see every instruction."""
    global pc
    h = mu.hook_add(UC_HOOK_CODE, hook)
    mu.ctl_flush_tb()
    count["on"] = True
    pc = run_until(pc, name)
    count["on"] = False
    mu.hook_del(h)
    mu.ctl_flush_tb()
counted("mark_audio_end")
audio_blocks = 20
def tally(k):
    own, soft = count["n"] / k, count["lib"] / k
    hw = sum(LIBM[f] * n for f, n in count["calls"].items()) / k
    return own + hw, own + soft, own, {f: n / k for f, n in count["calls"].items()}
audio, audio_hi, audio_own, audio_calls = tally(audio_blocks)
pc = run_until(pc, "mark_ui_start")
count.update(n=0, lib=0, calls={})
counted("mark_ui_end")
ui, ui_hi, _, _ = tally(4)

budget = F_CPU * BLOCK / FS                                    # clocks per 48-sample block: 480 000
lines = [f"Seed3 load estimate (emulated Cortex-M7, firmware flags, worst case: every stage on, pid on all, clarity 10 bands)",
         f"audio: {audio:,.0f} instructions per {BLOCK}-sample block ({audio / BLOCK:,.0f} per sample)",
         f"  (core code {audio_own:,.0f}; maths calls per block: " + ", ".join(f"{f} {n:.0f}" for f, n in sorted(audio_calls.items(), key=lambda v: -v[1])[:6]) + ")",
         f"  upper bound with the maths run in software doubles: {100 * audio_hi * 1.5 / (F_CPU * BLOCK / FS):.1f} % at 1.5 clocks/instruction"]
for cpi in (1.0, 1.5):
    lines.append(f"  at {cpi} clocks/instruction: {100 * audio * cpi / budget:.1f} % of the audio time")
lines.append(f"screen: {ui:,.0f} instructions per frame on the visualizer page")
for cpi in (1.0, 1.5):
    left = F_CPU * (1 - audio * cpi / budget)
    lines.append(f"  at {cpi}: room for ~{left / (ui * cpi):.0f} frames/s of drawing (the SPI link's own limit is ~60 fps)")
lines.append(f"(emulation took {time.time() - t0:.0f} s)")
out = "\n".join(lines)
print(out)
open(os.path.join(HERE, "seed3-load.txt"), "w").write(out + "\n")
