#include "ili9341.h"

using namespace daisy;

bool Ili9341::Init(const Pins& p, bool flip)
{
    errors_ = 0;
    madctl_ = flip ? 0xE8 : 0x28;
    cs_.Init(p.cs, GPIO::Mode::OUTPUT);
    dc_.Init(p.dc, GPIO::Mode::OUTPUT);
    rst_.Init(p.rst, GPIO::Mode::OUTPUT);
    led_.Init(p.led, GPIO::Mode::OUTPUT);
    cs_.Write(true);
    led_.Write(false);

    SpiHandle::Config c;
    c.periph         = SpiHandle::Config::Peripheral::SPI_1;
    c.mode           = SpiHandle::Config::Mode::MASTER;
    c.direction      = SpiHandle::Config::Direction::TWO_LINES_TX_ONLY;
    c.datasize       = 8;
    c.clock_polarity = SpiHandle::Config::ClockPolarity::LOW;
    c.clock_phase    = SpiHandle::Config::ClockPhase::ONE_EDGE;
    c.nss            = SpiHandle::Config::NSS::SOFT;
    c.baud_prescaler = SpiHandle::Config::BaudPrescaler::PS_4; // ~25 MHz: the ILI9341 takes it fine for writes
    c.pin_config.sclk = p.sck;
    c.pin_config.mosi = p.mosi;
    c.pin_config.miso = Pin();
    c.pin_config.nss  = Pin();
    if(spi_.Init(c) != SpiHandle::Result::OK)
        return false;

    rst_.Write(true);
    System::Delay(5);
    rst_.Write(false);
    System::Delay(20);
    rst_.Write(true);
    System::Delay(150);

    Cmd(0x01); // software reset
    System::Delay(150);
    Cmd(0xCF, {0x00, 0xC1, 0x30});
    Cmd(0xED, {0x64, 0x03, 0x12, 0x81});
    Cmd(0xE8, {0x85, 0x00, 0x78});
    Cmd(0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02});
    Cmd(0xF7, {0x20});
    Cmd(0xEA, {0x00, 0x00});
    Cmd(0xC0, {0x23});       // power control 1
    Cmd(0xC1, {0x10});       // power control 2
    Cmd(0xC5, {0x3E, 0x28}); // VCOM
    Cmd(0xC7, {0x86});
    // MADCTL: MV (landscape) + BGR. 0xE8 is the same rotated 180 degrees.
    Cmd(0x36, {madctl_});
    Cmd(0x3A, {0x55}); // 16 bit colour
    Cmd(0xB1, {0x00, 0x18});
    Cmd(0xB6, {0x08, 0x82, 0x27});
    Cmd(0xF2, {0x00});
    Cmd(0x26, {0x01});
    Cmd(0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
    Cmd(0xE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});
    Cmd(0x11); // sleep out
    System::Delay(120);
    Cmd(0x29); // display on
    Fill(0, 0, kW, kH, 0);
    led_.Write(true);
    return true;
}

void Ili9341::Cmd(uint8_t c)
{
    dc_.Write(false);
    cs_.Write(false);
    spi_.BlockingTransmit(&c, 1);
    cs_.Write(true);
}

void Ili9341::Data(const uint8_t* d, size_t n)
{
    dc_.Write(true);
    cs_.Write(false);
    while(n > 0)
    {
        size_t chunk = n > 0xFFFF ? 0xFFFF : n;
        if(spi_.BlockingTransmit(const_cast<uint8_t*>(d), chunk, 200) != SpiHandle::Result::OK)
            errors_++;
        else if(errors_ > 0 && errors_ < 3)
            errors_ = 0;
        d += chunk;
        n -= chunk;
    }
    cs_.Write(true);
}

void Ili9341::Cmd(uint8_t c, std::initializer_list<uint8_t> d)
{
    Cmd(c);
    uint8_t tmp[16];
    size_t  n = 0;
    for(uint8_t v : d)
        tmp[n++] = v;
    Data(tmp, n);
}

void Ili9341::Window(int x0, int y0, int x1, int y1)
{
    Cmd(0x2A, {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)});
    Cmd(0x2B, {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)});
    Cmd(0x2C);
}

void Ili9341::Fill(int x, int y, int w, int h, uint16_t color)
{
    if(x < 0) { w += x; x = 0; }
    if(y < 0) { h += y; y = 0; }
    if(x + w > kW) w = kW - x;
    if(y + h > kH) h = kH - y;
    if(w <= 0 || h <= 0)
        return;
    Window(x, y, x + w - 1, y + h - 1);
    for(size_t i = 0; i < sizeof(buf_); i += 2)
    {
        buf_[i]     = color >> 8;
        buf_[i + 1] = color & 0xFF;
    }
    size_t total = size_t(w) * size_t(h) * 2;
    while(total > 0)
    {
        size_t n = total > sizeof(buf_) ? sizeof(buf_) : total;
        Data(buf_, n);
        total -= n;
    }
}

int Ili9341::Text(int x, int y, const char* s, const FontDef& f, uint16_t fg, uint16_t bg, int scale)
{
    const int cw = f.FontWidth * scale, ch = f.FontHeight * scale;
    if(size_t(cw * ch * 2) > sizeof(buf_))
        return x;
    for(; *s; s++)
    {
        if(x + cw > kW)
            break;
        char c = (*s < 32 || *s > 126) ? '?' : *s;
        size_t k = 0;
        for(int row = 0; row < ch; row++)
        {
            uint16_t bits = f.data[(c - 32) * f.FontHeight + row / scale];
            for(int col = 0; col < cw; col++)
            {
                uint16_t px = ((bits << (col / scale)) & 0x8000) ? fg : bg;
                buf_[k++]   = px >> 8;
                buf_[k++]   = px & 0xFF;
            }
        }
        Window(x, y, x + cw - 1, y + ch - 1);
        Data(buf_, k);
        x += cw;
    }
    return x;
}

void Ili9341::Refresh()
{
    Cmd(0x3A, {0x55});    // 16 bit colour
    Cmd(0x36, {madctl_}); // rotation
    Cmd(0x13);            // normal display mode (no partial / scroll state left over)
    Cmd(0x20);            // inversion off
    Cmd(0x29);            // display on
}

void Ili9341::Sleep()
{
    led_.Write(false);
    Cmd(0x28); // display off
    Cmd(0x10); // sleep in
    System::Delay(10);
}

// Pushes a block of the core's framebuffer: one address window, then the pixels (big-endian RGB565),
// streamed through the 4 KB buffer.
void Ili9341::Blit(int x, int y, int w, int h, const uint16_t* src, int stride)
{
    // never address outside the panel, whatever comes in (the core clips too)
    if(x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > kW || y + h > kH)
        return;
    Window(x, y, x + w - 1, y + h - 1);
    size_t k = 0;
    for(int row = 0; row < h; row++)
    {
        const uint16_t* p = src + row * stride;
        for(int col = 0; col < w; col++)
        {
            buf_[k++] = uint8_t(p[col] >> 8);
            buf_[k++] = uint8_t(p[col] & 0xFF);
            if(k == sizeof(buf_))
            {
                Data(buf_, k);
                k = 0;
            }
        }
    }
    if(k)
        Data(buf_, k);
}
