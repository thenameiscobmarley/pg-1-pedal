// PG-1 screen palettes. "pearl" (default) matches the white pedal: a light blue-white frame, bright
// royal-blue boxes with white text, and deep navy behind the graphs so every word and line stands out.
// "bios" is the original BIOS blue on grey.
#include "PgUi.h"
#include <cstdio>

namespace pg
{
namespace ui
{
uint16_t kBlue, kBlueDeep, kGrey, kGreyDark, kWhite, kYellow, kCyan, kLtBlue, kDimText, kGrid, kSpecFill, kPink, kCream;

const char* KnobName(int k)
{
    static char n[4][8];
    k = k < 0 ? 0 : (k > 3 ? 3 : k);
    snprintf(n[k], sizeof(n[k]), "%s%d", kCtrlPrefix, k + 1);
    return n[k];
}
const char* FootName(int f)
{
    static char n[3][8];
    f = f < 0 ? 0 : (f > 2 ? 2 : f);
    snprintf(n[f], sizeof(n[f]), "%s%c", kCtrlPrefix, 'a' + f);
    return n[f];
}

void SetTheme(int theme)
{
    kWhite = Canvas::Rgb(255, 255, 255);
    kPink  = Canvas::Rgb(255, 150, 220);
    kCream = Canvas::Rgb(255, 243, 214);
    if(theme == THEME_BIOS)
    {
        kBlue     = Canvas::Rgb(0, 0, 170);
        kBlueDeep = Canvas::Rgb(0, 0, 128);
        kGrey     = Canvas::Rgb(170, 170, 170);
        kGreyDark = Canvas::Rgb(85, 85, 85);
        kYellow   = Canvas::Rgb(255, 255, 85);
        kCyan     = Canvas::Rgb(85, 255, 255);
        kLtBlue   = Canvas::Rgb(85, 85, 255);
        kDimText  = Canvas::Rgb(120, 120, 200);
        kGrid     = Canvas::Rgb(30, 30, 190);
        kSpecFill = Canvas::Rgb(0, 80, 175);
        return;
    }
    kBlue     = Canvas::Rgb(34, 62, 178);
    kBlueDeep = Canvas::Rgb(12, 22, 74);
    kGrey     = Canvas::Rgb(222, 230, 246);
    kGreyDark = Canvas::Rgb(58, 74, 122);
    kYellow   = Canvas::Rgb(255, 226, 110);
    kCyan     = Canvas::Rgb(120, 226, 255);
    kLtBlue   = Canvas::Rgb(120, 150, 255);
    kDimText  = Canvas::Rgb(168, 184, 232);
    kGrid     = Canvas::Rgb(36, 54, 128);
    kSpecFill = Canvas::Rgb(44, 92, 196);
}
} // namespace ui
} // namespace pg
