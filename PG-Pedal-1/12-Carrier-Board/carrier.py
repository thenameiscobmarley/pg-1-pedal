#!/usr/bin/env python3
"""PG-1 carrier board, budget version (NE5532 + TPA6139A2, one 9 V supply): every part, its footprint, its maker part number and its connections (the netlist).
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
JACK = "Connector_Audio:Jack_6.35mm_Neutrik_NMJ6HCD2_Horizontal"   # stocked by JLCPCB (C368502), so they fit it too
VSON10 = MSOP8EP = ""  # (only the full version uses these)

# ---------------------------------------------------------------- power (one 9 V supply)
P("J9", "9V in", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Horizontal", "", {1: "+9V_RAW", 2: "GND"},
  "right-angle, on the parts side with the others; 2 wires from the panel DC jack (centre negative: centre pin -> GND here)")
P("F1", "PTC 300mA", "Fuse:Fuse_1206_3216Metric", "MF-NSMF030X-2", {1: "+9V_RAW", 2: "+9V_F"})
P("D1", "SS34", "Diode_SMD:D_SMA", "SS34", {1: "+9V", 2: "+9V_F"}, "reverse-polarity protection (cathode = pin 1)")
P("D2", "SMAJ12A", "Diode_SMD:D_SMA", "SMAJ12A", {1: "+9V", 2: "GND"}, "surge clamp")
P("C1", "22uF 25V", C1206, "CL31A226KAHNNNE", {1: "+9V", 2: "GND"})
# 4.5 V mid-point for the NE5532s (a stiff divider, only bias resistors hang off it)
P("R1", "10k", R0603, "", {1: "+9V", 2: "VREF"})
P("R2", "10k", R0603, "", {1: "VREF", 2: "GND"})
P("C2", "22uF 25V", C0805, "CL21A226MAQNNNE", {1: "VREF", 2: "GND"})
# 3.3 V for the two headphone-level chips: LP2985 (16 V in, 30 uV noise)
P("U1", "LP2985-33", "Package_TO_SOT_SMD:SOT-23-5", "LP2985-33DBVR", {1: "+9V", 2: "GND", 3: "+9V", 4: "BYP", 5: "+3V3A"},
  "(pins: 1 IN, 2 GND, 3 ON/OFF, 4 BYPASS, 5 OUT)")
P("C3", "10nF", C0603, "", {1: "BYP", 2: "GND"})
P("C4", "4.7uF", C0805, "", {1: "+3V3A", 2: "GND"})
P("C5", "1uF", C0603, "", {1: "+9V", 2: "GND"})

# ---------------------------------------------------------------- audio, per channel
# NE5532 (SOIC-8): 1 OUT A, 2 -IN A, 3 +IN A, 4 V- (GND), 5 +IN B, 6 -IN B, 7 OUT B, 8 V+ (+9 V)
NE5532 = lambda a, b: {1: a[2], 2: a[1], 3: a[0], 4: "GND", 5: b[0], 6: b[1], 7: b[2], 8: "+9V"}  # (+in, -in, out)
for k, ch in enumerate("LR"):
    n = lambda s: f"{s}_{ch}"
    base = 10 + 20 * k
    # input: 1k (with the jack's TVS) -> 10 uF -> inverting x0.5 around 4.5 V (20k in, 10k feedback) -> 10 uF -> Seed3 in
    P(f"R{base}", "1k", R0603, "", {1: n("JIN"), 2: n("IN_A")})
    P(f"R{base+14}", "100k", R0603, "", {1: n("IN_A"), 2: "AGND"}, "keeps the coupling cap charged right (no pop when a cable goes in)")
    P(f"C{base}", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: n("IN_B"), 2: n("IN_A")}, "ceramic X5R: the audio voltage across it is tiny, so it adds no distortion")
    P(f"R{base+1}", "20k", R0603, "", {1: n("IN_B"), 2: n("IN_N")})
    P(f"R{base+2}", "10k", R0603, "", {1: n("IN_N"), 2: n("IN_O")})
    P(f"C{base+1}", "100pF C0G", C0603, "", {1: n("IN_N"), 2: n("IN_O")})
    P(f"C{base+2}", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: n("IN_O"), 2: n("SEED_IN_RAW")})
    P(f"R{base+3}", "100", R0603, "", {1: n("SEED_IN_RAW"), 2: f"SEED_IN_{ch}"})
    P(f"R{base+4}", "100k", R0603, "", {1: n("SEED_IN_RAW"), 2: "AGND"})
    # from the Seed3: 10 uF -> NE5532 follower (biased at 4.5 V) -> 10 uF -> BUS (0 V centred, feeds both pots and no-amp)
    P(f"C{base+3}", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: n("OB_P"), 2: f"SEED_OUT_{ch}"})
    P(f"R{base+5}", "100k", R0603, "", {1: n("OB_P"), 2: "VREF"})
    P(f"R{base+6}", "100k", R0603, "", {1: f"SEED_OUT_{ch}", 2: "AGND"})
    P(f"C{base+4}", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: n("OB_O"), 2: n("BUS")})
    P(f"R{base+7}", "100k", R0603, "", {1: n("BUS"), 2: "AGND"}, "keeps the pots at 0 V (no scratching)")
    # line out: pg-line wiper -> 51k -> NE5532 inverting x2 (100k) -> 10 uF -> 100 ohm -> jack (100k bleed: no plug-in pop)
    P(f"C{base+5}", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: n("LO_A"), 2: n("LINE_W")})
    P(f"R{base+8}", "51k", R0603, "", {1: n("LO_A"), 2: n("LO_N")})
    P(f"R{base+9}", "100k", R0603, "", {1: n("LO_N"), 2: n("LO_O")})
    P(f"C{base+6}", "47pF C0G", C0603, "", {1: n("LO_N"), 2: n("LO_O")})
    P(f"C{base+7}", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: n("LO_O"), 2: n("LO_C")})
    P(f"R{base+10}", "100", R0603, "", {1: n("LO_C"), 2: n("JLO")})
    P(f"R{base+11}", "100k", R0603, "", {1: n("JLO"), 2: "AGND"})
    # the two headphone-level outputs (TPA6139A2, ground-centred, no output caps): 1 uF into each input
    P(f"C{base+8}", "1uF", C0603, "", {1: n("BUS"), 2: n("NA_IN")}, "no amp input")
    P(f"C{base+9}", "1uF", C0603, "", {1: n("HP_W"), 2: n("HP_IN")}, "phones input")
    P(f"R{base+12}", "1", R0603, "", {1: n("NA_O"), 2: n("JNA")})
    P(f"R{base+13}", "1", R0603, "", {1: n("HP_O"), 2: n("JHP")})
P("U4", "NE5532", SOIC8, "NE5532DR", NE5532(("VREF", "IN_N_L", "IN_O_L"), ("VREF", "IN_N_R", "IN_O_R")), "input stages")
P("U5", "NE5532", SOIC8, "NE5532DR", NE5532(("OB_P_L", "OB_O_L", "OB_O_L"), ("OB_P_R", "OB_O_R", "OB_O_R")), "followers from the Seed3")
P("U6", "NE5532", SOIC8, "NE5532DR", NE5532(("VREF", "LO_N_L", "LO_O_L"), ("VREF", "LO_N_R", "LO_O_R")), "line drivers")
# TPA6139A2 (TSSOP-14): 1 -IN_L, 2 OUT_L, 3 GND, 4 MUTE (low = muted), 5 VSS, 6 CN, 7 NC, 8 NC, 9 CP, 10 VDD, 11 GND,
# 12 GAIN (resistor to GND: open = x-2, 11k5 = x-4), 13 OUT_R, 14 -IN_R
TPA = lambda inl, outl, inr, outr, pre: {1: inl, 2: outl, 3: "AGND", 4: "MUTE_N", 5: pre + "VSS", 6: pre + "CN", 9: pre + "CP",
                                         10: "+3V3A", 11: "AGND", 12: pre + "GAIN", 13: outr, 14: inr}
P("U7", "TPA6139A2", "Package_SO:TSSOP-14_4.4x5mm_P0.65mm", "TPA6139A2PWR", {**TPA("NA_IN_L", "NA_O_L", "NA_IN_R", "NA_O_R", "NA_"), 12: "NC_GAIN"},
  "no amp: x-2 (gain pin open) = the same loudness that came in (the input stage was x-0.5)")
P("U8", "TPA6139A2", "Package_SO:TSSOP-14_4.4x5mm_P0.65mm", "TPA6139A2PWR", TPA("HP_IN_L", "HP_O_L", "HP_IN_R", "HP_O_R", "HP_"),
  "phones: x-4 (11k5 on the gain pin): +12 dB over the Seed3 at the top of pg-hp (clean up to ~1 V rms on 50 ohm, ~2 V rms on 200+)")
P("R60", "11.5k 1%", R0603, "", {1: "HP_GAIN", 2: "AGND"})
for pre, cc in (("NA_", "C60"), ("HP_", "C62")):
    P(cc, "1uF", C0603, "", {1: pre + "CP", 2: pre + "CN"}, "charge-pump flying cap")
    P(cc[:-1] + str(int(cc[-1]) + 1), "1uF", C0603, "", {1: "AGND", 2: pre + "VSS"}, "charge-pump hold cap")
for ref in ("C64", "C65"):
    P(ref, "1uF", C0603, "", {1: "+3V3A", 2: "AGND"}, "at each TPA6139A2")
for ref in ("C66", "C67", "C68"):
    P(ref, "100nF", C0603, "", {1: "+9V", 2: "GND"}, "at each NE5532")
# pop-free: both TPA6139A2s stay muted until ~0.5 s after power-up, and mute at once when the 9 V goes
P("U9", "TLV7031", "Package_TO_SOT_SMD:SOT-23-5", "TLV7031DBVR", {1: "MUTE_N", 2: "GND", 3: "EN_RC", 4: "EN_REF", 5: "+3V3A"},
  "(pins: 1 OUT, 2 V-, 3 IN+, 4 IN-, 5 V+)")
P("R50", "470k", R0603, "", {1: "+3V3A", 2: "EN_RC"})
P("C50", "1uF", C0603, "", {1: "EN_RC", 2: "GND"})
P("D50", "1N4148W", "Diode_SMD:D_SOD-123", "1N4148W", {1: "+9V_SENSE", 2: "EN_RC"}, "drains the delay when the 9 V goes")
P("R51", "10k", R0603, "", {1: "+9V", 2: "+9V_SENSE"})
P("R52", "100k", R0603, "", {1: "+3V3A", 2: "EN_REF"})
P("R53", "100k", R0603, "", {1: "EN_REF", 2: "GND"})

# ---------------------------------------------------------------- jacks (PCB-mount, their nuts hold the board)
for ref, name, tip, ring in (("J1", "line in", "JIN_L", "JIN_R"), ("J2", "line out", "JLO_L", "JLO_R"),
                             ("J3", "no amp", "JNA_L", "JNA_R"), ("J4", "phones", "JHP_L", "JHP_R")):
    P(ref, name, JACK, "NMJ6HCD2", {"T": tip, "R": ring, "S": "AGND"})
    P("D6" + ref[1], "PESD15VL2BT", "Package_TO_SOT_SMD:SOT-23", "PESD15VL2BT", {1: tip, 2: ring, 3: "AGND"},
      "static protection, both lines; 15 V so it never touches the +-7.5 V audio")

# ---------------------------------------------------------------- headers
# Right-angle headers on the BOTTOM of the board along its lower edge: the jumpers lie flat between the board and the lid
# (pointing straight up they would hit the back of the screen). Only audio + power go through the board; the screen,
# encoders and footswitches plug straight into the Seed3's jumper block as in v1.
HDR_RA = lambda n: f"Connector_PinHeader_2.54mm:PinHeader_1x{n:02d}_P2.54mm_Horizontal"
P("J10", "seed3 in + power", HDR_RA(5), "", {1: "SEED_IN_L", 2: "SEED_IN_R", 3: "AGND", 4: "+9V", 5: "DGND"},
  "input side: 5 female-female jumpers to the Seed3 block: pins 16, 17, 20 (AGND), 39 (VIN), 40 (DGND)")
P("J11", "seed3 out", HDR_RA(2), "", {1: "SEED_OUT_L", 2: "SEED_OUT_R"}, "output side: 2 jumpers from the Seed3's pins 18, 19")
P("J19", "pg-hp", HDR_RA(6), "", {1: "BUS_L", 2: "HP_W_L", 3: "AGND", 4: "BUS_R", 5: "HP_W_R", 6: "AGND"}, "dual pot: top, wiper, bottom per gang")
P("J20", "pg-line", HDR_RA(6), "", {1: "BUS_L", 2: "LINE_W_L", 3: "AGND", 4: "BUS_R", 5: "LINE_W_R", 6: "AGND"})

# grounds: analog (AGND, the Seed3's pin 20) and the rest (DGND, pin 40) meet at one point on the board (two 0 ohm links)
P("R70", "0", R0603, "", {1: "GND", 2: "AGND"}, "the only place power ground meets audio ground")
P("R71", "0", R0603, "", {1: "DGND", 2: "AGND"}, "the Seed3's digital ground meets audio ground here too (one point)")
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
