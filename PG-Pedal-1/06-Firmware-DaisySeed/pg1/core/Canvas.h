// A 320 x 240 RGB565 display. The pedal's ILI9341 driver implements it on the Seed3; the Carla plugin
// implements it on a framebuffer that becomes the 3D screen's texture. The core draws into its own
// framebuffer and pushes only the rectangles that changed with Blit() - exactly what the Seed3 sends
// over SPI, so what the plugin shows is what the real screen can show.
#pragma once
#include <cstdint>
#include "util/oled_fonts.h"

namespace pg
{
class Canvas
{
  public:
    static constexpr int kW = 320, kH = 240; // landscape

    virtual ~Canvas() = default;
    virtual void Fill(int x, int y, int w, int h, uint16_t color) = 0;
    // Draws text with a libDaisy OLED font, scaled up by an integer factor. Returns the x after the text.
    virtual int Text(int x, int y, const char* s, const FontDef& f, uint16_t fg, uint16_t bg, int scale = 1) = 0;
    // Copies a w x h block of pixels (rows `stride` pixels apart) to (x, y).
    virtual void Blit(int x, int y, int w, int h, const uint16_t* src, int stride) = 0;

    static constexpr uint16_t Rgb(uint8_t r, uint8_t g, uint8_t b)
    {
        return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
};

// Shared glyph rasteriser: calls `put` for every pixel of one character cell.
template <typename PutFn>
inline void RasterChar(char c, const FontDef& f, int scale, PutFn&& put)
{
    if(c < 32 || c > 126)
        c = '?';
    const int cw = f.FontWidth * scale, ch = f.FontHeight * scale;
    for(int row = 0; row < ch; row++)
    {
        const uint16_t bits = f.data[(c - 32) * f.FontHeight + row / scale];
        for(int col = 0; col < cw; col++)
            put(col, row, ((bits << (col / scale)) & 0x8000) != 0);
    }
}
} // namespace pg
