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
    pga  = pga_db < 0.f ? 0.f : (pga_db > 47.5f ? 47.5f : pga_db);
    const uint8_t g = uint8_t(pga * 2.f + 0.5f) & 0x7F; // 0.5 dB steps, D7 = 0: gain on
    // MICPGA routing: + input = IN1 (mic, 10k) or IN2 (line, 20k); - input = common mode (same resistance)
    const uint8_t lp = p == kMic ? 0x40 : 0x20, ln = p == kMic ? 0x40 : 0x80;
    return W(1, 0x34, lp) && W(1, 0x36, ln) && W(1, 0x37, p == kMic ? 0x40 : 0x20) && W(1, 0x39, ln)
           && W(1, 0x3B, g) && W(1, 0x3C, g);
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

} // namespace pg
