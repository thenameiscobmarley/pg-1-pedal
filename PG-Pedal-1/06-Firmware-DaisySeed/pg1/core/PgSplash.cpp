// PG-1 start-up screen: on pitch black, the pg audio logo is drawn on stroke by stroke while the view
// slowly zooms out; "pg audio" fades in underneath; then both fade away, and the black fades into the
// main screen. Any control skips straight to the fade-in.
#include "PgCore.h"
#include "PgLogo.h"
#include "PgUi.h"
#include <cmath>
#include <cstdio>

namespace pg
{
// timeline (ms)
static constexpr float kDrawEnd = 1700.f, kZoomEnd = 2600.f, kTextIn0 = 900.f, kTextIn1 = 1800.f;
static constexpr float kFadeOut0 = 3000.f, kFadeOut1 = 3600.f, kUiIn0 = 3800.f, kUiIn1 = 4500.f;

static float ta_version(float t) { return Clampf((t - 1200.f) / 800.f, 0.f, 1.f); }

static float Ease(float t)
{
    t = Clampf(t, 0.f, 1.f);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}

// brighten a pixel towards (r, g, b) by `a` (the background is black, so the brighter value wins)
void Core::PxAdd(int x, int y, float a, float r, float g, float b)
{
    if(x < 0 || x >= Canvas::kW || y < 0 || y >= Canvas::kH || a <= 0.f)
        return;
    uint16_t& p  = fb_[y * Canvas::kW + x];
    const int pr = (p >> 11) & 31, pg = (p >> 5) & 63, pb = p & 31;
    const int nr = int(r * a * 31.f + 0.5f), ng = int(g * a * 63.f + 0.5f), nb = int(b * a * 31.f + 0.5f);
    p            = uint16_t(((nr > pr ? nr : pr) << 11) | ((ng > pg ? ng : pg) << 5) | (nb > pb ? nb : pb));
}

// returns true while the start-up screen is still running
bool Core::DrawSplash(uint32_t now)
{
    float t = float(now - splash_t0_);
    if(splash_skip_ && t < kUiIn0) // a control was used: go straight to the fade-in
    {
        splash_t0_ = now - uint32_t(kUiIn0);
        t          = kUiIn0;
    }
    splash_skip_ = false;
    if(now - last_anim_ < 33 && t < kUiIn1)
        return true;
    last_anim_ = now;

    if(t >= kUiIn0) // the main screen comes up out of the black
    {
        if(!splash_ui_drawn_)
        {
            splash_ui_drawn_ = true;
            DrawTitle(now);
            DrawHome(now);
            redraw_title_ = false;
        }
        fade_ = Ease((t - kUiIn0) / (kUiIn1 - kUiIn0));
        Dirty(0, 0, Canvas::kW, Canvas::kH);
        if(t >= kUiIn1)
        {
            fade_      = 1.f;
            splash_on_ = false;
            a_boot_.on = true, a_boot_.t0 = now, a_boot_.ms = 2300; // then the tabs get their pearl outlines
        }
        return true;
    }

    FillRect(0, 0, Canvas::kW, Canvas::kH, 0);
    Dirty(0, 0, Canvas::kW, Canvas::kH);
    if(t >= kFadeOut1)
        return true; // pitch black for a moment

    const float bright = t < kFadeOut0 ? 1.f : 1.f - Ease((t - kFadeOut0) / (kFadeOut1 - kFadeOut0));
    const float zoom   = 2.4f - 1.4f * Ease(t / kZoomEnd);  // close up -> its final size
    const float s      = 5.f * zoom;                          // px per mm
    const float cy_mm  = 0.5f * (logo::kTop + logo::kBottom); // keep the whole mark centred
    auto        X      = [&](float mm) { return 160.f + mm * s; };
    auto        Y      = [&](float mm) { return 112.f - (mm - cy_mm) * s; };

    // the line, drawn on: soft round pen stamps along the curve, up to how far the pen has got
    const float draw = Ease(t / kDrawEnd);
    float       total = 0.f;
    for(int i = 1; i < logo::kPts; i++)
        total += hypotf(logo::kXY[2 * i] - logo::kXY[2 * i - 2], logo::kXY[2 * i + 1] - logo::kXY[2 * i - 1]);
    const float limit = total * draw, r = 0.5f * logo::kStroke * s;
    float       done = 0.f, hx = X(logo::kXY[0]), hy = Y(logo::kXY[1]);
    for(int i = 1; i < logo::kPts && done < limit; i++)
    {
        const float ax = X(logo::kXY[2 * i - 2]), ay = Y(logo::kXY[2 * i - 1]);
        const float bx = X(logo::kXY[2 * i]), by = Y(logo::kXY[2 * i + 1]);
        const float seg_mm = hypotf(logo::kXY[2 * i] - logo::kXY[2 * i - 2], logo::kXY[2 * i + 1] - logo::kXY[2 * i - 1]);
        const float frac   = seg_mm > 0.f ? Clampf((limit - done) / seg_mm, 0.f, 1.f) : 1.f;
        const float ex = ax + (bx - ax) * frac, ey = ay + (by - ay) * frac;
        const float len = hypotf(ex - ax, ey - ay);
        const int   n   = 1 + int(len / 0.6f);
        for(int k = 0; k <= n; k++)
        {
            const float cx = ax + (ex - ax) * float(k) / float(n), cy = ay + (ey - ay) * float(k) / float(n);
            for(int y = int(cy - r - 1.f); y <= int(cy + r + 1.f); y++)
                for(int x = int(cx - r - 1.f); x <= int(cx + r + 1.f); x++)
                {
                    const float d = hypotf(float(x) + 0.5f - cx, float(y) + 0.5f - cy);
                    PxAdd(x, y, Clampf(r + 0.5f - d, 0.f, 1.f) * bright, 1.f, 1.f, 1.f);
                }
        }
        done += seg_mm;
        hx = ex, hy = ey;
    }
    if(draw < 0.999f) // a pearl glint rides on the pen tip while it draws
    {
        float pr, pg, pb;
        const uint16_t c = Pearl(t / 600.f);
        pr = float((c >> 11) & 31) / 31.f, pg = float((c >> 5) & 63) / 63.f, pb = float(c & 31) / 31.f;
        const float gr = r * 2.2f + 1.f;
        for(int y = int(hy - gr - 1.f); y <= int(hy + gr + 1.f); y++)
            for(int x = int(hx - gr - 1.f); x <= int(hx + gr + 1.f); x++)
            {
                const float d = hypotf(float(x) + 0.5f - hx, float(y) + 0.5f - hy) / gr;
                PxAdd(x, y, Clampf(1.f - d, 0.f, 1.f) * Clampf(1.f - d, 0.f, 1.f), pr, pg, pb);
            }
    }

    // model + firmware version, small, at the bottom
    {
        char v[32];
        snprintf(v, sizeof(v), "pg-1  firmware %s", kFirmwareVersion);
        const float a = ta_version(t) * bright;
        const uint16_t c = Canvas::Rgb(uint8_t(150.f * a), uint8_t(150.f * a), uint8_t(165.f * a));
        TextFb((Canvas::kW - TextW(v, Font_6x8)) / 2, 222, v, Font_6x8, c);
    }
    // "pg audio" underneath, fading in, scaled with the same zoom (smooth: bilinear from a 10 px/mm image)
    const float ta = Ease((t - kTextIn0) / (kTextIn1 - kTextIn0)) * bright;
    if(ta > 0.f)
    {
        const float x0 = X(logo::kTextX0), y0 = Y(logo::kTextY1);
        const float k  = logo::kTextPxPerMm / s; // image px per screen px
        const int   w = int(float(logo::kTextW) / k) + 2, h = int(float(logo::kTextH) / k) + 2;
        for(int y = 0; y < h; y++)
            for(int x = 0; x < w; x++)
            {
                const float u = (float(x) + 0.5f) * k - 0.5f, v = (float(y) + 0.5f) * k - 0.5f;
                const int   iu = int(floorf(u)), iv = int(floorf(v));
                const float fu = u - float(iu), fv = v - float(iv);
                auto        at = [](int a, int b) {
                    return (a < 0 || b < 0 || a >= logo::kTextW || b >= logo::kTextH) ? 0.f : float(logo::kText[b * logo::kTextW + a]) / 255.f;
                };
                const float a = (at(iu, iv) * (1.f - fu) + at(iu + 1, iv) * fu) * (1.f - fv) + (at(iu, iv + 1) * (1.f - fu) + at(iu + 1, iv + 1) * fu) * fv;
                PxAdd(int(x0) + x, int(y0) + y, a * ta, 1.f, 0.96f, 0.88f); // a warm cream white
            }
    }
    return true;
}
} // namespace pg
