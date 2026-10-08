#!/usr/bin/env python3
# THE FULL-QUALITY VERSION (OPA1622, +-7.5 V rails, ~4 V rms phones): kept for later. The board being made is carrier.py.
"""PG-1 carrier board: every part, its footprint, its maker part number and its connections (the netlist).
DESIGN.md explains the circuit. make_board.py (needs KiCad's pcbnew) turns this into the board, and the BOM /
placement files PCBWay's assembly service needs.

Nets are plain strings. Per audio channel the same block is built twice (L / R)."""

# Seed3 pin -> what it is (the same map as 05-Wiring-and-Schematics/WIRING.md)
SEED = {1: "KEY", 2: "PG1_A", 3: "PG1_B", 4: "PG1_PUSH", 5: "PG2_A", 6: "PG2_B", 7: "PG2_PUSH", 8: "LCD_CS",
        9: "LCD_SCK", 10: "PGC", 11: "LCD_SDI", 12: "LCD_DC", 13: "LCD_RESET", 14: "PG3_A", 15: "PG3_B",
        16: "SEED_IN_L", 17: "SEED_IN_R", 18: "SEED_OUT_L", 19: "SEED_OUT_R", 20: "AGND", 21: "3V3A",
        22: "PG3_PUSH", 23: "LCD_LED", 24: "PGA", 25: "PGB", 26: "PG4_A", 27: "PG4_B", 28: "PG4_PUSH",
        29: "T_CLK", 30: "T_CS", 31: "T_DIN", 32: "T_DO", 33: "T_IRQ", 34: "SPARE_34", 35: "SPARE_35",
        36: "SPARE_36", 37: "SPARE_37", 38: "3V3D", 39: "VIN", 40: "DGND"}
SCREEN = ["3V3D", "DGND", "LCD_CS", "LCD_RESET", "LCD_DC", "LCD_SDI", "LCD_SCK", "LCD_LED", "NC_SDO",
          "T_CLK", "T_CS", "T_DIN", "T_DO", "T_IRQ"]

parts = []   # (ref, value, footprint, mpn, {pin: net}, note)


def P(ref, value, fp, mpn, pins, note=""):
    parts.append((ref, value, fp, mpn, pins, note))


R0603, C0603, C0805, C1206 = "Resistor_SMD:R_0603_1608Metric", "Capacitor_SMD:C_0603_1608Metric", \
    "Capacitor_SMD:C_0805_2012Metric", "Capacitor_SMD:C_1206_3216Metric"
SOIC8 = "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm"
VSON10 = "Package_SON:VSON-10-1EP_3x3mm_P0.5mm_EP1.2x2mm"
MSOP8EP = "Package_SO:MSOP-8-1EP_3x3mm_P0.65mm_EP1.68x1.88mm"
HDR = lambda n, rows=1: f"Connector_PinHeader_2.54mm:PinHeader_{rows}x{n:02d}_P2.54mm_Vertical"
JACK = "PG1:Jack_6.35mm_TRS_PCB_A-1122"   # custom footprint (made by make_board.py from the jack's drawing)

# ---------------------------------------------------------------- power
P("J9", "9V in", HDR(2), "", {1: "+9V_RAW", 2: "GND"}, "from the panel DC jack (centre negative: centre pin -> GND here)")
P("F1", "PTC 300mA", "Fuse:Fuse_1206_3216Metric", "MF-NSMF030X-2", {1: "+9V_RAW", 2: "+9V_F"})
P("D1", "SS34", "Diode_SMD:D_SMA", "SS34", {1: "+9V", 2: "+9V_F"}, "reverse-polarity protection (cathode = pin 1)")
P("D2", "SMAJ12A", "Diode_SMD:D_SMA", "SMAJ12A", {1: "+9V", 2: "GND"}, "surge clamp")
P("C1", "47uF 25V", "Capacitor_SMD:CP_Elec_6.3x5.8", "EEE-1EA470WP", {1: "+9V", 2: "GND"})
# +7.5 V: TPS7A4901 (adjustable): R2/R1 sets 1.185 V * (1 + R2/R1)
# (datasheet pins: 1 OUT, 2 FB, 3 NC, 4 GND, 5 EN, 6 NR/SS, 7 DNC, 8 IN, pad GND; VFB 1.185 V)
P("U1", "TPS7A4901", MSOP8EP, "TPS7A4901DGNR", {1: "+7V5", 2: "FB_P", 3: "NC", 4: "GND", 5: "+9V", 6: "NR_P", 7: "NC_DNC", 8: "+9V", 9: "GND"})
P("R1", "10k", R0603, "", {1: "FB_P", 2: "GND"})
P("R2", "53.6k", R0603, "", {1: "+7V5", 2: "FB_P"})
P("C2", "10nF", C0603, "", {1: "NR_P", 2: "GND"}, "noise-reduction cap")
P("C3", "10uF 25V", C0805, "", {1: "+7V5", 2: "GND"})
P("C4", "10uF 25V", C0805, "", {1: "+9V", 2: "GND"})
# -8 V raw from the LT1054 charge pump, LC filtered, then -7.5 V from the TPS7A3001
P("U2", "LT1054", SOIC8, "LT1054CS8#PBF", {1: "NC", 2: "CAP+", 3: "GND", 4: "CAP-", 5: "-8V_RAW", 6: "NC", 7: "NC", 8: "+9V"})
P("C5", "10uF 25V", C0805, "", {1: "CAP+", 2: "CAP-"}, "flying cap")
P("C6", "47uF 25V", "Capacitor_SMD:CP_Elec_6.3x5.8", "EEE-1EA470WP", {1: "GND", 2: "-8V_RAW"})
P("L1", "10uH", "Inductor_SMD:L_1210_3225Metric", "", {1: "-8V_RAW", 2: "-8V_F"}, "keeps the 25 kHz switching out of the audio")
P("C7", "22uF 25V", C1206, "", {1: "GND", 2: "-8V_F"})
# (datasheet pins: 1 OUT, 2 FB, 3 NC, 4 GND, 5 EN (tied to IN = on), 6 NR/SS, 7 DNC, 8 IN, pad IN; VFB -1.176 V)
P("U3", "TPS7A3001", MSOP8EP, "TPS7A3001DGNR", {1: "-7V5", 2: "FB_N", 3: "NC", 4: "GND", 5: "-8V_F", 6: "NR_N", 7: "NC_DNC", 8: "-8V_F", 9: "-8V_F"})
P("R3", "10k", R0603, "", {1: "FB_N", 2: "GND"})
P("R4", "53.6k", R0603, "", {1: "-7V5", 2: "FB_N"})
P("C8", "10nF", C0603, "", {1: "NR_N", 2: "GND"})
P("C9", "10uF 25V", C0805, "", {1: "GND", 2: "-7V5"})

# ---------------------------------------------------------------- audio, per channel
OPA1652 = lambda ch_a, ch_b: {1: ch_a[2], 2: ch_a[1], 3: ch_a[0], 4: "-7V5", 5: ch_b[0], 6: ch_b[1], 7: ch_b[2], 8: "+7V5"}  # (+in, -in, out)
for k, ch in enumerate("LR"):
    n = lambda s: f"{s}_{ch}"
    base = 10 + 20 * k
    # input: TVS, 1k, clamps to the rails, 100k bias, 10 uF in, inverting x0.5 (20k in / 10k feedback)
    P(f"R{base}", "1k", R0603, "", {1: n("JIN"), 2: n("IN_P")})
    P(f"D{base+1}", "BAV99", "Package_TO_SOT_SMD:SOT-23", "BAV99", {1: "-7V5", 2: "+7V5", 3: n("IN_P")}, "clamp to the rails")
    P(f"R{base+1}", "100k", R0603, "", {1: n("IN_P"), 2: "GND"})
    P(f"C{base}", "10uF NP", "Capacitor_SMD:CP_Elec_5x5.4", "UUP1E100MCL1GS", {1: n("IN_P"), 2: n("IN_C")}, "non-polar coupling")
    P(f"R{base+2}", "20k", R0603, "", {1: n("IN_C"), 2: n("IN_N")})
    P(f"R{base+3}", "10k", R0603, "", {1: n("IN_N"), 2: n("SEED_IN_RAW")})
    P(f"C{base+1}", "100pF C0G", C0603, "", {1: n("IN_N"), 2: n("SEED_IN_RAW")})
    P(f"R{base+4}", "100", R0603, "", {1: n("SEED_IN_RAW"), 2: f"SEED_IN_{ch}"})
    # output buffer from the Seed3: 10 uF, inverting x1 (10k / 10k)
    P(f"C{base+2}", "10uF NP", "Capacitor_SMD:CP_Elec_5x5.4", "UUP1E100MCL1GS", {1: f"SEED_OUT_{ch}", 2: n("OB_C")})
    P(f"R{base+5}", "100k", R0603, "", {1: f"SEED_OUT_{ch}", 2: "GND"})
    P(f"R{base+6}", "10k", R0603, "", {1: n("OB_C"), 2: n("OB_N")})
    P(f"R{base+7}", "10k", R0603, "", {1: n("OB_N"), 2: n("BUS")})
    P(f"C{base+3}", "100pF C0G", C0603, "", {1: n("OB_N"), 2: n("BUS")})
    # no amp: OPA1622 non-inverting x2 (1k / 1k), 1 ohm out
    P(f"R{base+8}", "1k", R0603, "", {1: n("NA_N"), 2: "GND"})
    P(f"R{base+9}", "1k", R0603, "", {1: n("NA_N"), 2: n("NA_O")})
    P(f"R{base+10}", "1", R0603, "", {1: n("NA_O"), 2: n("JNA")})
    # line out: pg-line pot -> OPA1652 non-inverting x2, 100 ohm out
    P(f"R{base+11}", "10k", R0603, "", {1: n("LO_N"), 2: "GND"})
    P(f"R{base+12}", "10k", R0603, "", {1: n("LO_N"), 2: n("LO_O")})
    P(f"R{base+13}", "100", R0603, "", {1: n("LO_O"), 2: n("JLO")})
    # phones: pg-hp pot -> OPA1622 non-inverting x4 (1k / 3k), 1 ohm out
    P(f"R{base+14}", "1k", R0603, "", {1: n("HP_N"), 2: "GND"})
    P(f"R{base+15}", "3k", R0603, "", {1: n("HP_N"), 2: n("HP_O")})
    P(f"R{base+16}", "1", R0603, "", {1: n("HP_O"), 2: n("JHP")})
# op-amps: U4 = input stages (L, R), U5 = output buffers (L, R), U6 = line drivers (L, R)
P("U4", "OPA1652", SOIC8, "OPA1652AIDR", OPA1652(("GND", "IN_N_L", "SEED_IN_RAW_L"), ("GND", "IN_N_R", "SEED_IN_RAW_R")))
P("U5", "OPA1652", SOIC8, "OPA1652AIDR", OPA1652(("GND", "OB_N_L", "BUS_L"), ("GND", "OB_N_R", "BUS_R")))
P("U6", "OPA1652", SOIC8, "OPA1652AIDR", OPA1652(("LINE_W_L", "LO_N_L", "LO_O_L"), ("LINE_W_R", "LO_N_R", "LO_O_R")))
# OPA1622 (VSON-10, datasheet): 1 +IN A, 2 V+, 3 GND, 4 V-, 5 +IN B, 6 -IN B, 7 OUT B, 8 EN (to its GND pin: > 0.82 V = on),
# 9 OUT A, 10 -IN A, pad V-
OPA1622 = lambda pa, na, oa, pb, nb, ob: {1: pa, 2: "+7V5", 3: "GND", 4: "-7V5", 5: pb, 6: nb, 7: ob, 8: "AMP_EN", 9: oa, 10: na, 11: "-7V5"}
P("U7", "OPA1622", VSON10, "OPA1622IDRCR", OPA1622("BUS_L", "NA_N_L", "NA_O_L", "BUS_R", "NA_N_R", "NA_O_R"), "no amp")
P("U8", "OPA1622", VSON10, "OPA1622IDRCR", OPA1622("HP_W_L", "HP_N_L", "HP_O_L", "HP_W_R", "HP_N_R", "HP_O_R"), "phones")
for i, (net, ref) in enumerate((("+7V5", "C40"), ("-7V5", "C41"), ("+7V5", "C42"), ("-7V5", "C43"), ("+7V5", "C44"),
                                ("-7V5", "C45"), ("+7V5", "C46"), ("-7V5", "C47"), ("+7V5", "C48"), ("-7V5", "C49"))):
    P(ref, "100nF", C0603, "", {1: net, 2: "GND"} if net[0] == "+" else {1: "GND", 2: net}, "decoupling at each op-amp")
# pop-free: the amps switch on ~1 s after +7.5 V is up, off at once when the 9 V goes (TLV7031 comparator)
P("U9", "TLV7031", "Package_TO_SOT_SMD:SOT-23-5", "TLV7031DBVR", {1: "AMP_EN", 2: "GND", 3: "EN_RC", 4: "EN_REF", 5: "+7V5"})
P("R50", "1M", R0603, "", {1: "+7V5", 2: "EN_RC"})
P("C50", "1uF", C0603, "", {1: "EN_RC", 2: "GND"})
P("D50", "BAT54", "Package_TO_SOT_SMD:SOT-23", "BAT54", {1: "EN_RC", 2: "NC", 3: "+9V_SENSE"}, "drains the delay when the 9 V goes")
P("R51", "10k", R0603, "", {1: "+9V", 2: "+9V_SENSE"})
P("R52", "100k", R0603, "", {1: "+7V5", 2: "EN_REF"})
P("R53", "100k", R0603, "", {1: "EN_REF", 2: "GND"})

# ---------------------------------------------------------------- jacks (PCB-mount, their nuts hold the board)
for ref, name, tip, ring in (("J1", "line in", "JIN_L", "JIN_R"), ("J2", "line out", "JLO_L", "JLO_R"),
                             ("J3", "no amp", "JNA_L", "JNA_R"), ("J4", "phones", "JHP_L", "JHP_R")):
    P(ref, name, JACK, "Tayda A-1122", {"T": tip, "R": ring, "S": "AGND"})
    P("D6" + ref[1], "PESD15VL2BT", "Package_TO_SOT_SMD:SOT-23", "PESD15VL2BT", {1: tip, 2: ring, 3: "AGND"},
      "static protection, both lines; 15 V so it never touches the +-7.5 V audio")

# ---------------------------------------------------------------- headers
def seed_net(p):
    return "NC_" + SEED[p] if SEED[p] in ("KEY", "3V3A") else SEED[p]
# the 2x20 header laid out like the Seed3 itself: one row = Seed3 pins 1-20, the other = 40 down to 21 (pin 40 opposite pin 1)
# (KiCad numbers a 2-row header 1, 2 across the first pair, 3, 4 the next ...)
P("J10", "seed port", HDR(20, 2), "", {**{2 * k - 1: seed_net(k) for k in range(1, 21)}, **{2 * k: seed_net(41 - k) for k in range(1, 21)}}, "2x20: 40 female-female jumpers from the Seed3 block")
P("J11", "screen", HDR(14), "", {i + 1: SCREEN[i] for i in range(14)})
for k in range(4):
    a, b, push = f"PG{k+1}_A", f"PG{k+1}_B", f"PG{k+1}_PUSH"
    P(f"J{12+k}", f"pg-{k+1}", HDR(5), "", {1: a, 2: "DGND", 3: b, 4: push, 5: "DGND"}, "encoder: A, C, B, push, push-2")
for k, net in enumerate(("PGA", "PGB", "PGC")):
    P(f"J{16+k}", "pg-" + "abc"[k], HDR(2), "", {1: net, 2: "DGND"}, "footswitch")
P("J19", "pg-hp", HDR(6), "", {1: "BUS_L", 2: "HP_W_L", 3: "AGND", 4: "BUS_R", 5: "HP_W_R", 6: "AGND"}, "dual pot: in, wiper, gnd per gang")
P("J20", "pg-line", HDR(6), "", {1: "BUS_L", 2: "LINE_W_L", 3: "AGND", 4: "BUS_R", 5: "LINE_W_R", 6: "AGND"})
P("J21", "spare", HDR(4), "", {1: "SPARE_34", 2: "SPARE_35", 3: "SPARE_36", 4: "SPARE_37"})
P("J22", "seed power", HDR(2), "", {1: "+9V", 2: "DGND"}, "(also on the seed port: VIN = pin 39)")

# grounds: analog (AGND, the Seed3's pin 20) and the rest (DGND, pin 40) meet at one point on the board (net tie)
P("NT1", "net tie", "NetTie:NetTie-3_SMD_Pad0.5mm", "", {1: "GND", 2: "AGND", 3: "DGND"}, "the only place the grounds join")
NET_ALIASES = {"VIN": "+9V"}   # the Seed3's VIN is the board's protected +9 V


def nets():
    out = {}
    for ref, _, _, _, pins, _ in parts:
        for pin, net in pins.items():
            if net.startswith("NC"):
                continue
            out.setdefault(NET_ALIASES.get(net, net), []).append((ref, str(pin)))
    return out


if __name__ == "__main__":
    ns = nets()
    print(f"{len(parts)} parts, {len(ns)} nets")
    lonely = [n for n, c in ns.items() if len(c) < 2]
    print("nets with only one connection (check):", lonely)
