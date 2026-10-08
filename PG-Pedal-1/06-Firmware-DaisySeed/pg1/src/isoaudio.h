#pragma once
// The v3 carrier board's isolated audio: a TLV320AIC3204 codec (on the isolated side, reached over I2C through an
// ISO1540 and over SAI2 / I2S through an ISO7741). The levels are NOT set here: both small knobs are analog gain
// stages on the board; the codec runs at one fixed gain (line input, 0 dB).
// Register values follow TI's TLV320AIC3204 Application Reference Guide (SLAA557): clocks from BCLK through the PLL
// (3.072 MHz x 4 x 8 = 98.304 MHz -> 48 kHz with NDAC/NADC 2, MDAC/MADC 8, DOSR/AOSR 128), single 3.3 V supply
// (DVDD LDO by the LDO_SELECT pin, AVDD LDO by register), headphone outputs on LDOIN with a 1.65 V common mode.
#include <cstdint>
#include "daisy_seed.h"

namespace pg
{
class IsoCodec
{
  public:
    enum Path
    {
        kLine, // IN2: through the 1M input buffer (guitar, line, headphone outputs; ~1.9 V rms at 0 dB)
        kMic,  // IN1: straight to the mic preamp (mics, very quiet sources)
    };
    bool Init(daisy::I2CHandle* i2c);            // false: the chip didn't answer
    bool SetInput(Path p, float pga_db);          // 0 .. 47.5 dB, 0.5 dB steps
    bool SetMute(bool mute);                      // the DAC's own soft mute
    bool ReadOverflow(bool& any);                 // the ADC's sticky overflow flags (cleared by reading)
    bool Alive();                                 // answers on I2C
    Path  path = kLine;
    float pga  = 0.f;

  private:
    bool              W(uint8_t page, uint8_t reg, uint8_t v);
    bool              R(uint8_t page, uint8_t reg, uint8_t& v);
    daisy::I2CHandle* i2c_  = nullptr;
    uint8_t           page_ = 0xff;
};

// An optional add-on board in the carrier's fx loop (J22, after the codec, before pg-hp): an MCP4461 quad digital pot on the same isolated
// I2C bus (0x2C). Per side: pot 0 / 2 = attenuator (the "VCA": level / analog compression, can only turn DOWN) and
// pot 1 / 3 = rheostat into 10 nF C0G (analog low-pass, ~1.6 kHz .. fully open). Codes 0..256, 256 = unity / open.
// Its non-volatile wipers hold unity / open, so it powers up clean before the firmware runs.
class AnalogFx
{
  public:
    bool Init(daisy::I2CHandle* i2c);       // unity + open (and stores that in its EEPROM if it isn't already)
    bool SetLevel(int side, float gain);    // side 0 = L, 1 = R; gain 0..1 (1 = unity)
    bool SetCutoff(int side, float hz);     // >= ~200 kHz = fully open
    bool Alive();

  private:
    bool              W(uint8_t reg, uint16_t v);
    bool              R(uint8_t reg, uint16_t& v);
    daisy::I2CHandle* i2c_ = nullptr;
};

} // namespace pg
