#include "isoaudio.h"

using namespace daisy;

namespace pg
{
static constexpr uint8_t kCodecAddr = 0x18;
static constexpr uint32_t kI2cMs = 5;

bool IsoCodec::W(uint8_t page, uint8_t reg, uint8_t v)
{
    if(page != page_)
    {
        uint8_t p[2] = {0x00, page};
        if(i2c_->TransmitBlocking(kCodecAddr, p, 2, kI2cMs) != I2CHandle::Result::OK)
            return page_ = 0xff, false;
        page_ = page;
    }
    uint8_t d[2] = {reg, v};
    return i2c_->TransmitBlocking(kCodecAddr, d, 2, kI2cMs) == I2CHandle::Result::OK;
}

bool IsoCodec::R(uint8_t page, uint8_t reg, uint8_t& v)
{
    if(page != page_ && !W(page, 0x00, page)) // writing the page register selects the page
        return false;
    page_ = page;
    return i2c_->ReadDataAtAddress(kCodecAddr, reg, 1, &v, 1, kI2cMs) == I2CHandle::Result::OK;
}

bool IsoCodec::Alive()
{
    uint8_t v;
    return R(0, 0x04, v); // clock register: any answer will do
}

bool IsoCodec::Init(I2CHandle* i2c)
{
    i2c_  = i2c;
    page_ = 0xff;
    bool ok = W(0, 0x01, 0x01); // software reset
    System::Delay(2);
    page_ = 0xff;
    // clocks: PLL from BCLK, CODEC_CLKIN = PLL; P = 1, R = 4, J = 8, D = 0 -> 98.304 MHz (low PLL range)
    ok = ok && W(0, 0x04, 0x07) && W(0, 0x06, 0x08) && W(0, 0x07, 0x00) && W(0, 0x08, 0x00) && W(0, 0x05, 0x94);
    System::Delay(12); // PLL lock
    ok = ok && W(0, 0x0B, 0x82) && W(0, 0x0C, 0x88) && W(0, 0x0D, 0x00) && W(0, 0x0E, 0x80); // NDAC 2, MDAC 8, DOSR 128
    ok = ok && W(0, 0x12, 0x82) && W(0, 0x13, 0x88) && W(0, 0x14, 0x80);                     // NADC 2, MADC 8, AOSR 128
    ok = ok && W(0, 0x1B, 0x20);                    // I2S, 24-bit words, BCLK and WCLK come in (the Seed3 is master)
    ok = ok && W(0, 0x3C, 0x01) && W(0, 0x3D, 0x01); // processing blocks PRB_P1 / PRB_R1
    // power: single 3.3 V rail
    ok = ok && W(1, 0x01, 0x08); // crude AVDD off (before the AVDD LDO comes on)
    ok = ok && W(1, 0x02, 0x01); // AVDD LDO on (1.72 V), analog blocks on
    ok = ok && W(1, 0x47, 0x32); // analog inputs power up in 6.4 ms
    ok = ok && W(1, 0x7B, 0x01); // reference: 40 ms charge
    ok = ok && W(1, 0x0A, 0x3B); // input CM 0.9 V; line out CM 1.65 V powered from LDOIN (1.8..3.6 V)
    ok = ok && W(1, 0x03, 0x00) && W(1, 0x04, 0x00); // DAC PTM_P3/4 (best performance)
    // the DAC drives the LINE outputs; they feed pg-hp (a real pot) and then the TPA6139A2 headphone amp
    ok = ok && W(1, 0x0E, 0x08) && W(1, 0x0F, 0x08); // left DAC -> LOL, right DAC -> LOR
    ok = ok && W(1, 0x12, 0x00) && W(1, 0x13, 0x00); // LOL / LOR 0 dB, unmuted
    ok = ok && SetInput(path, pga);
    ok = ok && W(1, 0x09, 0x0C); // LOL + LOR on
    ok = ok && W(0, 0x51, 0xC0) && W(0, 0x52, 0x00); // both ADCs on, unmuted
    ok = ok && W(0, 0x3F, 0xD4);                    // both DACs on, left data -> left, right -> right
    ok = ok && W(0, 0x41, 0x00) && W(0, 0x42, 0x00); // DAC digital volume 0 dB
    ok = ok && SetMute(true);                       // the main loop unmutes once the HP drivers have ramped up
    return ok;
}

bool IsoCodec::SetInput(Path p, float pga_db)
{
    path = p;
    pga  = pga_db < 0.f ? 0.f : (pga_db > kPgaMax ? kPgaMax : pga_db);
    const uint8_t g = uint8_t(pga * 2.f + 0.5f) & 0x7F; // 0.5 dB steps, D7 = 0: gain on
    // MICPGA routing: + input = IN1 (mic, 10k) or IN2 (line, 20k); - input = common mode (same resistance)
    const uint8_t lp = p == kMic ? 0x40 : 0x20, ln = p == kMic ? 0x40 : 0x80;
    return W(1, 0x34, lp) && W(1, 0x36, ln) && W(1, 0x37, p == kMic ? 0x40 : 0x20) && W(1, 0x39, ln)
           && W(1, 0x3B, g) && W(1, 0x3C, g);
}

int IsoCodec::Guard()
{
    const uint8_t g     = uint8_t(pga * 2.f + 0.5f) & 0x7F;
    const struct { uint8_t page, reg, want; } k[] = {
        {1, 0x12, 0x00}, {1, 0x13, 0x00}, // LOL / LOR gain 0 dB (they can go to +29 dB)
        {0, 0x41, 0x00}, {0, 0x42, 0x00}, // DAC digital volume 0 dB (can go to +24 dB)
        {1, 0x3B, g},    {1, 0x3C, g},    // input PGA
    };
    int fixed = 0;
    for(const auto& x : k)
    {
        uint8_t v = 0;
        if(R(x.page, x.reg, v) && v != x.want && W(x.page, x.reg, x.want))
            fixed++;
    }
    return fixed;
}

// unmuted, the DAC still mutes itself (in the chip, no software needed) if the digital audio stops: 400 samples of DC
bool IsoCodec::SetMute(bool mute) { return W(0, 0x40, mute ? 0x0C : 0x30); }

bool IsoCodec::ReadOverflow(bool& any)
{
    uint8_t v = 0;
    if(!R(0, 0x2A, v))
        return false;
    any = (v & 0x0C) != 0; // left / right ADC overflow since the last read
    return true;
}

static constexpr uint8_t kFxAddr = 0x2C; // MCP4461, A1 = A0 = 0
// volatile wiper registers 00h 01h 06h 07h, non-volatile 02h 03h 08h 09h (DS22265 table 4-2)
static constexpr uint8_t kLevelReg[2] = {0x00, 0x06}, kCutReg[2] = {0x01, 0x07};

bool AnalogFx::W(uint8_t reg, uint16_t v)
{
    if(v > 256)
        v = 256;
    uint8_t d[2] = {uint8_t((reg << 4) | (v >> 8)), uint8_t(v)}; // command 00 = write, D8 in bit 0
    return i2c_->TransmitBlocking(kFxAddr, d, 2, kI2cMs) == I2CHandle::Result::OK;
}

bool AnalogFx::R(uint8_t reg, uint16_t& v)
{
    uint8_t d[2] = {0, 0};
    if(i2c_->ReadDataAtAddress(kFxAddr, uint16_t((reg << 4) | 0x0C), 1, d, 2, kI2cMs) != I2CHandle::Result::OK)
        return false;
    v = uint16_t(((d[0] & 0x01) << 8) | d[1]);
    return true;
}

bool AnalogFx::Alive()
{
    uint16_t v;
    return R(0x00, v);
}

bool AnalogFx::Init(I2CHandle* i2c)
{
    i2c_ = i2c;
    for(uint8_t reg : {0x02, 0x03, 0x08, 0x09}) // non-volatile: what it powers up at
    {
        uint16_t v = 0;
        if(!R(reg, v))
            return false;
        if(v != 256)
        {
            if(!W(reg, 256))
                return false;
            System::Delay(10); // EEPROM write cycle (tWC 10 ms max)
        }
    }
    return W(0x00, 256) && W(0x01, 256) && W(0x06, 256) && W(0x07, 256);
}

bool AnalogFx::SetLevel(int side, float gain)
{
    gain = gain < 0.f ? 0.f : gain > 1.f ? 1.f : gain;
    return W(kLevelReg[side & 1], uint16_t(gain * 256.f + 0.5f));
}

bool AnalogFx::SetCutoff(int side, float hz)
{
    // R = 1 / (2 pi f C) - wiper (~75 ohm); the rheostat is 10k at code 0, ~0 at 256
    const float r    = hz > 1.f ? 1.f / (6.2831853f * hz * 10e-9f) - 75.f : 1e9f;
    const float code = 256.f - (r < 0.f ? 0.f : r) * (256.f / 10000.f);
    return W(kCutReg[side & 1], uint16_t(code < 0.f ? 0.f : code + 0.5f));
}

// ------------------------------------------------------------------ the analog leveller (MCP4725 U15)
bool AnalogLeveller::Init(I2CHandle* i2c)
{
    i2c_ = i2c;
    for(uint8_t a = 0x60; a <= 0x67; a++)
    {
        uint8_t off[2] = {0x00, 0x00}; // fast write: 0 V (LEDs off = full level)
        if(i2c_->TransmitBlocking(a, off, 2, kI2cMs) == I2CHandle::Result::OK)
        {
            addr_ = a, last_ = 0;
            return true;
        }
    }
    return false;
}

void AnalogLeveller::SetCut(float db)
{
    if(!addr_)
        return;
    db = db < 0.f ? 0.f : (db > 20.f ? 20.f : db);
    // the LDR needed for that cut (4.7k in series, the LDR to ground): R = 4.7k * g / (1 - g)
    const float g  = powf(10.f, -db / 20.f);
    const float r  = db < 0.05f ? 1e7f : 4700.f * g / (1.f - g);
    // a typical LDR pressed on a lit LED: ~2k at 1 mA, R ~ I^-0.75 -> the LED current (8 mA at most: the LEDs' 5 V
    // runs out), then the DAC volts: the NPN's base-emitter drop + I x 100 ohm
    float       ma = powf(2000.f / r, 1.f / 0.75f);
    ma             = ma > 8.f ? 8.f : ma;
    const float v  = db < 0.05f ? 0.f : 0.65f + ma * 0.1f;
    uint16_t    code = uint16_t(v / 3.3f * 4095.f + 0.5f);
    code             = code > 4095 ? 4095 : code;
    if(code == last_)
        return;
    last_       = code;
    uint8_t d[2] = {uint8_t(code >> 8), uint8_t(code)}; // fast write (power-down bits 0)
    i2c_->TransmitBlocking(addr_, d, 2, kI2cMs);
}

} // namespace pg
