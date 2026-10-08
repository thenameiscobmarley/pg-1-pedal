// PG-1 core: everything the pedal does that isn't hardware - the home screen with its 8 tabs, every
// page and visualizer, knob / touch / footswitch behaviour, display safety and the audio DSP. The
// Seed3 firmware (main.cpp) and the Carla plugin (10-Carla-Plugin) both run this same code, so the
// screen you see in Carla is pixel for pixel the screen on the pedal.
//
// Threads: Process() runs on the audio thread / interrupt; DrawUi() and Touch() on the UI thread /
// main loop; the control calls (KnobTurn, KnobPress, Footswitch) on either. Shared state is plain
// ints, floats and flags with one writer each, as on the Seed3.
#pragma once
#include <cstddef>
#include <cstdint>
#include "Canvas.h"
#include "PgDsp.h"

// Big scratch buffers only the screen code uses: the firmware puts them in SDRAM (Makefile), so the fast
// on-chip RAM stays free for the audio. Elsewhere they are ordinary memory.
#ifndef PG_BIG_BSS
#define PG_BIG_BSS
#endif

namespace pg
{
constexpr int kKnobs = 4, kBands = 4, kTabs = 16; // home shows 8 per page

// The tabs, in signal-flow order (home: 4 top, 4 bottom)
enum Tab
{
    T_HUM,
    T_EQ,
    T_COMP,
    T_CLARITY,
    T_SAT,
    T_DEHARSH,
    T_SAFETY,
    T_VIS,
    T_INPUT, // page 2 (but first in the signal chain)
    T_HEALTH, // page 2: health checks
    T_TAKEBACK,
    T_WIDTH,
    T_LOUD,
    T_CONFIG,
    T_MBAND, // multiband dynamics (runs right after comp)
    T_PID    // pid auto-adjust: steers the other tabs' settings (listens after loudness)
};
constexpr int kSlots = 8; // saved configs
constexpr const char* kFirmwareVersion = "1.0";

// how a value is shown
enum Fmt : uint8_t
{
    F_INT,    // 18
    F_SINT,   // -24
    F_STENTH, // +3.0 (value in tenths)
    F_FREQ,   // semitones from 20 Hz -> 101 / 2.50k
    F_Q,      // index -> 0.92
    F_PCT,    // 40
    F_RATIO,  // tenths -> 2.5
    F_ATTACK, // index -> 10.8
    F_RELEASE,// 0 = auto, else index -> ms
    F_MAINS,  // auto / 50 / 60
    F_VMODE,  // spectrum / waterfall / stereo / levels
    F_VSRC,   // in / out / both
    F_ROW,    // 3/16
    F_SENS,   // relaxed / normal / strict
    F_ONOFF,  // off / on
    F_HVIEW,  // now / history
    F_CLMODE, // dynamic / add / normalise
    F_HZOFF,  // off / 120 hz
    F_SLOT,   // config 3
    F_SAVE,   // push x2
    F_THEME,  // pearl / bios
    F_KNOBS,  // normal / reverse
    F_HUND,   // 0.50 (value in hundredths)
    F_PIDGRP  // eq / comp / mband / clarity / deharsh / width
};

struct Param
{
    const char* name; // short, lower case (box label)
    const char* unit;
    const char* help; // one plain line, shown when the knob moves
    uint8_t     fmt;
    int         value, min, max, step, fine, def;
};

// dynamic eq: 5 values per band
enum
{
    B_FREQ,
    B_GAIN,
    B_Q,
    B_THRESH,
    B_RANGE,
    kPerBand
};
inline int ParamIndex(int band, int which) { return band * kPerBand + which; }

enum
{
    P_EQ_END = kBands * kPerBand,
    P_HUM_MAINS = P_EQ_END, P_HUM_DEPTH, P_HISS_CUT, P_HISS_THR,
    P_C_THRESH, P_C_RATIO, P_C_ATTACK, P_C_RELEASE,
    P_THUMP, P_DETAIL, P_CLARITY, P_WARMTH,
    P_DRIVE, P_EVEN, P_TONE, P_MIX,
    P_DH_DEPTH, P_DH_SENS, P_DH_SPEED, P_DH_COMFORT,
    P_S_CEIL, P_S_EAR, P_S_WOOF, P_S_TWEET,
    P_V_MODE, P_V_FALL, P_V_RANGE, P_V_SRC,
    P_IN_TARGET, P_IN_BOOST, P_IN_CUT, P_IN_SPEED,
    P_H_ROW, P_H_SENS, P_H_POP, P_H_VIEW,
    P_LRN_MARGIN, // hum tab, learned mode: how far above the learned hiss floor a band opens
    P_CL_MODE,    // clarity: dynamic / add / normalise (hold pg-1 + turn, or tap "mode")
    P_TB_PUNCH, P_TB_DETAIL, P_TB_AIR, P_TB_SPACE,
    P_W_WIDTH, P_W_MONO, P_W_AIR, P_W_GUARD,
    P_LD_LISTEN, P_LD_BASS, P_LD_TREBLE, P_LD_AUTO,
    P_CF_SLOT, P_CF_SAVE, P_CF_THEME, P_CF_KNOBS,
    P_TOUR_DONE, // (hidden) the first-power-up tour has been shown
    P_MB_LOW, P_MB_LMID, P_MB_HMID, P_MB_HIGH,
    P_DPAD, // config: the touch d-pad mode (off by default)
    P_SCR_FAST, // config: screen link fast (48 MHz, ~60 fps) or safe (24 MHz, ~25 fps)
    P_PID_P, P_PID_I, P_PID_D, P_PID_GROUP, // pid: the 3 gains, and which group pg-4 points at
    P_PID_MASK, // pid: the groups it steers (bits: eq, comp, mband, clarity, deharsh, width)
    P_PID_TILT, // pid: the target balance, dB per octave in tenths (hold pg-4 + turn)
    P_COUNT // a new setting also needs a permanent name in PgState.cpp (kRestKeys); saves load by name, so order is free
};

class Core
{
  public:
    // `framebuffer` = 320 x 240 RGB565 that the core draws into (on the Seed3 it lives in SDRAM)
    void Init(float sample_rate, uint16_t* framebuffer);

    // ---- controls
    void KnobTurn(int knob, int detents, uint32_t now_ms); // speed-sensitive; held knob = fine (eq pg-1: q)
    void KnobPress(int knob, bool down, uint32_t now_ms);  // click / double-click / long-press decided on release
    void Footswitch(int index, bool down, uint32_t now_ms); // 0 = fs-1, 1 = fs-2, 2 = fs-3
    void Touch(bool down, int x, int y, uint32_t now_ms); // call ~50x a second while running

    // ---- audio (non-interleaved stereo)
    void Process(const float* const* in, float* const* out, size_t frames, uint32_t now_ms);
    int  LatencySamples() const { return safety_.look - 1; } // the safety limiter's look-ahead

    // ---- screen: draws into the framebuffer, then pushes the changed rectangles to `c`.
    // Returns the number of pixels pushed this call (0 = nothing changed).
    int  DrawUi(Canvas& c, uint32_t now_ms);
    void ForceRedraw() { redraw_all_ = true; }
    void RepushScreen() { repush_ = true; } // send every pixel again (no redraw): heals a scrambled panel
    bool BacklightOn() const { return backlight_; } // off after 20 idle minutes (display rest)
    bool ScreenFast() const { return params_[P_SCR_FAST].value != 0; } // the platform sets the link speed from this
    uint32_t FrameMs() const { return ScreenFast() ? 16u : 40u; }       // ~60 fps fast, 25 fps safe

    // ---- flash mode request (fs-1 + fs-2 held 2 s)
    bool WantsFlashMode() const { return want_dfu_; }
    void ClearFlashMode() { want_dfu_ = false; }

    // ---- state (for saving in a DAW session / presets)
    Param& GetParam(int p) { return params_[p]; }
    void   SetParam(int p, int v);
    bool   EffectOn() const { return effect_on_; }
    bool   Comparing() const { return ab_; } // fs-3: hearing the untouched input at matched loudness
    void   SetEffectOn(bool on) { effect_on_ = on; redraw_title_ = true; state_dirty_ = true; }
    bool   StageOn(int tab) const { return tab == T_SAFETY || (tab >= 0 && tab < kTabs && stage_on_[tab]); }
    void   SetStageOn(int tab, bool on);
    int    Glitches() const { return safety_.glitches; }
    void   SetDeviceId(uint32_t id) { device_id_ = id; } // shown in config's about line (0 = the plugin)
    // the hum tab's learned noise profile (saved with the DAW session by the plugin)
    const NoiseProfile& Profile() const { return hum_.prof; }
    void                SetProfile(const NoiseProfile& p)
    {
        hum_.prof.valid = false;
        hum_.prof = p, hum_.applied_learned = false;
    }

    // ---- health checks: the platform reports what only it can see
    void ReportLoad(float fraction);  // audio processing time / block time (1.0 = no time left)
    void ReportDisplayFault();        // the screen link failed and was restarted
    int  ActiveAlerts() const;        // warnings + faults showing right now

    // ---- saving: the whole state (settings, 8 configs, learned noise) as one block of bytes. The pedal
    // keeps it in its flash; the plugin in the DAW session. WantsSave: something changed and then
    // nothing changed for 4 s (so a knob being turned isn't written over and over).
    static constexpr int kStateBytes = 4096;
    int  SaveState(uint8_t* out, int max) const; // returns the bytes used (0 = didn't fit)
    bool LoadState(const uint8_t* in, int n);
    bool WantsSave(uint32_t now) const { return state_dirty_ && now - last_input_ > 4000; }
    void SaveDone() { state_dirty_ = false; }
    // the health checks (the list on the health page is in this order)
    enum Check
    {
        C_DC, C_CLIP, C_ONESIDE, C_PHASE, C_NOINPUT, C_LIMITER, C_EARS, C_GLITCH, C_CPU, C_DROPOUT,
        C_KNOBSTUCK, C_KNOBJITTER, C_FSSTUCK, C_TOUCH, C_SCREEN, C_ULTRA_IN, C_INFRA_IN, C_ULTRA_OUT, C_INFRA_OUT,
        kChecks
    };
    static int         CheckSev(int check);  // 0 note, 1 warning, 2 fault
    static const char* CheckWhat(int check);
    bool               CheckOn(int check) const { return hs_[check].on; }
    int                RangeTrips() const { return safety_.range_trips; }

  private:
    enum Screen
    {
        HOME,
        PAGE
    };
    struct Rect
    {
        int x, y, w, h;
    };
    // a hard-edged outline: a `thick` px stroke inside `r`, drawn where the distance from the top
    // centre (0..1 around either side) is in [c0, c1). Composited at push time (never in fb_).
    struct Outline
    {
        Rect  r;
        int   thick;
        float c0, c1;
    };
    struct Anim
    {
        bool     on = false;
        int      index = 0;
        uint32_t t0 = 0, ms = 0;
        Rect     from{}, to{};
    };

    // ---- framebuffer drawing
    void Px(int x, int y, uint16_t c);
    void FillRect(int x, int y, int w, int h, uint16_t c);
    void FrameRect(int x, int y, int w, int h, uint16_t c);
    void Line(int x0, int y0, int x1, int y1, uint16_t c);
    void HLineDots(int x0, int x1, int y, int every, uint16_t c);
    void VLineDots(int x, int y0, int y1, int every, uint16_t c);
    int  TextFb(int x, int y, const char* s, const FontDef& f, uint16_t fg, int bg = -1);
    int  TextW(const char* s, const FontDef& f) const;
    void Dirty(int x, int y, int w, int h);
    void DrawIcon(int i, int cx, int cy, uint16_t c);
    void PearlBorder(const Rect& r, uint32_t now);
    static uint16_t Pearl(float h); // white / cream / pearl aurora palette
    static uint16_t Heat(float v);  // 0..1 -> deep blue .. cyan .. pearl .. white (waterfall)

    // ---- frame + animations
    int  Push(Canvas& c);
    int  CollectOutlines(Outline* out, uint32_t now) const;
    void AnimFrame(uint32_t now);
    void Compose(uint16_t* dst, int x, int y, int w, int h) const; // overlay outlines on a pushed block
    void FlashBox(int knob, uint32_t now);
    void FlashTab(int i, uint32_t now);
    void StartWipe(const Rect& from, const Rect& to, uint32_t now);
    void DrawRest(uint32_t now);
    void DrawTour(uint32_t now); // first power-up: 4 short cards over the home screen
    // touch d-pad (config: d-pad on; tap the page title to show it)
    Rect PadRect(int b) const; // 0 up, 1 left, 2 middle, 3 right, 4 down
    int  PadButtonAt(int x, int y) const;
    void PadAction(int b, bool down, uint32_t now);
    void DrawPad(uint32_t now);
    bool DrawSplash(uint32_t now); // start-up logo, then fade into the main screen
    void PxAdd(int x, int y, float a, float r, float g, float b);

    // ---- screens
    void DrawTitle(uint32_t now);
    void DrawHome(uint32_t now);
    void DrawTab(int i, uint32_t now);
    void DrawPage(uint32_t now);
    void DrawGraph(uint32_t now);
    void DrawStrip(uint32_t now);
    void DrawParamBox(int knob, uint32_t now);
    void GraphEq(uint32_t now);
    void GraphHum(uint32_t now);
    void GraphComp(uint32_t now);
    void GraphClarity(uint32_t now);
    void GraphSat(uint32_t now);
    void GraphDeHarsh(uint32_t now);
    void GraphSafety(uint32_t now);
    void GraphVis(uint32_t now);
    void GraphInput(uint32_t now);
    void GraphHealth(uint32_t now);
    void GraphTakeback(uint32_t now);
    void GraphWidth(uint32_t now);
    void GraphLoud(uint32_t now);
    void GraphConfig(uint32_t now);
    void GraphMband(uint32_t now);
    void GraphPid(uint32_t now);
    void VisScope(), VisBars(float fall, float range), VisHistory(float range);
    void RecordHistory(uint32_t now);
    void UpdateHealth(uint32_t now);
    void DrawFooter(uint32_t now);
    void StartLearn(uint32_t now);
    void UpdateLearn(uint32_t now);
    void FreqGrid(float fmin, float decades);
    void Meter(int x, int y, int w, int h, float db, float lo, float hi, float limit, uint16_t c, const char* label);
    Rect TabRect(int i) const;
    Rect ParamRect(int knob) const;

    // ---- helpers
    int   KnobParam(int knob) const; // which parameter a knob moves right now
    float FreqHz(int band) const;
    float QOf(int band) const;
    float Pf(int p) const { return float(params_[p].value); }
    int   XF(float hz, float fmin, float decades) const;
    float FX(int x, float fmin, float decades) const;
    int   YDb(float db, float top_db, float bottom_db) const;
    void  Format(int p, char* buf, int n) const;
    void  Spectrum(const float* ring, float* out, float fmin, float decades, float fall);
    void  SpecBars(const float* spec, float top_db, float bottom_db, uint16_t fill, uint16_t edge);
    void  EqResponse(float* out_set, float* out_now);
    void  OpenTab(int i, uint32_t now);
    void  GoHome(uint32_t now);
    int   StepFor(int p, uint32_t dt_ms, bool held) const;
    int   NodeAt(int x, int y) const;
    void  UpdateStages(size_t frames);
    void  ResetStages();
    bool  Waking(uint32_t now); // a control used while resting only wakes the screen

    Param params_[P_COUNT];

    float             sample_rate_ = 48000.f;
    volatile int      screen_ = HOME, focus_ = T_EQ, tab_ = T_EQ, band_ = 0;
    volatile bool     effect_on_ = true, want_dfu_ = false;
    volatile bool     stage_on_[kTabs] = {true, true, true, true, false, true, true, true, true, true, true, false, false, true, true, false};
    bool              pending_boot_ = true, repush_ = false;
    bool              splash_on_ = false, splash_ui_drawn_ = false;
    volatile bool     splash_skip_ = false;
    uint32_t          splash_t0_ = 0;
    float             fade_ = 1.f; // whole-screen brightness while the main screen fades in
    bool              tour_on_ = false;
    volatile bool     tour_skip_ = false;
    uint32_t          tour_t0_ = 0;
    int               tour_drawn_ = -1;
    uint32_t          device_id_ = 0;
    bool              pad_shown_ = false, pad_dirty_ = false;
    int               pad_sel_ = 0, pad_btn_ = -1; // the value box it works on; the button held down
    uint32_t          pad_use_t_ = 0, pad_btn_t0_ = 0, pad_rep_t_ = 0;
    volatile bool     redraw_all_ = true, redraw_title_ = true, redraw_panel_ = true;
    volatile uint32_t param_changed_ = 0; // bit per param box (0-3)
    uint32_t          both_held_since_ = 0, fs_t0_[3] = {};
    uint32_t          last_detent_[kKnobs] = {}, last_click_[kKnobs] = {}, press_t0_[kKnobs] = {};
    bool              knob_down_[kKnobs] = {}, turned_while_held_[kKnobs] = {};
    bool              fs_down_[3] = {}, fs_both_[3] = {};
    volatile bool     ab_ = false;

    // animations (outline overlays)
    Anim              a_tab_, a_box_, a_wipe_, a_boot_;
    Outline           prev_ol_[16];
    int               n_prev_ol_ = 0;
    uint32_t          ol_clock_ = 0; // colour flow
    volatile int      pending_flash_kind_ = -1, pending_flash_index_ = 0;
    volatile int      pending_open_ = -1;
    volatile bool     pending_home_ = false, pending_stage_toggle_ = false;


    // display rest (anti image-retention): rest screen after 5 idle minutes, backlight off after 20
    volatile uint32_t last_input_ = 0;
    volatile bool     resting_ = false, wake_ = false;
    bool              backlight_ = true;
    uint32_t          last_rest_draw_ = 0;
    Rect              rest_rect_{0, 0, 0, 0};

    // DSP
    LevelStage    lvl_;
    HumStage      hum_;
    DynEqStage    eq_;
    CompStage     comp_;
    ClarityStage  clar_;
    SaturateStage sat_;
    DeHarshStage  dh_;
    MultibandStage mb_;
    WidthStage    width_;
    TakebackStage tb_;
    LoudnessStage loud_;
    SafetyStage   safety_;
    PidStage      pid_;
    float         mix_[kTabs] = {}, wet_ = 1.f, c_ramp_ = 0.f;
    float         p_dry_ = 0.f, p_wet_ = 0.f, match_ = 1.f, ab_mix_ = 0.f, c_match_ = 0.f; // fair A/B
    size_t        sub_block_ = 48;
    bool          ts_mono_ = false;
    volatile float peak_in_ = 0.f;
    volatile uint32_t clip_t_ = 0, clips_ = 0; // input hotter than the converter: last time, count
    // health: raw-input statistics (from Process), platform reports, and each check's state
    volatile float    h_dc_[2] = {}, h_pow_[2] = {}, h_x_ = 0.f, load_ = 0.f;
    float             h_c_ = 0.f;
    volatile uint32_t disp_faults_ = 0, dropouts_ = 0, last_proc_ = 0, last_drop_t_ = 0, last_disp_t_ = 0, last_glitch_t_ = 0;
    int               seen_glitches_ = 0, drops_window_ = 0, seen_trips_ = 0;
    uint32_t          last_trip_t_ = 0;
    uint32_t          drops_mark_ = 0, drops_mark_t_ = 0;
    volatile int      knob_flips_[kKnobs] = {}, last_sign_[kKnobs] = {}, jitter_[kKnobs] = {};
    struct HealthState
    {
        float    bad = 0.f, good = 0.f;
        bool     on = false;
        uint16_t seen = 0;
        int      detail = -1;
    } hs_[kChecks];
    uint32_t last_health_ = 0, last_jitter_ = 0;
    int      alert_check_ = -1, drawn_alerts_ = 0;
    uint32_t alert_t0_ = 0;
    bool     footer_alert_ = false;
    Rect     alert_chip_{0, 0, 0, 0};
    bool           clip_lit_ = false;
    volatile float in_pk_[2] = {}, out_pk_[2] = {}, in_rms_ = 0.f, out_rms_ = 0.f;
    static constexpr int kRing = 1024, kScope = 512;
    float          ring_in_[kRing] = {}, ring_out_[kRing] = {};
    float          scope_l_[kScope] = {}, scope_r_[kScope] = {};
    volatile int   ring_pos_ = 0, scope_pos_ = 0;
    // hum learn: the input low-passed and kept at 1/8 the sample rate (fine detail below ~2.9 kHz)
    static constexpr int kRingLo = 2048, kDecim = 8;
    float          ring_lo_[kRingLo] = {};
    volatile int   lo_pos_ = 0;
    int            dec_n_ = 0;
    Svf            dec_lp_[2];
    int            learn_frames_ = 0, learn_state_ = 0; // 0 idle, 1 learning, 2 learned, 3 failed
    uint32_t       learn_t0_ = 0, last_learn_fft_ = 0, learn_msg_t0_ = 0;
    const char*    learn_msg_ = "";
    volatile bool  pending_learn_ = false;
    // configs
    struct Snap
    {
        int16_t v[P_COUNT];
        uint8_t st[kTabs];
        uint8_t on, used;
    } slots_[kSlots] = {};
    volatile bool     state_dirty_ = false, pending_load_ = false, pending_save_ = false;
    uint32_t          save_arm_t_ = 0;
    int               drawn_theme_ = -1;
    const char*       cfg_msg_ = "";
    uint32_t          cfg_msg_t0_ = 0;
    void              TakeSnap(Snap& s) const;
    void              PutSnap(const Snap& s);

    // UI state
    uint16_t* fb_ = nullptr;
    Rect      dirty_[40];
    int       n_dirty_ = 0;
    float     spec_a_[320] = {}, spec_b_[320] = {};
    float     gr_hist_[200] = {}, lv_in_[160] = {}, lv_out_[160] = {};
    int       lv_pos_ = 0;
    // visualizer: bars' peak caps, loudness history (one column per 50 ms, ~15 s), scope height
    static constexpr int kBars = 31;
    float     bar_pk_[kBars] = {};
    float     hist_in_[320] = {}, hist_out_[320] = {};
    int       hist_pos_ = 0;
    uint32_t  last_hist_ = 0;
    float     scope_gain_ = 4.f;
    int       gr_pos_ = 0;
    float     corr_ = 0.f;
    uint32_t  last_graph_ = 0, last_border_ = 0, last_anim_ = 0, last_strip_ = 0;
    int       drawn_focus_ = -1, drawn_band_ = -1, vis_drawn_ = -1, drawn_page_ = 0;
    struct TouchState
    {
        bool     down = false;
        int      x0 = 0, y0 = 0, x = 0, y = 0;
        uint32_t t0   = 0;
        int      node = -1;
        bool     moved = false;
    } ts_;
    uint32_t last_tap_time_ = 0;
    int      last_tap_node_ = -1;
};
} // namespace pg
