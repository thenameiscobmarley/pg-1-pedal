// Minimal ILI9341 driver for the Tayda A-8180 (MSP2402) 2.4" SPI screen on a Daisy Seed3.
// Blocking SPI, called from the main loop only (never from the audio callback).
#pragma once
#include <initializer_list>
#include "daisy_seed.h"
#include "util/oled_fonts.h"
#include "../core/Canvas.h"

class Ili9341 : public pg::Canvas
{
  public:
    struct Pins
    {
        daisy::Pin cs, dc, rst, led, sck, mosi;
    };

    // flip = true turns the picture 180 degrees (use it if the text is upside down).
    // fast = 1/4 of the SPI clock (48 MHz from the 192 MHz PLL1Q set in main), else 1/8 (24 MHz)
    bool Init(const Pins& p, bool flip, bool fast = false);
    void Fill(int x, int y, int w, int h, uint16_t color) override;
    // Draws text with a libDaisy OLED font, scaled up by an integer factor.
    // Returns the x position after the last character.
    int Text(int x, int y, const char* s, const FontDef& f, uint16_t fg, uint16_t bg, int scale = 1) override;
    void Blit(int x, int y, int w, int h, const uint16_t* src, int stride) override;
    void Backlight(bool on) { led_.Write(on); }

    // ---- display safety
    // Re-sends the panel's mode registers (colour format, rotation, normal mode, display on). Harmless
    // to repeat; it pulls the panel back if a static spark or a bad power moment scrambled its state.
    void Refresh();
    // Backlight off, display off, sleep in: the clean way down before a reset or power-off, so the
    // liquid crystal isn't left holding a DC image.
    void Sleep();
    // true when SPI transfers have been failing (the main loop then re-initialises the panel)
    bool Faulted() const { return errors_ >= 3; }

  private:
    void Cmd(uint8_t c);
    void Data(const uint8_t* d, size_t n);
    void Cmd(uint8_t c, std::initializer_list<uint8_t> d);
    void Window(int x0, int y0, int x1, int y1);

    daisy::SpiHandle spi_;
    daisy::GPIO      cs_, dc_, rst_, led_;
    uint8_t          buf_[4096];
    uint8_t          madctl_ = 0x28;
    int              errors_ = 0;
};
