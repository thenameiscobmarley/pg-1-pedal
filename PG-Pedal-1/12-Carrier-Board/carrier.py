#!/usr/bin/env python3
"""PG-1 carrier board, v3: ONE isolated stereo input + ONE isolated stereo output (chosen 2026-10-08).

Everything that touches a jack sits on the ISOLATED side, with its own power and ground (IGND): a TLV320AIC3204 audio
chip (mic / guitar / line / headphone-out levels in; headphones or line out), an input buffer, and protection. The
audio crosses to the pedal as digital data only: ISO7741 (I2S) + ISO1540 (I2C), powered across by a B0505S. Nothing
the jacks connect to has a wire path into the pedal, not even ground.

The pedal side: 9 V in (fuse, reverse-polarity, surge), 5 V + 3.3 V and the isolators' pedal halves.

BOTH LEVELS ARE ANALOG: pg-line (input gain) and pg-hp (output gain) are centre-detent dual linear pots that set the
gain of op-amp stages in the signal path, on the isolated side. Centre click = unity ("whatever comes in goes out,
processed"), left = less, right = more. No software sets a level, so a crash can't make it loud.

DESIGN.md explains it. make_board.py turns this into the board, make_jlc.py into JLCPCB's files.
Nets are plain strings. The old 4-jack analog board is carrier_v2.py."""

# Seed3 pin -> what it is (05-Wiring-and-Schematics/WIRING.md). v3: SAI2 on 32-35 + I2C1 on 12/13; the Seed3's own
# audio pins (16-19) are not used.
SEED = {1: "LCD_RESET", 2: "PG1_A", 3: "PG1_B", 4: "PG1_PUSH", 5: "PG2_A", 6: "PG2_B", 7: "PG2_PUSH", 8: "LCD_CS",
        9: "LCD_SCK", 10: "PGC", 11: "LCD_SDI", 12: "SEED_SCL", 13: "SEED_SDA", 14: "PG3_A", 15: "PG3_B",
        16: "NC_AUDIO_IN_1", 17: "NC_AUDIO_IN_2", 18: "NC_AUDIO_OUT_1", 19: "NC_AUDIO_OUT_2", 20: "AGND", 21: "3V3A",
        22: "PG3_PUSH", 23: "LCD_LED", 24: "PGA", 25: "PGB", 26: "PG4_A", 27: "PG4_B", 28: "PG4_PUSH",
        29: "T_CLK", 30: "T_CS", 31: "T_DIN", 32: "SEED_RX", 33: "SEED_TX", 34: "SEED_FS", 35: "SEED_SCK",
        36: "T_DO", 37: "LCD_DC", 38: "3V3D", 39: "VIN", 40: "DGND"}

parts = []   # (ref, value, footprint, mpn, {pin: net}, note)


def P(ref, value, fp, mpn, pins, note=""):
    parts.append((ref, value, fp, mpn, pins, note))


R0603, R1206 = "Resistor_SMD:R_0603_1608Metric", "Resistor_SMD:R_1206_3216Metric"
C0603, C0805, C1206 = "Capacitor_SMD:C_0603_1608Metric", "Capacitor_SMD:C_0805_2012Metric", "Capacitor_SMD:C_1206_3216Metric"
SOIC8 = "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm"
HDR_RA = lambda n: f"Connector_PinHeader_2.54mm:PinHeader_1x{n:02d}_P2.54mm_Horizontal"
JACK = "Connector_Audio:Jack_6.35mm_Neutrik_NMJ6HFD2_Horizontal"   # plastic nose: the sleeve never touches the box

# ================================================================ PEDAL SIDE (ground: GND)
P("J10", "seed3 + 9v", HDR_RA(10), "",
  {1: "+9V_RAW", 2: "GND", 3: "+9V", 4: "GND", 5: "SEED_SCL", 6: "SEED_SDA", 7: "SEED_SCK", 8: "SEED_FS", 9: "SEED_TX", 10: "SEED_RX"},
  "2 wires from the panel DC jack (centre negative: centre -> pin 2), then 8 jumpers to the Seed3: 39 VIN, 40 DGND, "
  "12 SCL, 13 SDA, 35 SAI2 SCK, 34 SAI2 FS, 33 SAI2 SD A (Seed3 out), 32 SAI2 SD B (Seed3 in)")
P("J21", "expansion", HDR_RA(4), "", {1: "+3V3", 2: "GND", 3: "SEED_SCL", 4: "SEED_SDA"},
  "the module bay in the left wall: 3.3 V, ground and the Seed3's I2C (buttons / switches / faders on I2C chips)")
P("F1", "PTC 300mA", "Fuse:Fuse_1206_3216Metric", "1206L030/24NR", {1: "+9V_RAW", 2: "+9V_F"})
P("D1", "B5819W", "Diode_SMD:D_SOD-123", "B5819W", {1: "+9V", 2: "+9V_F"}, "reverse-polarity protection, 1 A (cathode = pin 1)")
P("D2", "PSM712", "Package_TO_SOT_SMD:SOT-23", "PSM712-LF-T7", {1: "+9V", 2: "+9V", 3: "GND"}, "surge clamp (12 V stand-off; D1 keeps reverse off it)")
P("C1", "22uF 25V", C1206, "CL31A226KAHNNNE", {1: "+9V", 2: "GND"})
SOT223 = "Package_TO_SOT_SMD:SOT-223-3_TabPin2"
P("U1", "AMS1117-5.0", SOT223, "AMS1117-5.0", {1: "GND", 2: "+5V", 3: "+9V"}, "5 V for the isolated supply")
P("C2", "22uF 25V", C1206, "CL31A226KAHNNNE", {1: "+5V", 2: "GND"})
P("U2", "XC6206P332MR", "Package_TO_SOT_SMD:SOT-23", "XC6206P332MR-G", {1: "GND", 2: "+3V3", 3: "+5V"},
  "3.3 V for the isolators' pedal halves (~10 mA)")
P("C3", "4.7uF", C0805, "CL21A475KAQNNNE", {1: "+3V3", 2: "GND"})
P("R3", "4.7k", R0603, "", {1: "+3V3", 2: "SEED_SCL"}, "I2C pull-ups, pedal side")
P("R4", "4.7k", R0603, "", {1: "+3V3", 2: "SEED_SDA"})

# ================================================================ THE BARRIER (each part has one half on each side)
P("U3", "B0505S-1WR3", "PG1:DCDC_SIP4_B0505S", "B0505S-1WR3", {1: "GND", 2: "+5V", 3: "IGND", 4: "ISO5V_RAW"},
  "isolated 5 V, 1 W, 1 kV")
P("C7", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: "+5V", 2: "GND"})
P("C8", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: "ISO5V_RAW", 2: "IGND"})
P("U4", "ISO7741", "Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm", "ISO7741DWR",
  {1: "+3V3", 2: "GND", 3: "SEED_SCK", 4: "SEED_FS", 5: "SEED_TX", 6: "SEED_RX", 7: "+3V3", 8: "GND",
   9: "IGND", 10: "ISO3V3", 11: "DOUT_X", 12: "DIN_X", 13: "WCLK_X", 14: "BCLK_X", 15: "IGND", 16: "ISO3V3"},
  "digital audio across: A bit clock, B word clock, C data to the codec; D data back")
P("C9", "100nF", C0603, "", {1: "+3V3", 2: "GND"}, "ISO7741 side 1")
P("C10", "100nF", C0603, "", {1: "ISO3V3", 2: "IGND"}, "ISO7741 side 2")
P("U5", "ISO1540", SOIC8, "ISO1540DR",
  {1: "+3V3", 2: "SEED_SDA", 3: "SEED_SCL", 4: "GND", 5: "IGND", 6: "ISCL", 7: "ISDA", 8: "ISO3V3"}, "I2C across (codec control)")
P("C11", "100nF", C0603, "", {1: "+3V3", 2: "GND"}, "ISO1540 side 1")
P("C12", "100nF", C0603, "", {1: "ISO3V3", 2: "IGND"}, "ISO1540 side 2")

# ================================================================ ISOLATED SIDE (ground: IGND)
P("U7", "XC6206P332MR", "Package_TO_SOT_SMD:SOT-23", "XC6206P332MR-G",
  {1: "IGND", 2: "ISO3V3", 3: "ISO5V_RAW"}, "3.3 V for the codec (its own internal regulator cleans the analog supply)")
P("C14", "4.7uF", C0805, "CL21A475KAQNNNE", {1: "ISO3V3", 2: "IGND"})
P("C15", "1uF", C0603, "", {1: "ISO5V_RAW", 2: "IGND"})
P("R5", "10", R0603, "", {1: "ISO5V_RAW", 2: "ISO5V"}, "filters the converter's ripple off the input buffer's supply")
P("C16", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: "ISO5V", 2: "IGND"})
P("R6", "10k", R0603, "", {1: "ISO3V3", 2: "IBIAS"}, "1.65 V mid-point for the input buffer")
P("R7", "10k", R0603, "", {1: "IBIAS", 2: "IGND"})
P("C17", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: "IBIAS", 2: "IGND"})
# the codec: TLV320AIC3204 (RHB, QFN-32). I2C 0x18, clocks from the bit clock (PLL), internal LDOs for AVDD / DVDD.
P("U9", "TLV320AIC3204", "Package_DFN_QFN:Texas_RHB0032E_VQFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm", "TLV320AIC3204IRHBR",
  {1: "IGND", 2: "BCLK", 3: "WCLK", 4: "DIN", 5: "DOUT", 6: "ISO3V3", 7: "IGND", 8: "IGND", 9: "ISCL", 10: "ISDA",
   12: "IGND", 15: "CIN2_L", 16: "CIN2_R", 17: "IGND", 18: "REF", 24: "AVDD",
   22: "LO_L", 23: "LO_R", 26: "ISO3V3", 28: "IGND", 29: "DVDD", 30: "CRESET", 31: "CRESET", 33: "IGND"},
  "pins: 1 MCLK (unused: PLL from BCLK), 8 MFP3 grounded, 12 SPI_SELECT=0 (I2C), 30 LDO_SELECT high (DVDD LDO on):\n"
  "  joined to the reset pin's pull-up next to it, so both come up together; the firmware then resets it over I2C")
P("C18", "10uF 25V", C0805, "CL21A106KAYNNNE", {1: "REF", 2: "IGND"}, "REF")
P("C19", "1uF", C0603, "", {1: "AVDD", 2: "IGND"})
P("C20", "100nF", C0603, "", {1: "AVDD", 2: "IGND"})
P("C21", "1uF", C0603, "", {1: "DVDD", 2: "IGND"})
P("C22", "100nF", C0603, "", {1: "DVDD", 2: "IGND"})
P("C23", "1uF", C0603, "", {1: "ISO3V3", 2: "IGND"}, "LDOIN / HPVDD")
P("C24", "100nF", C0603, "", {1: "ISO3V3", 2: "IGND"}, "IOVDD")
P("R8", "10k", R0603, "", {1: "ISO3V3", 2: "CRESET"}, "power-on reset (the firmware also resets it over I2C)")
P("C25", "100nF", C0603, "", {1: "CRESET", 2: "IGND"})
P("R9", "4.7k", R0603, "", {1: "ISO3V3", 2: "ISCL"}, "I2C pull-ups, isolated side")
P("R10", "4.7k", R0603, "", {1: "ISO3V3", 2: "ISDA"})
for ref, a, b in (("R11", "BCLK_X", "BCLK"), ("R12", "WCLK_X", "WCLK"), ("R13", "DIN_X", "DIN"), ("R14", "DOUT", "DOUT_X")):
    P(ref, "33", R0603, "", {1: a, 2: b}, "series resistor: clean clock / data edges")
# input buffer: TLV9062 (CMOS input: quiet into the 1M divider, rail-to-rail) on the filtered isolated 5 V
P("U8", "TLV9062", SOIC8, "TLV9062IDR",
  {1: "IN_D_L", 2: "IN_D_L", 3: "IN_C_L", 4: "IGND", 5: "IN_C_R", 6: "IN_D_R", 7: "IN_D_R", 8: "ISO5V"}, "input followers")
P("C26", "100nF", C0603, "", {1: "ISO5V", 2: "IGND"}, "at the TLV9062")
P("U11", "TLV9062", SOIC8, "TLV9062IDR",
  {1: "IN_G_L", 2: "GIN_W_L", 3: "IBIAS", 4: "IGND", 5: "IBIAS", 6: "GIN_W_R", 7: "IN_G_R", 8: "ISO5V"}, "pg-line gain stages")
P("C27", "100nF", C0603, "", {1: "ISO5V", 2: "IGND"}, "at the TLV9062")
P("U12", "TLV9062", SOIC8, "TLV9062IDR",
  {1: "OUT_G_L", 2: "GOUT_W_L", 3: "IBIAS", 4: "IGND", 5: "IBIAS", 6: "GOUT_W_R", 7: "OUT_G_R", 8: "ISO5V"}, "pg-hp gain stages")
P("C28", "100nF", C0603, "", {1: "ISO5V", 2: "IGND"}, "at the TLV9062")
P("J20", "pg-line", HDR_RA(6), "", {1: "GIN_A_L", 2: "GIN_W_L", 3: "GIN_B_L", 4: "GIN_A_R", 5: "GIN_W_R", 6: "GIN_B_R"},
  "isolated side: pg-line, centre-detent dual 10k LINEAR pot, pins 1 / 2 / 3 per gang (left gang, then right)")

# ---------------------------------------------------------------- the input, per channel
# jack -> 1k (1206: survives an amp's speaker output with the TVS) -> IN_A (1M to ground: ~670k input, guitar-friendly)
#   line / guitar / headphone-out path: 47 nF C0G -> 1M / 1M (x0.5 round 1.65 V) -> follower -> 20k / 10k (x1/3) ->
#     1 uF -> codec IN2 (clean up to ~1.9 V rms at the jack)
# then pg-line (analog gain, centre = unity) -> 20k / 10k (x1/3) -> 1 uF -> codec IN2 (fixed gain: no software leveling)
for k, ch in enumerate("LR"):
    n = lambda s: f"{s}_{ch}"
    b = 30 + 10 * k
    P(f"R{b}", "1k", R1206, "", {1: n("JIN"), 2: n("IN_A")}, "1206: takes the current when the TVS clamps a hot input")
    P(f"R{b+1}", "1M", R0603, "", {1: n("IN_A"), 2: "IGND"}, "no pop when a cable goes in")
    P(f"C{b}", "100nF", C0603, "", {1: n("IN_A"), 2: n("IN_B")}, "tiny signal across it at 1M load: no distortion")
    P(f"R{b+2}", "1M", R0603, "", {1: n("IN_B"), 2: n("IN_C")})
    P(f"R{b+3}", "1M", R0603, "", {1: n("IN_C"), 2: "IBIAS"})
    P(f"C{b+1}", "100pF C0G", C0603, "", {1: n("IN_C"), 2: "IBIAS"}, "keeps radio out")
    P(f"R{b+4}", "20k", R0603, "", {1: n("IN_G"), 2: n("IN_E")})
    P(f"R{b+5}", "10k", R0603, "", {1: n("IN_E"), 2: "IGND"})
    P(f"C{b+2}", "1uF", C0603, "", {1: n("IN_E"), 2: n("CIN2")})
    # pg-line: inverting stage, the pot between 220-ohm ends: gain = (220 + R_bw) / (220 + R_aw): -33 dB .. 0 (centre) .. +33 dB
    P(f"R{b+6}", "220", R0603, "", {1: n("IN_D"), 2: n("GIN_A")})
    P(f"R{b+7}", "220", R0603, "", {1: n("GIN_B"), 2: n("IN_G")})
    P(f"C{b+3}", "100pF C0G", C0603, "", {1: n("GIN_W"), 2: n("IN_G")}, "keeps the stage calm with the pot on wires")
# ---------------------------------------------------------------- the output, per channel
# codec line out -> 4.7 uF -> pg-hp stage (inverting, the pot between 1k ends: -21 dB .. 0 (centre) .. +21 dB) ->
# 1 uF -> TPA6139A2 (x-2, ground-centred: no output caps, short-proof, pop-free) -> 1 ohm -> jack
for k, ch in enumerate("LR"):
    n = lambda s: f"{s}_{ch}"
    b = 50 + 10 * k
    P(f"C{b}", "4.7uF", C0805, "CL21A475KAQNNNE", {1: n("FXR"), 2: n("OUT_A")})
    P(f"R{b+1}", "1k", R0603, "", {1: n("OUT_A"), 2: n("GOUT_A")})
    P(f"R{b+2}", "1k", R0603, "", {1: n("GOUT_B"), 2: n("OUT_G")})
    P(f"C{b+2}", "100pF C0G", C0603, "", {1: n("GOUT_W"), 2: n("OUT_G")}, "keeps the stage calm with the pot on wires")
    P(f"C{b+1}", "1uF", C0603, "", {1: n("OUT_G"), 2: n("HPIN")})
    P(f"R{b}", "1", R0603, "", {1: n("HPO"), 2: n("JOUT")})
# ---------------------------------------------------------------- fx loop (for an add-on board later, no new order)
# codec line out -> J22 -> back into the pg-hp stage. Two jumper caps (1-2, 3-4) close the loop; an add-on board
# (e.g. an analog filter / VCA) plugs in instead and gets the isolated 3.3 V, ground and the codec's I2C bus.
P("J22", "fx loop", "Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical", "",
  {1: "LO_L", 2: "FXR_L", 3: "LO_R", 4: "FXR_R", 5: "ISO3V3", 6: "IGND", 7: "ISCL", 8: "ISDA"},
  "isolated side: send l, return l, send r, return r, 3v3, gnd, scl, sda. Jumper caps on 1-2 and 3-4 = normal")
P("J19", "pg-hp", HDR_RA(6), "", {1: "GOUT_A_L", 2: "GOUT_W_L", 3: "GOUT_B_L", 4: "GOUT_A_R", 5: "GOUT_W_R", 6: "GOUT_B_R"},
  "isolated side: pg-hp, centre-detent dual 10k LINEAR pot, pins 1 / 2 / 3 per gang (left gang, then right)")
P("U10", "TPA6139A2", "Package_SO:TSSOP-14_4.4x5mm_P0.65mm", "TPA6139A2PWR",
  {1: "HPIN_L", 2: "HPO_L", 3: "IGND", 4: "HP_ON", 5: "HP_VSS", 6: "HP_CN", 9: "HP_CP", 10: "ISO3V3", 11: "IGND",
   12: "NC_GAIN", 13: "HPO_R", 14: "HPIN_R"}, "headphone / line driver, x-2 (gain pin open): +6 dB at the top of pg-hp")
P("C70", "1uF", C0603, "", {1: "HP_CP", 2: "HP_CN"}, "charge-pump flying cap")
P("C71", "1uF", C0603, "", {1: "IGND", 2: "HP_VSS"}, "charge-pump hold cap")
P("C72", "1uF", C0603, "", {1: "ISO3V3", 2: "IGND"}, "at the TPA6139A2")
P("R70", "470k", R0603, "", {1: "ISO3V3", 2: "HP_ON"}, "stays muted ~0.5 s after power-up: no thump")
P("C73", "1uF", C0603, "", {1: "HP_ON", 2: "IGND"})

# ---------------------------------------------------------------- jacks (PCB-mount, their nuts hold the board)
P("J1", "in", JACK, "NMJ6HFD2", {"T": "JIN_L", "R": "JIN_R", "S": "IGND"}, "isolated input: mic, guitar, line, headphone out")
P("D61", "PSM712", "Package_TO_SOT_SMD:SOT-23", "PSM712-LF-T7", {1: "JIN_L", 2: "JIN_R", 3: "IGND"},
  "static / overvoltage clamp, both lines (+12 / -7 V: never touches a normal signal)")
P("J2", "out", JACK, "NMJ6HFD2", {"T": "JOUT_L", "R": "JOUT_R", "S": "IGND"}, "isolated output: headphones or line")
P("D62", "PSM712", "Package_TO_SOT_SMD:SOT-23", "PSM712-LF-T7", {1: "JOUT_L", 2: "JOUT_R", 3: "IGND"})

# small resistors and capacitors in 0402 (JLCPCB basic parts; the board shrank to clear the box's corner posts),
# except the two values JLCPCB only stocks as basic parts in 0603
R0402, C0402 = "Resistor_SMD:R_0402_1005Metric", "Capacitor_SMD:C_0402_1005Metric"
parts[:] = [(r, v, (R0402 if fp == R0603 and v not in ("470k", "1") else C0402 if fp == C0603 else fp), m, pins, n)
            for r, v, fp, m, pins, n in parts]

NET_ALIASES = {}
ISOLATED = {"HP_ON", "HP_VSS", "HP_CN", "HP_CP", "IGND", "ISO5V_RAW", "ISO5V", "ISO3V3", "IBIAS", "REF", "AVDD", "DVDD", "CRESET", "ISCL", "ISDA",
            "BCLK", "WCLK", "DIN", "DOUT", "BCLK_X", "WCLK_X", "DIN_X", "DOUT_X"} | \
           {f"{s}_{c}" for s in ("JIN", "IN_A", "IN_B", "IN_C", "IN_D", "IN_E", "IN_G", "GIN_A", "GIN_W", "GIN_B", "CIN2", "LO", "FXR",
                                 "OUT_A", "GOUT_A", "GOUT_W", "GOUT_B", "OUT_G", "HPIN", "HPO", "JOUT")
            for c in "LR"}


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
    print(f"{len(parts)} parts, {len(ns)} nets ({len([n for n in ns if n in ISOLATED])} isolated)")
    lonely = [n for n, c in ns.items() if len(c) < 2]
    print("nets with only one connection (check):", lonely)
    # nothing may connect the two sides except the barrier parts
    barrier = {"U3", "U4", "U5"}
    for ref, _, _, _, pins, _ in parts:
        sides = {n in ISOLATED for n in pins.values() if not n.startswith("NC")}
        if len(sides) > 1 and ref not in barrier:
            print("CROSSES THE BARRIER:", ref)
