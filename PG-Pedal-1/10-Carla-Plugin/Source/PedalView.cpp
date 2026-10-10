#include "PedalView.h"
#include "BinaryData.h"

using namespace juce::gl;
using hwk::gfx::Mat4;
using hwk::gfx::MeshData;
using hwk::gfx::Vec3;
namespace geo = hwk::geo;

/*  World units: 1 = 100 mm. The face's top is y = 0; x across (pedal x), z down the face
    (z = -face y, so the jack edge is at -z, the footswitches at +z). Positions come from
    _Tools/pg_generate.py (the same numbers as the drill template and the UV print). */
namespace dims
{
    constexpr float W = 1.211f, H = 1.455f, R = 0.05f, HB = 0.361f, LID = 0.032f, BEV = 0.006f;
    constexpr float knobX[4] = { -0.405f, -0.135f, 0.135f, 0.405f };
    constexpr float knobZ = 0.185f, knobR = 0.10f;
    constexpr int   kFs = 3;   // fs-1, fs-2, fs-3 (left to right)
    constexpr float fsX[kFs] = { -0.36f, 0.0f, 0.36f };
    constexpr float fsZ = 0.50f;
    constexpr float screws[4][2] = { { -0.3074f, -0.3975f }, { 0.3652f, -0.3975f }, { -0.3074f, -0.0303f }, { 0.3652f, -0.0303f } };
    constexpr float winCZ = -0.2139f, winHW = 0.239f, winHD = 0.1775f;      // the cut window (47.8 x 35.5 mm)
    constexpr float lcdHW = 0.2448f, lcdHD = 0.1836f, lcdY = -0.028f;       // the lit area under it
    constexpr float jackY = -0.1805f;
    constexpr float sideB[2][2] = { { -0.34f, 0 }, { 0.34f, 0 } }; // in, out (the Neutrik jacks on the carrier board)
    constexpr float dcZ = 0.34f;   // the 9 V jack: in the LEFT wall, low (face y -34), between pg-1 and pg-a
    constexpr float smallKnobs[2][2] = { { 0.48f, -0.52f }, { -0.48f, -0.52f } };   // pg-hp (under "out"), pg-line (under "in") (x, z = -face y)
    constexpr float selX = 0.14f, selZ = -0.54f;    // the page selector (A-8233, 4 clicks 45 degrees apart, pink chicken head)
    constexpr float powX = -0.14f, powZ = -0.56f;   // the power switch (the same: position 1 = 0 = off, 2 = 1 = on)
    inline float selAngle (int pos) { return (125.0f - 45.0f * (float) (pos - 1)) * 3.14159265f / 180.0f; }   // face angle of position 1..4
    constexpr float seedSide = 1.f;   // the Seed3 cartridge is in the right wall (-1 = left)
    // the Seed3 cartridge: stands on its edge in a window in the left wall (face y 15.5 mm), parts side out,
    // USB-C toward the footswitches; 4 socket board screws, 29.21 mm beyond the window centre each way, 10.16 mm above and below
    constexpr float seedZ = -0.155f;
    // the expansion bay: a 4.0 x 11.9 mm slot in the LEFT wall (face y 17 mm) holding 4 glued jumper ends (carrier J21)
    constexpr float bayZ = -0.17f;
    constexpr float printX0 = -0.585f, printZ0 = -0.705f, printW = 1.17f, printH = 1.41f;
}

//==============================================================================
void FrameCanvas::Fill (int x, int y, int w, int h, uint16_t c)
{
    const int x0 = juce::jmax (0, x), y0 = juce::jmax (0, y), x1 = juce::jmin (kW, x + w), y1 = juce::jmin (kH, y + h);
    for (int yy = y0; yy < y1; ++yy)
        std::fill (px.begin() + yy * kW + x0, px.begin() + yy * kW + juce::jmax (x0, x1), c);
}

int FrameCanvas::Text (int x, int y, const char* s, const FontDef& f, uint16_t fg, uint16_t bg, int scale)
{
    const int cw = f.FontWidth * scale;
    for (; *s; ++s)
    {
        if (x + cw > kW)
            break;
        pg::RasterChar (*s, f, scale, [&] (int col, int row, bool on)
        {
            const int xx = x + col, yy = y + row;
            if (xx >= 0 && xx < kW && yy >= 0 && yy < kH)
                px[(size_t) (yy * kW + xx)] = on ? fg : bg;
        });
        x += cw;
    }
    return x;
}

void FrameCanvas::Blit (int x, int y, int w, int h, const uint16_t* src, int stride)
{
    for (int row = 0; row < h; ++row)
        if (y + row >= 0 && y + row < kH)
            for (int col = 0; col < w; ++col)
                if (x + col >= 0 && x + col < kW)
                    px[(size_t) ((y + row) * kW + x + col)] = src[row * stride + col];
}

//==============================================================================
// The screen: the pedal's framebuffer, a faint pixel grid, and the cover glass reflecting the room
static const hwk::shaders::Material lcdMaterial { "pgLcd", R"GLSL(
    vec3 px = texture (uTex, vUV).rgb;
    vec2 cell = fract (vUV * vec2 (320.0, 240.0));
    float grid = 0.84 + 0.16 * smoothstep (0.0, 0.16, min (cell.x, cell.y));
    col  = px * grid * 1.08;
    col += envColor (R) * (0.03 + 0.30 * pow (facing, 5.0));
    col += lightCol * pow (ndh, 260.0) * 0.30;
)GLSL", "", true };

PedalView::PedalView (pg::Core& c) : core (c)
{
    setOpaque (true);
    juce::OpenGLPixelFormat pf;
    pf.multisamplingLevel = 4;
    gl.setPixelFormat (pf);
    gl.setMultisamplingEnabled (true);
    gl.setRenderer (this);
    gl.setContinuousRepainting (false);
    gl.attachTo (*this);
    core.PageSelector (selPos.load(), juce::Time::getMillisecondCounter());   // the selector always sits on a position
    core.ForceRedraw();
    startTimerHz (50);
}

PedalView::~PedalView()
{
    stopTimer();
    gl.detach();
}

//==============================================================================
static MeshData faceTop()
{
    using namespace dims;
    const float hw = W * 0.5f - BEV, hd = H * 0.5f - BEV, r = R - BEV;
    MeshData m;
    m.append (geo::plateWithHoles ({ 0.0f, 0.0f, hw - r, hd }, 0.0f, { { 0.0f, winCZ, winHW, winHD } }));
    m.append (geo::plateWithHoles ({ -(hw - r * 0.5f), 0.0f, r * 0.5f, hd - r }, 0.0f, {}));
    m.append (geo::plateWithHoles ({ hw - r * 0.5f, 0.0f, r * 0.5f, hd - r }, 0.0f, {}));
    for (auto [sx, sz] : { std::pair { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } })
    {
        const Vec3 c { sx * (hw - r), 0.0f, sz * (hd - r) };
        const float mid = std::atan2 (-sz, sx);
        const int seg = 10;
        const auto ci = m.addVertex (c, { 0, 1, 0 }, 0, 0);
        for (int i = 0; i < seg; ++i)
        {
            const float a0 = mid - geo::kPi * 0.25f + geo::kPi * 0.5f * (float) i / seg;
            const float a1 = mid - geo::kPi * 0.25f + geo::kPi * 0.5f * (float) (i + 1) / seg;
            const auto p0 = m.addVertex ({ c.x + r * std::cos (a0), 0.0f, c.z - r * std::sin (a0) }, { 0, 1, 0 }, 0, 0);
            const auto p1 = m.addVertex ({ c.x + r * std::cos (a1), 0.0f, c.z - r * std::sin (a1) }, { 0, 1, 0 }, 0, 0);
            m.addTriangle (ci, p0, p1);
        }
    }
    return m;
}

void PedalView::newOpenGLContextCreated()
{
    using namespace dims;
    namespace lib = hwk::shaders::library;
    juce::String err;
    auto make = [&] (const hwk::shaders::Material& mat)
    {
        auto p = std::make_unique<hwk::gfx::ShaderProgram>();
        if (! p->build (hwk::shaders::vertex, hwk::shaders::fragmentSource (mat).toRawUTF8(), err))
            DBG ("PG-1 shader " << mat.name << ": " << err);
        return p;
    };
    progFace = make (lib::lacquerPanel);  progPowder = make (lib::powderCoat); progChrome = make (lib::chrome);
    progPlastic = make (lib::plastic);    progRecess = make (lib::recess);     progWood = make (lib::woodTable);
    progShadow = make (lib::softShadow);  progLcd = make (lcdMaterial);

    meshFace.upload (faceTop());
    meshShell.upload (geo::sweptRoundedRect (W * 0.5f - R, H * 0.5f - R, R, 10, { { 0.0f, -HB }, { 0.0f, -BEV }, { -BEV, 0.0f } }, false));
    meshLid.upload (geo::sweptRoundedRect (W * 0.5f - R, H * 0.5f - R, R, 10, { { -0.002f, -HB - LID }, { -0.002f, -HB } }, true));
    meshWell.upload (geo::wellWalls ({ 0.0f, winCZ, winHW, winHD }, 0.0f, 0.03f));
    meshLcd.upload (geo::horizontalQuad ({ 0.0f, winCZ, lcdHW, lcdHD }, lcdY));
    meshDesk.upload (geo::horizontalQuad ({ 0.0f, 0.4f, 5.0f, 5.0f }, -HB - LID - 0.001f));
    meshShadow.upload (geo::unitQuad());

    meshNutSmall.upload (geo::sweptPolygon (6, 0.046f, { { 0.0f, 0.0f }, { 0.0f, 0.02f } }, true));
    meshNutBig.upload (geo::sweptPolygon (6, 0.082f, { { 0.0f, 0.0f }, { 0.0f, 0.024f } }, true));
    meshThread.upload (geo::lathe (0.06f, { { 0.0f, 0.024f }, { 0.0f, 0.052f } }, 48, true));
    meshPlunger.upload (geo::lathe (0.0435f, { { 0.0f, 0.0f }, { 0.0f, 0.058f }, { -0.008f, 0.066f } }, 48, true));
    meshCap.upload (geo::lathe (0.115f, { { 0.0f, 0.012f }, { 0.0f, 0.102f }, { -0.012f, 0.112f } }, 64, true)); // KN2310 cap, 23 x 10 mm
    meshScrew.upload (geo::lathe (0.0275f, { { 0.0f, 0.0f }, { 0.0f, 0.028f }, { -0.004f, 0.032f } }, 32, true));
    meshJackNut.upload (geo::sweptPolygon (6, 0.07f, { { 0.0f, 0.0f }, { 0.0f, 0.022f } }, true));
    meshJackHole.upload (geo::lathe (0.034f, { { 0.0f, 0.0221f }, { 0.0f, 0.0224f } }, 32, true));
    meshDcNut.upload (geo::sweptPolygon (6, 0.075f, { { 0.0f, 0.0f }, { 0.0f, 0.022f } }, true));
    {   // the chicken head (A-6623): a round skirt + a pointed body, built pointing along +x
        MeshData ch = geo::lathe (0.075f, { { 0.0f, 0.0f }, { 0.0f, 0.03f }, { -0.006f, 0.036f } }, 48, true);
        for (int i = 0; i < 24; ++i)   // the pointed body: narrows (and dips) toward the tip
        {
            const float x0 = -0.045f + 0.0068f * (float) i, x1 = x0 + 0.0068f, t = (float) i / 23.0f;
            const float hw = 0.03f - 0.02f * t, top = 0.082f - 0.016f * t;
            ch.append (geo::box ({ x0, 0.03f, -hw }, { x1, top, hw }));
        }
        meshChicken.upload (ch);
        meshChickenLine.upload (geo::box ({ 0.0f, 0.0745f, -0.003f }, { 0.118f, 0.0755f, 0.003f }));
    }
    // Seed3 cartridge parts, in the wall's frame (x = 0 is the outside of the left wall, -x = further out).
    // Socket = the female ends of the jumper wires, one per used Seed3 pin, in 2 rows. They stick halfway (7 mm)
    // out of the window and are hot-glued around the outside. The Seed3 plugs onto their outer ends:
    // its own black pin spacer (2.5 mm), its 1.6 mm PCB, then its parts.
    constexpr float out = 0.07f;                       // how far the jumper ends stick out of the wall
    meshSeedWin.upload (geo::box ({ -0.0045f, -0.097f, -0.262f }, { -0.001f, 0.097f, 0.262f }));
    meshBayWin.upload (geo::box ({ -0.0045f, -0.020f, -0.0595f }, { -0.001f, 0.020f, 0.0595f }));
    meshBayHdr.upload (geo::box ({ -0.0035f, -0.0125f, -0.051f }, { 0.0f, 0.0125f, 0.051f }));   // the 4 jumper ends, flush
    {
        MeshData hdr;
        for (int pin = 1; pin <= 40; ++pin)
        {
            if (pin == 21 || (pin >= 34 && pin <= 37))
                continue;                              // unused places stay empty
            const float ry = pin <= 20 ? 0.0762f : -0.0762f;               // pins 1-20 = the row nearer the face
            const int pos = pin <= 20 ? pin - 1 : 40 - pin;                // 0 = the USB-C end
            const float z = 0.2413f - 0.0254f * (float) pos;
            hdr.append (geo::box ({ -out, ry - 0.0115f, z - 0.0115f }, { 0.0f, ry + 0.0115f, z + 0.0115f }));
        }
        for (float ry : { -0.0762f, 0.0762f })        // the Seed3's own pin spacer strips
            hdr.append (geo::box ({ -out - 0.025f, ry - 0.0125f, -0.254f }, { -out, ry + 0.0125f, 0.254f }));
        meshSeedHdr.upload (hdr);
    }
    {
        MeshData glue;    // between the rows, and a bead all round where the block leaves the wall
        glue.append (geo::box ({ -out * 0.8f, -0.064f, -0.256f }, { 0.0f, 0.064f, 0.256f }));
        glue.append (geo::box ({ -0.008f, 0.0889f, -0.258f }, { 0.0f, 0.095f, 0.258f }));
        glue.append (geo::box ({ -0.008f, -0.095f, -0.258f }, { 0.0f, -0.0889f, 0.258f }));
        glue.append (geo::box ({ -0.008f, -0.095f, 0.254f }, { 0.0f, 0.095f, 0.260f }));
        glue.append (geo::box ({ -0.008f, -0.095f, -0.260f }, { 0.0f, 0.095f, -0.254f }));
        meshSeedGlue.upload (glue);
    }
    meshSeedPcb.upload (geo::box ({ -out - 0.041f, -0.09f, -0.255f }, { -out - 0.025f, 0.09f, 0.255f }));
    {
        MeshData chips;   // MCU, SDRAM, flash and codec
        chips.append (geo::box ({ -out - 0.056f, -0.05f, -0.06f }, { -out - 0.041f, 0.05f, 0.04f }));
        chips.append (geo::box ({ -out - 0.054f, -0.045f, -0.20f }, { -out - 0.041f, 0.045f, -0.09f }));
        chips.append (geo::box ({ -out - 0.052f, -0.03f, -0.245f }, { -out - 0.041f, 0.03f, -0.21f }));
        meshSeedChips.upload (chips);
    }
    meshSeedUsb.upload (geo::box ({ -out - 0.0735f, -0.0417f, 0.042f }, { -out - 0.041f, 0.0417f, 0.1155f }));
    {
        MeshData btn;     // BOOT and RESET, beside the USB-C end
        for (float by : { -0.065f, 0.065f })
            btn.append (geo::box ({ -out - 0.056f, by - 0.017f, 0.055f }, { -out - 0.041f, by + 0.017f, 0.095f }));
        meshSeedBtn.upload (btn);
    }

    // the knob: HardwareKit's machined aluminium style, tinted like the white knurled A-2850
    knobParts.clear();
    for (auto& part : hwk::models::knob (hwk::models::KnobStyle::aluminium, knobR, { 0.9f, 0.9f, 0.88f }, 2).parts)
    {
        auto k = std::make_unique<KnobPart>();
        k->gpu.upload (part.mesh);
        k->role = part.role; k->rotates = part.rotates; k->colour = part.colour; k->polish = part.polish;
        knobParts.push_back (std::move (k));
    }

    // the UV print: black ink coverage from the artwork's alpha
    if (auto img = juce::ImageFileFormat::loadFrom (BinaryData::faceprint_png, BinaryData::faceprint_pngSize); img.isValid())
    {
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        std::vector<juce::uint8> ink ((size_t) (img.getWidth() * img.getHeight()));
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
                ink[(size_t) (y * img.getWidth() + x)] = bd.getPixelColour (x, y).getAlpha();
        texPrint.upload (ink.data(), img.getWidth(), img.getHeight(), 1, true, 8);
    }
    lcdRgba.assign ((size_t) (pg::Canvas::kW * pg::Canvas::kH * 4), 0);
    texLcd.upload (lcdRgba.data(), pg::Canvas::kW, pg::Canvas::kH, 4, false, 1);
    texLcd.bind (0);   // true pixels, like the real 320 x 240 panel (no smoothing between them)
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    frameDirty = true;
    glReady = true;
}

void PedalView::openGLContextClosing()
{
    glReady = false;
    for (auto* p : { progFace.get(), progPowder.get(), progChrome.get(), progPlastic.get(), progRecess.get(), progWood.get(), progShadow.get(), progLcd.get() })
        if (p) p->release();
    for (auto* m : { &meshFace, &meshShell, &meshLid, &meshWell, &meshLcd, &meshDesk, &meshShadow, &meshNutSmall, &meshNutBig,
                     &meshThread, &meshPlunger, &meshCap, &meshScrew, &meshJackNut, &meshJackHole, &meshDcNut,
                     &meshSeedWin, &meshSeedHdr, &meshSeedGlue, &meshSeedPcb, &meshSeedChips, &meshSeedUsb, &meshSeedBtn, &meshBayWin, &meshBayHdr, &meshChicken, &meshChickenLine })
        m->release();
    for (auto& k : knobParts)
        k->gpu.release();
    knobParts.clear();
    texPrint.release();
    texLcd.release();
}

//==============================================================================
Vec3 PedalView::eye() const
{
    const float d = distance.load(), p = pitch.load(), y = yaw.load();
    const Vec3 target { 0.0f, -0.12f, 0.06f };
    return { target.x + d * std::cos (p) * std::sin (y), target.y + d * std::sin (p), target.z + d * std::cos (p) * std::cos (y) };
}

Mat4 PedalView::viewProj (float w, float h) const
{
    return Mat4::perspective (28.0f * geo::kPi / 180.0f, w / juce::jmax (1.0f, h), 0.05f, 40.0f)
         * Mat4::lookAt (eye(), { 0.0f, -0.12f, 0.06f }, { 0.0f, 1.0f, 0.0f });
}

void PedalView::renderOpenGL()
{
    using namespace dims;
    if (! glReady)
        return;
    const float scale = (float) gl.getRenderingScale();
    const int w = juce::roundToInt (scale * (float) getWidth()), h = juce::roundToInt (scale * (float) getHeight());
    glViewport (0, 0, w, h);
    glClearColor (0.075f, 0.07f, 0.068f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);

    if (frameDirty.exchange (false))
    {
        {
            const juce::SpinLock::ScopedLockType l (frameLock);
            for (size_t i = 0; i < frameCopy.size(); ++i)
            {
                const auto v = frameCopy[i];
                lcdRgba[i * 4 + 0] = (juce::uint8) (((v >> 11) & 31) * 255 / 31);
                lcdRgba[i * 4 + 1] = (juce::uint8) (((v >> 5) & 63) * 255 / 63);
                lcdRgba[i * 4 + 2] = (juce::uint8) ((v & 31) * 255 / 31);
                lcdRgba[i * 4 + 3] = 255;
            }
        }
        texLcd.updateRegion (lcdRgba.data(), 0, 0, pg::Canvas::kW, pg::Canvas::kH);
    }

    const Mat4 vp = viewProj ((float) w, (float) h);
    const Vec3 cam = eye(), light = hwk::gfx::normalise ({ -0.45f, 0.85f, 0.40f });
    const float t = (float) ((juce::Time::getMillisecondCounterHiRes() - startTime) * 0.001);
    const float zeros[15] {};
    auto use = [&] (hwk::gfx::ShaderProgram& p)
    {
        p.use();
        p.set ("uViewProj", vp);   p.set ("uCamPos", cam);   p.set ("uLightDir", light);
        p.set ("uTime", t);        p.set ("uViewport", (float) w, (float) h);   p.set ("uVignette", 0.25f);
        p.set ("uShOn", 0.0f);     p.set ("uOccMapOn", 0.0f); p.set ("uEnvMix", 0.0f);
        p.set ("uCoat", 0.0f);     p.set ("uSmudgeOn", 0.0f); p.set ("uBounceWood", Vec3 { 0, 0, 0 });
        p.setArray ("uWear", zeros, 15);
        p.set ("uEmissive", Vec3 { 0, 0, 0 }); p.set ("uGlow", Vec3 { 0, 0, 0 });
        p.set ("uParams", 0.0f, 0.0f, 0.0f, 0.0f); p.set ("uParams2", 0.0f, 0.0f, 0.0f, 0.0f);
        p.set ("uTex", 0); p.set ("uTex2", 1);
    };
    auto draw = [] (hwk::gfx::ShaderProgram& p, const hwk::gfx::GpuMesh& m, const Mat4& model, Vec3 colour)
    {
        p.set ("uModel", model);
        p.set ("uBaseColor", colour);
        m.draw();
    };
    const Vec3 paint { 0.93f, 0.925f, 0.905f }, steel { 0.86f, 0.87f, 0.89f }, black { 0.028f, 0.028f, 0.032f };
    const Mat4 I = Mat4::identity();
    const Mat4 sideBRot = Mat4::rotationX (-geo::kPi * 0.5f), sideCRot = Mat4::rotationZ (geo::kPi * 0.5f);

    // desk + the pedal's soft shadow on it
    use (*progWood);
    draw (*progWood, meshDesk, I, { 1, 1, 1 });
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (GL_FALSE);
    use (*progShadow);
    progShadow->set ("uParams", 0.78f, 0.92f, 0.62f, 0.75f);
    progShadow->set ("uParams2", 0.06f, 0.10f, 0.55f, 0.0f);
    draw (*progShadow, meshShadow, Mat4::translation ({ 0.02f, -HB - LID - 0.0005f, 0.05f }) * Mat4::scale (0.78f, 1.0f, 0.92f), { 0, 0, 0 });
    glDepthMask (GL_TRUE);
    glDisable (GL_BLEND);

    // white powder-coated box: sides and lid
    use (*progPowder);
    draw (*progPowder, meshShell, I, paint);
    draw (*progPowder, meshLid, I, paint);

    // the face with its UV print (black ink)
    use (*progFace);
    texPrint.bind (0);
    progFace->set ("uParams", printX0, printZ0, printW, printH);
    progFace->set ("uParams2", 0.0f, 1.0f, 0.0f, 0.0f);
    draw (*progFace, meshFace, I, paint);

    // screen window: the cut edge, then the lit screen just under it
    use (*progRecess);
    progRecess->set ("uParams", 0.03f, 0.0f, 0.0f, 0.0f);
    draw (*progRecess, meshWell, I, { 0.55f, 0.55f, 0.53f });
    use (*progLcd);
    texLcd.bind (0);
    draw (*progLcd, meshLcd, I, { 1, 1, 1 });

    // black M3 screws, footswitch / jack nuts and the side ports
    use (*progPlastic);
    for (auto& s : screws)
        draw (*progPlastic, meshScrew, Mat4::translation ({ s[0], 0.0f, s[1] }), black);
    const Mat4 seedAt = Mat4::translation ({ seedSide * W * 0.5f, jackY, seedZ }) * Mat4::scale (-seedSide, 1.f, 1.f); // built for the left wall, mirrored
    draw (*progPlastic, meshSeedHdr, seedAt, black);
    draw (*progPlastic, meshSeedGlue, seedAt, { 0.62f, 0.60f, 0.52f });   // the hot glue
    draw (*progPlastic, meshSeedPcb, seedAt, { 0.035f, 0.04f, 0.045f });
    draw (*progPlastic, meshSeedChips, seedAt, { 0.06f, 0.06f, 0.065f });
    draw (*progPlastic, meshSeedBtn, seedAt, { 0.75f, 0.75f, 0.74f });
    use (*progRecess);
    progRecess->set ("uParams", 0.02f, 0.0f, 0.0f, 0.0f);
    draw (*progRecess, meshSeedWin, seedAt, { 0.05f, 0.05f, 0.06f });
    const Mat4 bayAt = Mat4::translation ({ -seedSide * W * 0.5f, jackY, bayZ }) * Mat4::scale (seedSide, 1.f, 1.f); // the other wall
    draw (*progRecess, meshBayWin, bayAt, { 0.05f, 0.05f, 0.06f });
    draw (*progPlastic, meshBayHdr, bayAt, black);
    for (auto& j : sideB)
        draw (*progRecess, meshJackHole, Mat4::translation ({ j[0], jackY, -H * 0.5f }) * sideBRot, { 0.02f, 0.02f, 0.02f });
    draw (*progRecess, meshJackHole, Mat4::translation ({ -W * 0.5f, jackY, dcZ }) * sideCRot, { 0.02f, 0.02f, 0.02f });

    use (*progChrome);
    progChrome->set ("uParams", 0.65f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; ++i)
        draw (*progChrome, meshNutSmall, Mat4::translation ({ knobX[i], 0.0f, knobZ }), steel);
    for (int i = 0; i < kFs; ++i)
    {
        draw (*progChrome, meshNutBig, Mat4::translation ({ fsX[i], 0.0f, fsZ }), steel);
        draw (*progChrome, meshThread, Mat4::translation ({ fsX[i], 0.0f, fsZ }), steel);
    }
    draw (*progChrome, meshDcNut, Mat4::translation ({ -W * 0.5f, jackY, dcZ }) * sideCRot, steel);   // the DC jack's metal nut
    use (*progPlastic);
    for (auto& j : sideB)
        if (j[1] < 0.5f)
            draw (*progPlastic, meshJackNut, Mat4::translation ({ j[0], jackY, -H * 0.5f }) * sideBRot, { 0.05f, 0.05f, 0.055f });
    use (*progChrome);
    draw (*progChrome, meshSeedUsb, seedAt, { 0.80f, 0.81f, 0.83f });
    for (auto& s : smallKnobs)   // the small pots' nuts
        draw (*progChrome, meshNutSmall, Mat4::translation ({ s[0], 0.0f, s[1] }), steel);
    draw (*progChrome, meshNutSmall, Mat4::translation ({ selX, 0.0f, selZ }), steel);
    draw (*progChrome, meshNutSmall, Mat4::translation ({ powX, 0.0f, powZ }), steel);

    // the four knobs (white knurled aluminium), turned by their encoders
    use (*progPlastic);
    for (int i = 0; i < 4; ++i)
    {
        const Mat4 base = Mat4::translation ({ knobX[i], 0.02f, knobZ });
        const Mat4 turned = base * Mat4::rotationY (-knobAngle[(size_t) i].load());
        for (auto& k : knobParts)
        {
            const Vec3 c = k->role == hwk::models::Role::pointer ? Vec3 { 0.12f, 0.12f, 0.13f }
                         : k->role == hwk::models::Role::metal   ? Vec3 { 0.88f, 0.88f, 0.86f }
                                                                 : Vec3 { 0.90f, 0.90f, 0.88f };
            draw (*progPlastic, k->gpu, k->rotates ? turned : base, c);
        }
    }
    for (int i = 0; i < kFs; ++i)   // the pink anodised KN2310 caps (satin, so the plastic shader)
        draw (*progPlastic, meshCap, Mat4::translation ({ fsX[i], 0.052f - 0.016f * fsTravel[(size_t) i].load(), fsZ }), { 1.0f, 0.50f, 0.76f });
    for (int j = 0; j < 2; ++j)   // pg-hp, pg-line: 14 mm white knobs on the analog pots (set by hand, not by the core)
    {
        const Mat4 base = Mat4::translation ({ smallKnobs[j][0], 0.02f, smallKnobs[j][1] }) * Mat4::scale (0.7f, 0.85f, 0.7f);
        const Mat4 turned = base * Mat4::rotationY (-(smallKnob[(size_t) j].load() - 0.5f) * geo::kPi * 5.0f / 3.0f);
        for (auto& k : knobParts)
            draw (*progPlastic, k->gpu, k->rotates ? turned : base, k->role == hwk::models::Role::pointer ? Vec3 { 0.12f, 0.12f, 0.13f } : Vec3 { 0.93f, 0.93f, 0.91f });
    }
    {   // the pink chicken heads (A-6623): the page selector at its position, power at 0 or 1
        const Mat4 at = Mat4::translation ({ selX, 0.02f, selZ }) * Mat4::rotationY (selAngle (selPos.load()));   // (rotationY (a) turns +x to face angle a: x, -z)
        draw (*progPlastic, meshChicken, at, { 1.0f, 0.55f, 0.78f });
        draw (*progPlastic, meshChickenLine, at, { 0.95f, 0.95f, 0.95f });
        const float pa = (isOn() ? 160.0f : 205.0f) * geo::kPi / 180.0f;   // "0" (position 1) at 205 degrees, "1" at 160
        const Mat4 pw = Mat4::translation ({ powX, 0.02f, powZ }) * Mat4::rotationY (pa);
        draw (*progPlastic, meshChicken, pw, { 1.0f, 0.55f, 0.78f });
        draw (*progPlastic, meshChickenLine, pw, { 0.95f, 0.95f, 0.95f });
    }
}

//==============================================================================
juce::Point<float> PedalView::toPixels (Vec3 p) const
{
    float nx = 0, ny = 0;
    hwk::gfx::projectToNdc (viewProj ((float) getWidth(), (float) getHeight()), p, nx, ny);
    return { (nx * 0.5f + 0.5f) * (float) getWidth(), (0.5f - ny * 0.5f) * (float) getHeight() };
}

bool PedalView::screenPixel (juce::Point<float> m, int& x, int& y, bool clamp) const
{
    using namespace dims;
    const auto p00 = toPixels ({ -lcdHW, lcdY, winCZ - lcdHD });
    const auto p10 = toPixels ({ lcdHW, lcdY, winCZ - lcdHD });
    const auto p01 = toPixels ({ -lcdHW, lcdY, winCZ + lcdHD });
    const auto a = p10 - p00, b = p01 - p00, d = m - p00;
    const float det = a.x * b.y - a.y * b.x;
    if (std::abs (det) < 1.0e-6f)
        return false;
    float u = (d.x * b.y - d.y * b.x) / det, v = (a.x * d.y - a.y * d.x) / det;
    // only the window shows: map to the lit area under it
    const float u0 = (lcdHW - winHW) / (2 * lcdHW), v0 = (lcdHD - winHD) / (2 * lcdHD);
    if (! clamp && (u < u0 || u > 1 - u0 || v < v0 || v > 1 - v0))
        return false;
    u = juce::jlimit (0.0f, 0.999f, u);
    v = juce::jlimit (0.0f, 0.999f, v);
    x = (int) (u * pg::Canvas::kW);
    y = (int) (v * pg::Canvas::kH);
    return true;
}

PedalView::Target PedalView::hitTest (juce::Point<float> m) const
{
    using namespace dims;
    for (int i = 0; i < kFs; ++i)
    {
        const auto c = toPixels ({ fsX[i], 0.08f, fsZ }), e = toPixels ({ fsX[i] + 0.085f, 0.08f, fsZ });
        if (m.getDistanceFrom (c) < c.getDistanceFrom (e))
            return { Hit::footswitch, i };
    }
    for (int i = 0; i < 4; ++i)
    {
        const auto c = toPixels ({ knobX[i], 0.08f, knobZ }), e = toPixels ({ knobX[i] + knobR * 1.15f, 0.08f, knobZ });
        if (m.getDistanceFrom (c) < c.getDistanceFrom (e))
            return { Hit::knob, i };
    }
    for (int j = 0; j < 2; ++j)
    {
        const auto c = toPixels ({ smallKnobs[j][0], 0.06f, smallKnobs[j][1] }), e = toPixels ({ smallKnobs[j][0] + 0.09f, 0.06f, smallKnobs[j][1] });
        if (m.getDistanceFrom (c) < c.getDistanceFrom (e))
            return { Hit::smallKnob, j };
    }
    {
        const auto c = toPixels ({ selX, 0.08f, selZ }), e = toPixels ({ selX + 0.12f, 0.08f, selZ });
        if (m.getDistanceFrom (c) < c.getDistanceFrom (e))
            return { Hit::selector, 0 };
    }
    {
        const auto c = toPixels ({ powX, 0.08f, powZ }), e = toPixels ({ powX + 0.12f, 0.08f, powZ });
        if (m.getDistanceFrom (c) < c.getDistanceFrom (e))
            return { Hit::power, 0 };
    }
    int x, y;
    if (screenPixel (m, x, y, false))
        return { Hit::screen, 0 };
    return {};
}

//==============================================================================
void PedalView::mouseDown (const juce::MouseEvent& e)
{
    const auto now = juce::Time::getMillisecondCounter();
    drag = hitTest (e.position);
    lastMouse = e.position;
    dragAccum = 0.0f;
    dragMoved = false;
    shiftHold = false;
    switch (drag.kind)
    {
        case Hit::knob:
            if (e.mods.isShiftDown() || e.mods.isRightButtonDown())   // shift-drag = hold the knob down + turn: extra-fine
            {
                shiftHold = true;
                core.KnobPress (drag.index, true, now);
            }
            break;
        case Hit::footswitch:
            fsHeld[(size_t) drag.index] = true;
            core.Footswitch (drag.index, true, now);
            break;
        case Hit::screen:
            touching = screenPixel (e.position, touchX, touchY, true);
            core.Touch (true, touchX, touchY, now);
            break;
        case Hit::smallKnob: case Hit::selector: case Hit::power: case Hit::none: break;
    }
}

void PedalView::setSelector (int pos)
{
    pos = juce::jlimit (1, 4, pos);
    if (pos == selPos.load())
        return;
    selPos = pos;
    core.PageSelector (pos, juce::Time::getMillisecondCounter());
    needsRepaint = true;
}

void PedalView::mouseDrag (const juce::MouseEvent& e)
{
    const auto now = juce::Time::getMillisecondCounter();
    const auto d = e.position - lastMouse;
    lastMouse = e.position;
    if (e.getDistanceFromDragStart() > 3)
        dragMoved = true;
    switch (drag.kind)
    {
        case Hit::knob:
        {
            dragAccum += -d.y + d.x * 0.5f;
            const int detents = (int) (dragAccum / 7.0f);
            if (detents != 0)
            {
                dragAccum -= (float) detents * 7.0f;
                core.KnobTurn (drag.index, detents, now);
                knobAngle[(size_t) drag.index] = knobAngle[(size_t) drag.index].load() + (float) detents * geo::kPi / 10.0f;
                needsRepaint = true;
            }
            break;
        }
        case Hit::screen:
            screenPixel (e.position, touchX, touchY, true);
            core.Touch (true, touchX, touchY, now);
            break;
        case Hit::smallKnob:   // up / right = more; the centre click (0 dB) is sticky
        {
            auto& v = smallKnob[(size_t) drag.index];
            float nv = juce::jlimit (0.0f, 1.0f, v.load() + (-d.y + d.x * 0.5f) * 0.004f);
            if (std::abs (nv - 0.5f) < 0.02f)
                nv = 0.5f;
            v = nv;
            needsRepaint = true;
            break;
        }
        case Hit::selector:   // drag round: the position nearest the pointer
        {
            const auto c = toPixels ({ dims::selX, 0.08f, dims::selZ });
            const auto px = toPixels ({ dims::selX + 0.1f, 0.08f, dims::selZ }), pz = toPixels ({ dims::selX, 0.08f, dims::selZ - 0.1f });
            // the mouse in face coordinates (x right, y toward the jacks), through the projected axes
            const auto ax = px - c, ay = pz - c, dm = e.position - c;
            const float det = ax.x * ay.y - ax.y * ay.x;
            if (std::abs (det) > 1.0e-6f && dm.getDistanceFromOrigin() > 4.0f)
            {
                const float fx = (dm.x * ay.y - dm.y * ay.x) / det, fy = (ax.x * dm.y - ax.y * dm.x) / det;
                float deg = std::atan2 (fy, fx) * 180.0f / geo::kPi;
                if (deg < -100.0f)
                    deg += 360.0f;   // position 1 is at 125 degrees, 4 at -10
                setSelector ((int) std::lround ((125.0f - deg) / 45.0f) + 1);
            }
            break;
        }
        case Hit::none:
            yaw = yaw.load() - d.x * 0.008f;
            pitch = juce::jlimit (0.12f, 1.52f, pitch.load() + d.y * 0.006f);
            needsRepaint = true;
            break;
        case Hit::footswitch: break;
    }
}

void PedalView::mouseUp (const juce::MouseEvent& e)
{
    const auto now = juce::Time::getMillisecondCounter();
    switch (drag.kind)
    {
        case Hit::knob:
            if (shiftHold)
                core.KnobPress (drag.index, false, now);
            else if (! dragMoved)   // a click = a push of the knob (two quick clicks = double-push)
            {
                core.KnobPress (drag.index, true, now);
                core.KnobPress (drag.index, false, now);
            }
            break;
        case Hit::footswitch:
            fsHeld[(size_t) drag.index] = false;
            core.Footswitch (drag.index, false, now);
            break;
        case Hit::screen:
            touching = false;
            core.Touch (false, touchX, touchY, now);
            break;
        case Hit::selector:
            if (! dragMoved)   // a click = one click round (4 back to 1: the real switch turns back)
                setSelector (selPos.load() % 4 + 1);
            break;
        case Hit::power:   // a click flips it: 0 <-> 1
            if (power != nullptr)
            {
                power->store (! power->load());
                if (power->load())
                    core.ForceRedraw();   // (it comes back up like the pedal does)
                needsRepaint = true;
            }
            break;
        case Hit::smallKnob:
            if (! dragMoved && e.getNumberOfClicks() > 1)
                smallKnob[(size_t) drag.index] = 0.5f;   // double-click: back to the 0 dB click
            break;
        case Hit::none: break;
    }
    drag = {};
}

void PedalView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (hitTest (e.position).kind == Hit::none ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
}

void PedalView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (hitTest (e.position).kind == Hit::none)
    {
        yaw = 0.0f; pitch = 0.92f; distance = 3.25f;
        needsRepaint = true;
    }
}

void PedalView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto t = hitTest (e.position);
    if (t.kind == Hit::knob)
    {
        static float acc = 0.0f;
        acc += w.deltaY * 14.0f;
        const int detents = (int) acc;
        if (detents != 0)
        {
            acc -= (float) detents;
            core.KnobTurn (t.index, detents, juce::Time::getMillisecondCounter());
            knobAngle[(size_t) t.index] = knobAngle[(size_t) t.index].load() + (float) detents * geo::kPi / 10.0f;
            needsRepaint = true;
        }
        return;
    }
    if (t.kind == Hit::selector)
    {
        static float acc = 0.0f;
        acc += w.deltaY * 4.0f;
        if (std::abs (acc) >= 1.0f)
        {
            setSelector (selPos.load() + (acc < 0 ? 1 : -1));   // wheel down = clockwise
            acc = 0.0f;
        }
        return;
    }
    if (t.kind == Hit::smallKnob)
    {
        auto& v = smallKnob[(size_t) t.index];
        v = juce::jlimit (0.0f, 1.0f, v.load() + w.deltaY * 0.15f);
        needsRepaint = true;
        return;
    }
    distance = juce::jlimit (1.2f, 7.0f, distance.load() * (1.0f - w.deltaY * 0.35f));
    needsRepaint = true;
}

//==============================================================================
void PedalView::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (touching)
        core.Touch (true, touchX, touchY, now);   // keeps long-press timing going while held still
    const bool pushed = core.DrawUi (canvas, now) > 0, backlight = core.BacklightOn() && isOn();
    if (pushed || backlight != lastBacklight)
    {
        lastBacklight = backlight;
        const juce::SpinLock::ScopedLockType l (frameLock);
        frameCopy = canvas.px;
        if (! backlight)   // switched off, or display rest (backlight off after 20 idle minutes)
            frameCopy.fill (0);
        frameDirty = true;
    }
    if (core.WantsFlashMode())
        core.ClearFlashMode();   // (on the pedal this reboots into USB flash mode)
    for (size_t i = 0; i < (size_t) dims::kFs; ++i)
    {
        const float target = fsHeld[i] ? 1.0f : 0.0f, cur = fsTravel[i].load();
        if (std::abs (target - cur) > 0.001f)
        {
            fsTravel[i] = cur + (target - cur) * 0.55f;
            needsRepaint = true;
        }
    }
    if (frameDirty.load() || needsRepaint.exchange (false))
        gl.triggerRepaint();
}
