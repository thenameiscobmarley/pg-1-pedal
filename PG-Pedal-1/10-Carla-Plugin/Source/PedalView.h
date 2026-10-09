#pragma once
#include <juce_opengl/juce_opengl.h>
#include <hardwarekit/hardwarekit.h>
#include "PgCore.h"

/** The pedal's screen as a 320 x 240 RGB565 framebuffer (what the ILI9341 would show). */
class FrameCanvas : public pg::Canvas
{
public:
    void Fill (int x, int y, int w, int h, uint16_t c) override;
    int  Text (int x, int y, const char* s, const FontDef& f, uint16_t fg, uint16_t bg, int scale) override;
    void Blit (int x, int y, int w, int h, const uint16_t* src, int stride) override;
    std::array<uint16_t, kW * kH> px {};
};

/** A real-size 3D PG-1 (HardwareKit / OpenGL): drag knobs to turn them, click to push, wheel to spin;
    click and hold footswitches; click and drag on the screen to touch it; drag the background to orbit,
    wheel to zoom, double-click the background to reset the view. */
class PedalView : public juce::Component, private juce::OpenGLRenderer, private juce::Timer
{
public:
    explicit PedalView (pg::Core&);
    ~PedalView() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void resized() override { needsRepaint = true; }

private:
    // OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    enum class Hit { none, knob, footswitch, screen };
    struct Target { Hit kind = Hit::none; int index = -1; };
    Target hitTest (juce::Point<float>) const;
    hwk::gfx::Mat4 viewProj (float w, float h) const;
    hwk::gfx::Vec3 eye() const;
    bool screenPixel (juce::Point<float>, int& x, int& y, bool clamp) const;
    juce::Point<float> toPixels (hwk::gfx::Vec3) const;

    pg::Core& core;
    juce::OpenGLContext gl;

    // screen framebuffer: drawn by the core on the message thread, uploaded on the GL thread
    FrameCanvas canvas;
    juce::SpinLock frameLock;
    std::array<uint16_t, pg::Canvas::kW * pg::Canvas::kH> frameCopy {};
    std::atomic<bool> frameDirty { true };
    bool lastBacklight = true;

    // camera (orbit)
    std::atomic<float> yaw { 0.0f }, pitch { 0.92f }, distance { 3.25f };

    // control state
    std::array<std::atomic<float>, 4> knobAngle {};
    std::array<std::atomic<float>, 3> fsTravel {};
    std::array<bool, 3> fsHeld {};
    Target drag;
    float dragAccum = 0.0f;
    bool dragMoved = false, shiftHold = false;
    juce::Point<float> lastMouse;
    bool touching = false;
    int touchX = 0, touchY = 0;
    std::atomic<bool> needsRepaint { true };

    // GL resources (GL thread only)
    struct Mesh { hwk::gfx::GpuMesh gpu; };
    struct KnobPart { hwk::gfx::GpuMesh gpu; hwk::models::Role role; bool rotates; hwk::gfx::Vec3 colour; float polish; };
    std::unique_ptr<hwk::gfx::ShaderProgram> progFace, progPowder, progChrome, progPlastic, progRecess, progWood, progShadow, progLcd;
    hwk::gfx::GpuMesh meshFace, meshShell, meshLid, meshWell, meshLcd, meshDesk, meshShadow,
                      meshNutSmall, meshNutBig, meshThread, meshPlunger, meshCap, meshScrew, meshJackNut, meshJackHole,
                      meshDcNut,
                      meshSeedWin, meshSeedHdr, meshSeedGlue, meshSeedPcb, meshSeedChips, meshSeedUsb, meshSeedBtn,
                      meshBayWin, meshBayHdr;
    std::vector<std::unique_ptr<KnobPart>> knobParts;
    hwk::gfx::Texture2D texPrint, texLcd;
    std::vector<juce::uint8> lcdRgba;
    bool glReady = false;
    double startTime = juce::Time::getMillisecondCounterHiRes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalView)
};
