// PG-1 core: parameters, controls, the audio chain, the frame loop (dirty rectangles + outline
// overlays), display rest, home screen and touch. The pages' drawing is in PgPages.cpp.
#include "PgCore.h"
#include "PgUi.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pg
{
static constexpr uint32_t kTabAnimMs = 700, kBoxAnimMs = 520, kWipeMs = 650, kBootMs = 2300;
static constexpr uint32_t kRestMs = 5u * 60u * 1000u, kDarkMs = 20u * 60u * 1000u;

// which parameter each knob moves on a page (eq: per band, see KnobParam)
static const int kTabBase[kTabs] = {P_HUM_MAINS, -1, P_C_THRESH, P_THUMP, P_DRIVE, P_DH_DEPTH, P_S_CEIL, P_V_MODE, P_IN_TARGET, P_H_ROW,
                                   P_TB_PUNCH, P_W_WIDTH, P_LD_LISTEN, P_CF_SLOT, P_MB_LOW};

void Core::Init(float sample_rate, uint16_t* framebuffer)
{
    sample_rate_ = sample_rate;
    fb_          = framebuffer;
    static const int freq_def[kBands] = {28, 52, 84, 104}; // 100 Hz, 400 Hz, 2.5 kHz, 8 kHz
    for(int b = 0; b < kBands; b++)
    {
        params_[ParamIndex(b, B_FREQ)]   = {"freq", "hz", "pg-1 freq: where the band sits. hold+turn = q", F_FREQ, freq_def[b], 0, 119, 1, 1, freq_def[b]};
        params_[ParamIndex(b, B_GAIN)]   = {"gain", "db", "pg-2 gain: always lifts (+) or cuts (-) the band", F_STENTH, 0, -150, 150, 5, 1, 0};
        params_[ParamIndex(b, B_Q)]      = {"q", "", "q (hold pg-1 + turn): low = wide, high = narrow", F_Q, 13, 0, 40, 1, 1, 13};
        params_[ParamIndex(b, B_THRESH)] = {"thresh", "db", "pg-3 thresh: band level where it starts to act", F_SINT, -24, -60, 0, 1, 1, -24};
        params_[ParamIndex(b, B_RANGE)]  = {"range", "db", "pg-4 range: most it moves when loud. - cut + lift", F_STENTH, -60, -240, 120, 5, 1, -60};
    }
    const Param rest[] = {
        {"mode", "", "pg-1 mode: auto, 50, 60, learned (push it to learn)", F_MAINS, 0, 0, 3, 1, 1, 0},
        {"hum", "db", "pg-2 hum: how deep the hum notches may go", F_INT, 18, 0, 40, 1, 1, 18},
        {"hiss", "db", "pg-3 hiss: how far hiss drops in quiet parts", F_INT, 6, 0, 40, 1, 1, 6},
        {"hiss thr", "db", "pg-4 hiss thr: top end under this = hiss", F_SINT, -60, -90, -30, 1, 1, -60},
        {"thresh", "db", "pg-1 thresh: level where it starts to squeeze", F_SINT, -18, -40, 0, 1, 1, -18},
        {"ratio", ":1", "pg-2 ratio: how hard it squeezes above thresh", F_RATIO, 25, 10, 100, 1, 1, 25},
        {"attack", "ms", "pg-3 attack: how fast it grabs. slow = punchy", F_ATTACK, 27, 0, 40, 1, 1, 27},
        {"release", "ms", "pg-4 release: how fast it lets go. 0 = auto", F_RELEASE, 0, 0, 40, 1, 1, 0},
        {"thump", "db", "pg-1 thump: most it lifts the lows on each hit", F_STENTH, 24, 0, 60, 5, 1, 24},
        {"detail", "", "pg-2 detail: bass harmonics, heard anywhere", F_STENTH, 21, 0, 60, 5, 1, 21},
        {"clarity", "db", "pg-3 clarity: presence lift (and mud cut)", F_STENTH, 20, 0, 60, 5, 1, 20},
        {"warmth", "db", "pg-4 warmth: fuller lows (tames bright highs)", F_STENTH, 10, 0, 40, 5, 1, 10},
        {"drive", "db", "pg-1 drive: how hard it hits the tape curve", F_STENTH, 40, 0, 180, 5, 1, 40},
        {"even", "%", "pg-2 even: tube-like even harmonics (warmer)", F_PCT, 30, 0, 100, 2, 1, 30},
        {"tone", "%", "pg-3 tone: top end of the drive. low = darker", F_PCT, 70, 0, 100, 2, 1, 70},
        {"mix", "%", "pg-4 mix: blend of driven and clean", F_PCT, 100, 0, 100, 2, 1, 100},
        {"depth", "db", "pg-1 depth: most it cuts one harsh spot", F_INT, 6, 0, 12, 1, 1, 6},
        {"sens", "%", "pg-2 sens: how easily a spot counts as harsh", F_PCT, 50, 0, 100, 2, 1, 50},
        {"speed", "%", "pg-3 speed: how fast cuts let go again", F_PCT, 50, 0, 100, 2, 1, 50},
        {"comfort", "%", "pg-4 comfort: softer 3k as it gets loud", F_PCT, 50, 0, 100, 2, 1, 50},
        {"ceiling", "db", "pg-1 ceiling: peaks never go above this", F_STENTH, -10, -120, -3, 1, 1, -10},
        {"ears", "db", "pg-2 ears: max loudness over ~3 s, eases down", F_SINT, -10, -30, -3, 1, 1, -10},
        {"woofer", "db", "pg-3 woofer: max steady bass power (cool coil)", F_SINT, -9, -30, 0, 1, 1, -9},
        {"tweeter", "db", "pg-4 tweeter: max steady treble power", F_SINT, -20, -40, -6, 1, 1, -20},
        {"view", "", "pg-1 view: spectrum, waterfall, stereo, levels", F_VMODE, 0, 0, 3, 1, 1, 0},
        {"fall", "", "pg-2 fall: how fast the display drops back", F_INT, 5, 1, 10, 1, 1, 5},
        {"range", "db", "pg-3 range: db shown from top to bottom", F_INT, 90, 30, 120, 10, 10, 90},
        {"source", "", "pg-4 source: input, output or both", F_VSRC, 2, 0, 2, 1, 1, 2},
        {"target", "db", "pg-1 target: level every source is brought to", F_SINT, -18, -30, -10, 1, 1, -18},
        {"boost", "db", "pg-2 boost: most it lifts a quiet source", F_INT, 12, 0, 24, 1, 1, 12},
        {"cut", "db", "pg-3 cut: most it turns down a hot source", F_INT, 12, 0, 24, 1, 1, 12},
        {"speed", "%", "pg-4 speed: how fast it follows level changes", F_PCT, 30, 0, 100, 2, 1, 30},
        {"check", "", "pg-1 check: pick one to read what it means", F_ROW, 0, 0, kChecks - 1, 1, 1, 0},
        {"sens", "", "pg-2 sens: strict = flag even a little weird", F_SENS, 2, 0, 2, 1, 1, 2},
        {"pop-ups", "", "pg-3 pop-ups: show a line when something's off", F_ONOFF, 1, 0, 1, 1, 1, 1},
        {"view", "", "pg-4 view: now, or how often since power-on", F_HVIEW, 0, 0, 1, 1, 1, 0},
        {"margin", "db", "pg-4 margin: how far above the learned hiss it opens", F_INT, 4, 0, 12, 1, 1, 4},
        {"mode", "", "mode (hold pg-1 + turn): dynamic, add, normalise", F_CLMODE, 0, 0, 2, 1, 1, 0},
        {"punch", "%", "pg-1 punch: gives back the hits comp/limit squashed", F_PCT, 50, 0, 100, 2, 1, 50},
        {"detail", "%", "pg-2 detail: lifts quiet details that got buried", F_PCT, 40, 0, 100, 2, 1, 40},
        {"air", "%", "pg-3 air: gives back top end that went missing", F_PCT, 50, 0, 100, 2, 1, 50},
        {"space", "%", "pg-4 space: gives back stereo width that shrank", F_PCT, 40, 0, 100, 2, 1, 40},
        {"width", "%", "pg-1 width: 100 = as is, more = wider, less = narrower", F_PCT, 130, 0, 200, 5, 1, 130},
        {"bass mono", "hz", "pg-2 bass mono: bass below this stays centred", F_HZOFF, 120, 0, 300, 10, 10, 120},
        {"air", "db", "pg-3 air: a little sparkle on the sides", F_INT, 2, 0, 6, 1, 1, 2},
        {"guard", "", "pg-4 guard: eases width off if mono would lose sound", F_ONOFF, 1, 0, 1, 1, 1, 1},
        {"listen", "phon", "pg-1 listen: how loud you listen (40 quiet, 90 loud)", F_INT, 70, 40, 90, 1, 1, 70},
        {"bass", "%", "pg-2 bass: how much bass comes back at low volume", F_PCT, 60, 0, 100, 2, 1, 60},
        {"treble", "%", "pg-3 treble: how much treble comes back at low volume", F_PCT, 40, 0, 100, 2, 1, 40},
        {"follow", "", "pg-4 follow: quiet passages get a bit more too", F_ONOFF, 1, 0, 1, 1, 1, 1},
        {"config", "", "pg-1 config: pick 1-8. push = load it", F_SLOT, 0, 0, kSlots - 1, 1, 1, 0},
        {"save", "", "pg-2 save: push twice to save into this config", F_SAVE, 0, 0, 0, 1, 1, 0},
        {"theme", "", "pg-3 theme: pearl (light) or classic bios", F_THEME, 0, 0, 1, 1, 1, 0},
        {"knobs", "", "pg-4 knobs: flip if turning right goes down", F_KNOBS, 0, 0, 1, 1, 1, 0},
        {"tour", "", "", F_INT, 0, 0, 1, 1, 1, 0},
        {"bass", "%", "pg-1 bass: how firmly boomy bass peaks are held", F_PCT, 30, 0, 100, 2, 1, 30},
        {"low mids", "%", "pg-2 low mids: holds back muddy / boxy bursts", F_PCT, 30, 0, 100, 2, 1, 30},
        {"high mids", "%", "pg-3 high mids: holds back harsh, shouty hits", F_PCT, 30, 0, 100, 2, 1, 30},
        {"highs", "%", "pg-4 highs: holds back sharp s's and cymbals", F_PCT, 30, 0, 100, 2, 1, 30},
        {"d-pad", "", "hold pg-4 + turn: the touch d-pad on / off", F_ONOFF, 0, 0, 1, 1, 1, 0},
        {"screen", "", "hold pg-3 + turn: screen fast (60 fps) or safe", F_ONOFF, 1, 0, 1, 1, 1, 1},
    };
    for(int i = 0; i < P_COUNT - P_EQ_END; i++)
        params_[P_EQ_END + i] = rest[i];

    ui::SetTheme(params_[P_CF_THEME].value);
    drawn_theme_ = params_[P_CF_THEME].value;
    sub_block_ = size_t(sample_rate_ / 1000.f + 0.5f); // controls + coefficients at 1 kHz
    if(sub_block_ < 8)
        sub_block_ = 8;
    c_ramp_  = Coef(10.f, sample_rate_);
    c_match_ = Coef(3000.f, sample_rate_);
    h_c_     = Coef(500.f, sample_rate_);
    for(Svf& f : dec_lp_)
        f.Set(Svf::LP, sample_rate_, 2400.f, 0.707f), f.Reset();
    for(int i = 0; i < 160; i++)
        lv_in_[i] = lv_out_[i] = -120.f;
    ResetStages();
    safety_.Init(sample_rate_);
    for(int t = 0; t < kTabs; t++)
        mix_[t] = StageOn(t) ? 1.f : 0.f;
    wet_          = effect_on_ ? 1.f : 0.f;
    redraw_all_   = true;
    pending_boot_ = true;
}

void Core::ResetStages()
{
    lvl_.Init(sample_rate_);
    hum_.Init(sample_rate_);
    eq_.Init(sample_rate_);
    comp_.Init(sample_rate_);
    clar_.Init(sample_rate_);
    sat_.Init(sample_rate_);
    dh_.Init(sample_rate_);
    mb_.Init(sample_rate_);
    width_.Init(sample_rate_);
    tb_.Init(sample_rate_);
    loud_.Init(sample_rate_);
}

void Core::SetParam(int p, int v)
{
    if(p < 0 || p >= P_COUNT)
        return;
    Param& pr = params_[p];
    v         = v < pr.min ? pr.min : (v > pr.max ? pr.max : v);
    if(v == pr.value)
        return;
    pr.value = v;
    for(int k = 0; k < kKnobs; k++)
        if(KnobParam(k) == p || (tab_ == T_EQ && k == 0 && p == ParamIndex(band_, B_Q)))
            param_changed_ |= 1u << k;
    if(p < P_EQ_END && p % kPerBand == B_FREQ)
        redraw_panel_ = true; // the band chips show the frequency too
    state_dirty_ = true;
}

void Core::SetStageOn(int tab, bool on)
{
    if(tab < 0 || tab >= kTabs || tab == T_SAFETY || tab == T_VIS || tab == T_HEALTH || tab == T_CONFIG)
        return;
    stage_on_[tab] = on;
    state_dirty_   = true;
    redraw_title_  = true;
}

float Core::FreqHz(int band) const { return 20.f * powf(2.f, float(params_[ParamIndex(band, B_FREQ)].value) / 12.f); }
float Core::QOf(int band) const { return 0.3f * powf(2.f, float(params_[ParamIndex(band, B_Q)].value) / 8.f); }

int Core::KnobParam(int k) const
{
    if(screen_ != PAGE || k < 0 || k >= kKnobs)
        return -1;
    if(tab_ == T_EQ)
    {
        static const int map[kKnobs] = {B_FREQ, B_GAIN, B_THRESH, B_RANGE};
        return ParamIndex(band_, (k == 0 && knob_down_[0]) ? B_Q : map[k]);
    }
    if(tab_ == T_CLARITY && k == 0 && knob_down_[0])
        return P_CL_MODE; // hold pg-1 + turn: the mode
    if(tab_ == T_CONFIG && k == 3 && knob_down_[3])
        return P_DPAD; // hold pg-4 + turn: the touch d-pad
    if(tab_ == T_CONFIG && k == 2 && knob_down_[2])
        return P_SCR_FAST; // hold pg-3 + turn: screen fast / safe
    if(tab_ == T_HUM && k == 3 && params_[P_HUM_MAINS].value == 3)
        return P_LRN_MARGIN; // learned mode: pg-4 sets the gate's margin instead of the hiss threshold
    return kTabBase[tab_] + k;
}

int Core::StepFor(int p, uint32_t dt_ms, bool held) const
{
    const Param& pr = params_[p];
    if(held)
        return pr.fine;
    const int mult = dt_ms < 25 ? 10 : dt_ms < 50 ? 5 : dt_ms < 100 ? 2 : 1;
    const int span = pr.max - pr.min;
    const int s    = pr.step * mult;
    return (span >= 8 && s > span / 4) ? span / 4 : s;
}

// ------------------------------------------------------------------ controls
bool Core::Waking(uint32_t now)
{
    last_input_ = now;
    if(splash_on_)
        splash_skip_ = true; // any control skips the start-up logo
    if(tour_on_)
        tour_skip_ = true; // ...and the first-power-up tour
    if(resting_)
    {
        wake_ = true;
        return true;
    }
    return false;
}

void Core::KnobTurn(int i, int inc, uint32_t now)
{
    if(inc == 0 || i < 0 || i >= kKnobs || Waking(now))
        return;
    if(params_[P_CF_KNOBS].value)
        inc = -inc; // config: knobs wired the other way round
    const int sign = inc > 0 ? 1 : -1; // health: a knob flipping direction many times a second = noisy wiring
    if(last_sign_[i] != 0 && sign != last_sign_[i])
        knob_flips_[i]++;
    last_sign_[i] = sign;
    if(knob_down_[i])
        turned_while_held_[i] = true;
    if(screen_ == HOME)
    {
        focus_               = ((focus_ + inc) % kTabs + kTabs) % kTabs;
        pending_flash_kind_  = 0;
        pending_flash_index_ = focus_;
    }
    else
    {
        const int  p      = KnobParam(i);
        const bool q_mode = (tab_ == T_EQ || tab_ == T_CLARITY) && i == 0 && knob_down_[0];
        SetParam(p, params_[p].value + inc * StepFor(p, now - last_detent_[i], knob_down_[i] && !q_mode));
        pending_flash_kind_  = 1;
        pending_flash_index_ = i;
    }
    last_detent_[i] = now;
}

void Core::KnobPress(int i, bool down, uint32_t now)
{
    if(i < 0 || i >= kKnobs)
        return;
    if(down)
    {
        if(Waking(now))
            return;
        knob_down_[i]         = true;
        turned_while_held_[i] = false;
        press_t0_[i]          = now;
        if(screen_ == PAGE && (((tab_ == T_EQ || tab_ == T_CLARITY) && i == 0) || (tab_ == T_CONFIG && i >= 2)))
            param_changed_ |= 1u << i; // the box shows its held setting (q / mode / screen / d-pad) while held
        return;
    }
    if(!knob_down_[i])
        return;
    knob_down_[i] = false;
    if(screen_ == PAGE && (((tab_ == T_EQ || tab_ == T_CLARITY) && i == 0) || (tab_ == T_CONFIG && i >= 2)))
        param_changed_ |= 1u << i;
    if(turned_while_held_[i])
        return;
    if(now - press_t0_[i] >= 600) // long-press: back to the home screen
    {
        if(screen_ != HOME)
            pending_home_ = true;
        return;
    }
    if(screen_ == HOME)
    {
        pending_open_ = focus_;
        return;
    }
    const int p = KnobParam(i);
    if(tab_ == T_CONFIG && (i == 0 || i == 1)) // config: push pg-1 = load, push pg-2 twice = save
    {
        if(i == 0)
            pending_load_ = true;
        else if(now - save_arm_t_ < 3000)
            pending_save_ = true, save_arm_t_ = 0;
        else
            save_arm_t_ = now, cfg_msg_ = "push pg-2 again to save here", cfg_msg_t0_ = now;
        pending_flash_kind_ = 1, pending_flash_index_ = i;
        return;
    }
    if(tab_ == T_HUM && i == 0 && params_[P_HUM_MAINS].value == 3 && now - last_click_[i] >= 350)
        pending_learn_ = true; // hum tab, mode "learned": a push starts learning
    if(now - last_click_[i] < 350) // double-push: back to default
    {
        if(tab_ == T_HUM && i == 0)
            pending_learn_ = false, hum_.learning = false, learn_state_ = learn_state_ == 1 ? 0 : learn_state_;
        SetParam(p, params_[p].def);
        last_click_[i] = 0;
    }
    else
        last_click_[i] = now;
    pending_flash_kind_  = 1;
    pending_flash_index_ = i;
}

void Core::Footswitch(int i, bool down, uint32_t now)
{
    if(i < 0 || i > 2)
        return;
    if(i == 2) // fs-3: fair A/B - the untouched input at the same loudness, tap again to come back
    {
        const bool was3 = fs_down_[2];
        fs_down_[2]     = down;
        if(down && !was3)
            fs_t0_[2] = now;
        if(down && !was3 && !Waking(now))
            ab_ = !ab_, redraw_title_ = true;
        return;
    }
    const bool was = fs_down_[i];
    fs_down_[i]    = down;
    if(down && !was && Waking(now))
    {
        fs_both_[i] = true; // swallow this press and its release
        return;
    }
    if(fs_down_[0] && fs_down_[1])
    {
        fs_both_[0] = fs_both_[1] = true;
        if(both_held_since_ == 0)
            both_held_since_ = now;
        return;
    }
    both_held_since_ = 0;
    if(down && !was)
    {
        fs_t0_[i] = now;
        if(i == 0) // fs-1: whole-pedal bypass, on the press (no lag)
        {
            effect_on_    = !effect_on_;
            redraw_title_ = true;
            state_dirty_  = true;
        }
        return;
    }
    if(down || !was)
        return;
    if(fs_both_[i]) // the release after a both-held or a wake-up press
    {
        fs_both_[i] = false;
        return;
    }
    if(i == 1)
    {
        if(now - fs_t0_[1] >= 600) // hold fs-2: this stage on / off
            pending_stage_toggle_ = true;
        else if(screen_ == HOME)
        {
            focus_               = (focus_ + 1) % kTabs;
            pending_flash_kind_  = 0;
            pending_flash_index_ = focus_;
        }
        else if(tab_ == T_EQ) // fs-2 on the eq: next band
        {
            band_         = (band_ + 1) % kBands;
            redraw_panel_ = true;
        }
        else
            pending_open_ = (tab_ + 1) % kTabs; // elsewhere: the next page
    }
}

void Core::OpenTab(int i, uint32_t now)
{
    const Rect from = screen_ == HOME ? TabRect(i) : Rect{0, 0, Canvas::kW, 20};
    focus_ = tab_ = i;
    screen_       = PAGE;
    StartWipe(from, {0, 0, Canvas::kW, Canvas::kH}, now);
    a_box_.on   = false;
    redraw_all_ = true;
}

void Core::GoHome(uint32_t now)
{
    focus_  = tab_;
    screen_ = HOME;
    StartWipe({0, 0, Canvas::kW, Canvas::kH}, TabRect(tab_), now);
    a_box_.on   = false;
    redraw_all_ = true;
}

// ------------------------------------------------------------------ audio
void Core::UpdateStages(size_t frames)
{
    const float bs = float(frames) / sample_rate_;
    if(safety_.want_reset) // a glitch was caught: start every filter clean
    {
        safety_.want_reset = false;
        ResetStages();
    }
    if(StageOn(T_INPUT) || mix_[T_INPUT] > 0.f)
        lvl_.Update(Pf(P_IN_TARGET), Pf(P_IN_BOOST), Pf(P_IN_CUT), Pf(P_IN_SPEED) * 0.01f, bs);
    if(StageOn(T_HUM) || mix_[T_HUM] > 0.f)
        hum_.Update(params_[P_HUM_MAINS].value, Pf(P_HUM_DEPTH), Pf(P_HISS_CUT), Pf(P_HISS_THR), bs, Pf(P_LRN_MARGIN));
    if(StageOn(T_EQ) || mix_[T_EQ] > 0.f)
        for(int b = 0; b < kBands; b++)
            eq_.Update(b, FreqHz(b), QOf(b), Pf(ParamIndex(b, B_GAIN)) * 0.1f, Pf(ParamIndex(b, B_THRESH)),
                       Pf(ParamIndex(b, B_RANGE)) * 0.1f, bs);
    if(StageOn(T_COMP) || mix_[T_COMP] > 0.f)
    {
        const int r = params_[P_C_RELEASE].value;
        comp_.Update(Pf(P_C_THRESH), Pf(P_C_RATIO) * 0.1f, 0.1f * powf(2.f, Pf(P_C_ATTACK) / 4.f),
                     r == 0 ? 0.f : 20.f * powf(2.f, float(r - 1) / 6.f));
    }
    if(StageOn(T_CLARITY) || mix_[T_CLARITY] > 0.f)
        clar_.Update(Pf(P_THUMP) * 0.1f, Pf(P_DETAIL) * 0.1f, Pf(P_CLARITY) * 0.1f, Pf(P_WARMTH) * 0.1f, bs, params_[P_CL_MODE].value);
    if(StageOn(T_MBAND) || mix_[T_MBAND] > 0.f)
    {
        const float a[4] = {Pf(P_MB_LOW) * 0.01f, Pf(P_MB_LMID) * 0.01f, Pf(P_MB_HMID) * 0.01f, Pf(P_MB_HIGH) * 0.01f};
        mb_.Update(a, bs);
    }
    if(StageOn(T_WIDTH) || mix_[T_WIDTH] > 0.f)
        width_.Update(Pf(P_W_WIDTH) * 0.01f, Pf(P_W_MONO), Pf(P_W_AIR), params_[P_W_GUARD].value != 0, bs);
    if(StageOn(T_TAKEBACK) || mix_[T_TAKEBACK] > 0.f)
        tb_.Update(Pf(P_TB_PUNCH) * 0.01f, Pf(P_TB_DETAIL) * 0.01f, Pf(P_TB_AIR) * 0.01f, Pf(P_TB_SPACE) * 0.01f, bs);
    if(StageOn(T_LOUD) || mix_[T_LOUD] > 0.f)
        loud_.Update(Pf(P_LD_LISTEN), Pf(P_LD_BASS) * 0.01f, Pf(P_LD_TREBLE) * 0.01f, params_[P_LD_AUTO].value != 0, bs);
    if(StageOn(T_SAT) || mix_[T_SAT] > 0.f)
        sat_.Update(Pf(P_DRIVE) * 0.1f, Pf(P_EVEN) * 0.01f, Pf(P_TONE) * 0.01f, Pf(P_MIX) * 0.01f);
    if(StageOn(T_DEHARSH) || mix_[T_DEHARSH] > 0.f)
        dh_.Update(Pf(P_DH_DEPTH), Pf(P_DH_SENS) * 0.01f, Pf(P_DH_SPEED) * 0.01f, Pf(P_DH_COMFORT) * 0.01f, bs);
    safety_.Update(Pf(P_S_CEIL) * 0.1f, Pf(P_S_EAR), Pf(P_S_WOOF), Pf(P_S_TWEET), bs);
}

void Core::Process(const float* const* in, float* const* out, size_t n, uint32_t now)
{
    if(both_held_since_ != 0 && now - both_held_since_ > 2000)
        want_dfu_ = true;
    // health: a gap much longer than this block's own length = the audio stalled (a dropout)
    const uint32_t block_ms = uint32_t(float(n) * 1000.f / sample_rate_) + 1;
    if(last_proc_ != 0 && now - last_proc_ > 6u + 3u * block_ms && now - last_proc_ < 5000u)
        dropouts_++;
    last_proc_ = now;
    float hd[2] = {h_dc_[0], h_dc_[1]}, hp[2] = {h_pow_[0], h_pow_[1]}, hx = h_x_;

    float pk_in[2] = {}, pk_out[2] = {}, sq_in = 0.f, sq_out = 0.f;
    bool  clipped = false;
    size_t done = 0;
    while(done < n)
    {
        const size_t m = (n - done) < sub_block_ ? (n - done) : sub_block_;
        UpdateStages(m);
        bool on[kTabs];
        for(int t = 0; t < kTabs; t++)
            on[t] = StageOn(t);
        for(size_t j = 0; j < m; j++)
        {
            const size_t i = done + j;
            float        l = in[0][i], r = in[1][i];
            if(fabsf(l) >= 0.985f || fabsf(r) >= 0.985f) // the converter is at its limit: source too hot
                clipped = true;
            if(!(fabsf(l) < 8.f))
                l = 0.f; // a broken input sample never reaches the filters
            if(!(fabsf(r) < 8.f))
                r = 0.f;
            hd[0] += (l - hd[0]) * h_c_, hd[1] += (r - hd[1]) * h_c_; // raw input, before anything touches it
            hp[0] += (l * l - hp[0]) * h_c_, hp[1] += (r * r - hp[1]) * h_c_, hx += (l * r - hx) * h_c_;
            if(fabsf(r) < 1e-6f && fabsf(l) > 1e-4f && peak_in_ < 0.0005f) // mono (TS) cable: copy left to right
                r = l;
            peak_in_ = fmaxf(fabsf(r), peak_in_ * 0.9999f);
            pk_in[0] = fmaxf(pk_in[0], fabsf(l)), pk_in[1] = fmaxf(pk_in[1], fabsf(r));
            const float mono_in = 0.5f * (l + r);
            sq_in += mono_in * mono_in;
            ring_in_[ring_pos_] = mono_in;
            const float lo = dec_lp_[1].Run(0, dec_lp_[0].Run(0, mono_in)); // for the hum learn's fine low end
            if(++dec_n_ >= kDecim)
                dec_n_ = 0, ring_lo_[lo_pos_] = lo, lo_pos_ = (lo_pos_ + 1) & (kRingLo - 1);

            const float dl = l, dr = r;
            // each stage fades in / out over ~10 ms when switched (no clicks); off = not computed
#define PG_STAGE(T, obj)                                               \
    mix_[T] += ((on[T] ? 1.f : 0.f) - mix_[T]) * c_ramp_;              \
    if(!on[T] && mix_[T] < 1e-3f)                                      \
        mix_[T] = 0.f;                                                 \
    if(mix_[T] > 0.f)                                                  \
    {                                                                  \
        float a = l, b = r;                                            \
        obj.Run(a, b);                                                 \
        l += (a - l) * mix_[T];                                        \
        r += (b - r) * mix_[T];                                        \
    }
            PG_STAGE(T_HUM, hum_) // noise out first, so the auto-level never lifts it
            PG_STAGE(T_INPUT, lvl_)
            const float ref_l = l, ref_r = r; // takeback compares against the sound as it came in
            PG_STAGE(T_EQ, eq_)
            PG_STAGE(T_COMP, comp_)
            PG_STAGE(T_MBAND, mb_)
            PG_STAGE(T_CLARITY, clar_)
            PG_STAGE(T_SAT, sat_)
            PG_STAGE(T_DEHARSH, dh_)
            PG_STAGE(T_WIDTH, width_)
            mix_[T_TAKEBACK] += ((on[T_TAKEBACK] ? 1.f : 0.f) - mix_[T_TAKEBACK]) * c_ramp_;
            if(!on[T_TAKEBACK] && mix_[T_TAKEBACK] < 1e-3f)
                mix_[T_TAKEBACK] = 0.f;
            if(mix_[T_TAKEBACK] > 0.f)
            {
                float a = l, b = r;
                tb_.Run(a, b, ref_l, ref_r);
                l += (a - l) * mix_[T_TAKEBACK];
                r += (b - r) * mix_[T_TAKEBACK];
            }
            PG_STAGE(T_LOUD, loud_)
#undef PG_STAGE
            // loudness of the untouched input and of the processed sound (~3 s), for the fair A/B
            const float pd = 0.5f * (dl * dl + dr * dr), pw = 0.5f * (l * l + r * r);
            if(pd > 1e-7f && pw > 1e-7f) // only while something is playing
                p_dry_ += (pd - p_dry_) * c_match_, p_wet_ += (pw - p_wet_) * c_match_;
            wet_ += ((effect_on_ ? 1.f : 0.f) - wet_) * c_ramp_;
            l = dl + (l - dl) * wet_;
            r = dr + (r - dr) * wet_;
            ab_mix_ += ((ab_ ? 1.f : 0.f) - ab_mix_) * c_ramp_;
            if(ab_mix_ > 1e-4f)
            {
                const float k = Clampf(sqrtf((p_wet_ + 1e-12f) / (p_dry_ + 1e-12f)), 0.25f, 4.f); // +-12 dB at most
                match_ += (k - match_) * c_ramp_;
                l += (dl * match_ - l) * ab_mix_;
                r += (dr * match_ - r) * ab_mix_;
            }
            safety_.Run(l, r); // always: after the bypass too

            out[0][i] = l;
            out[1][i] = r;
            pk_out[0] = fmaxf(pk_out[0], fabsf(l)), pk_out[1] = fmaxf(pk_out[1], fabsf(r));
            const float mono_out = 0.5f * (l + r);
            sq_out += mono_out * mono_out;
            ring_out_[ring_pos_] = mono_out;
            ring_pos_            = (ring_pos_ + 1) & (kRing - 1);
            scope_l_[scope_pos_] = l, scope_r_[scope_pos_] = r;
            scope_pos_           = (scope_pos_ + 1) & (kScope - 1);
        }
        done += m;
    }
    if(clipped)
        clip_t_ = now, clips_++;
    h_dc_[0] = hd[0], h_dc_[1] = hd[1], h_pow_[0] = hp[0], h_pow_[1] = hp[1], h_x_ = hx;
    // meters for the screen (peaks fall back ~20 dB/s, rms is a ~300 ms average)
    const float fall = expf(-float(n) / sample_rate_ * 2.3f), rc = 1.f - expf(-float(n) / (0.3f * sample_rate_));
    for(int ch = 0; ch < 2; ch++)
    {
        in_pk_[ch]  = fmaxf(pk_in[ch], in_pk_[ch] * fall);
        out_pk_[ch] = fmaxf(pk_out[ch], out_pk_[ch] * fall);
    }
    in_rms_  = in_rms_ + (sq_in / float(n ? n : 1) - in_rms_) * rc;
    out_rms_ = out_rms_ + (sq_out / float(n ? n : 1) - out_rms_) * rc;
}

// ------------------------------------------------------------------ framebuffer primitives
// Every write is clipped here, so nothing - not even a NaN turned into a coordinate - can draw
// outside the 320 x 240 buffer or send an out-of-range window to the panel.
void Core::Px(int x, int y, uint16_t c)
{
    if(x >= 0 && x < Canvas::kW && y >= 0 && y < Canvas::kH)
        fb_[y * Canvas::kW + x] = c;
}

void Core::FillRect(int x, int y, int w, int h, uint16_t c)
{
    const int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    const int x1 = x + w > Canvas::kW ? Canvas::kW : x + w, y1 = y + h > Canvas::kH ? Canvas::kH : y + h;
    for(int yy = y0; yy < y1; yy++)
        for(int xx = x0; xx < x1; xx++)
            fb_[yy * Canvas::kW + xx] = c;
}

void Core::FrameRect(int x, int y, int w, int h, uint16_t c)
{
    FillRect(x, y, w, 1, c);
    FillRect(x, y + h - 1, w, 1, c);
    FillRect(x, y, 1, h, c);
    FillRect(x + w - 1, y, 1, h, c);
}

void Core::Line(int x0, int y0, int x1, int y1, uint16_t c)
{
    // clamp far-off ends first (a runaway value can't make a 4-billion-step loop)
    auto cl = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    x0 = cl(x0, -8, Canvas::kW + 8), x1 = cl(x1, -8, Canvas::kW + 8);
    y0 = cl(y0, -8, Canvas::kH + 8), y1 = cl(y1, -8, Canvas::kH + 8);
    const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int       err = dx + dy;
    for(;;)
    {
        Px(x0, y0, c);
        if(x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if(e2 >= dy)
            err += dy, x0 += sx;
        if(e2 <= dx)
            err += dx, y0 += sy;
    }
}

void Core::HLineDots(int x0, int x1, int y, int every, uint16_t c)
{
    for(int x = x0; x <= x1; x += every)
        Px(x, y, c);
}

void Core::VLineDots(int x, int y0, int y1, int every, uint16_t c)
{
    if(y0 > y1)
    {
        const int t = y0;
        y0 = y1, y1 = t;
    }
    for(int y = y0; y <= y1; y += every)
        Px(x, y, c);
}

int Core::TextFb(int x, int y, const char* s, const FontDef& f, uint16_t fg, int bg)
{
    for(; *s; s++)
    {
        RasterChar(*s, f, 1, [&](int col, int row, bool on) {
            if(on)
                Px(x + col, y + row, fg);
            else if(bg >= 0)
                Px(x + col, y + row, uint16_t(bg));
        });
        x += f.FontWidth;
    }
    return x;
}

int Core::TextW(const char* s, const FontDef& f) const { return int(strlen(s)) * f.FontWidth; }

void Core::Dirty(int x, int y, int w, int h)
{
    // clip to the screen: the panel only ever gets valid windows
    if(x < 0)
        w += x, x = 0;
    if(y < 0)
        h += y, y = 0;
    if(x + w > Canvas::kW)
        w = Canvas::kW - x;
    if(y + h > Canvas::kH)
        h = Canvas::kH - y;
    if(w <= 0 || h <= 0)
        return;
    constexpr int kMax = int(sizeof(dirty_) / sizeof(dirty_[0]));
    if(n_dirty_ < kMax)
    {
        dirty_[n_dirty_++] = {x, y, w, h};
        return;
    }
    Rect&     r  = dirty_[kMax - 1]; // full: grow the last one
    const int x0 = r.x < x ? r.x : x, y0 = r.y < y ? r.y : y;
    const int x1 = r.x + r.w > x + w ? r.x + r.w : x + w, y1 = r.y + r.h > y + h ? r.y + r.h : y + h;
    r            = {x0, y0, x1 - x0, y1 - y0};
}

// ------------------------------------------------------------------ colours
uint16_t Core::Pearl(float h)
{
    // white -> cream -> pink -> lavender -> sky -> mint -> cream -> (white)
    static const uint8_t stops[7][3] = {{255, 255, 255}, {255, 243, 214}, {255, 168, 222}, {206, 172, 255},
                                        {150, 212, 255}, {158, 255, 232}, {255, 240, 205}};
    h -= floorf(h);
    const float s = h * 7.f;
    const int   i = int(s) % 7, j = (i + 1) % 7;
    const float f = s - floorf(s);
    auto        mix = [&](int c) { return uint8_t(float(stops[i][c]) + (float(stops[j][c]) - float(stops[i][c])) * f); };
    return Canvas::Rgb(mix(0), mix(1), mix(2));
}

uint16_t Core::Heat(float v)
{
    static const uint8_t stops[6][3] = {{0, 0, 40}, {0, 0, 170}, {0, 150, 230}, {120, 255, 240}, {220, 170, 255}, {255, 255, 255}};
    v             = Clampf(v, 0.f, 1.f) * 5.f;
    const int   i = v >= 5.f ? 4 : int(v);
    const float f = v - float(i);
    auto        mix = [&](int c) { return uint8_t(float(stops[i][c]) + (float(stops[i + 1][c]) - float(stops[i][c])) * f); };
    return Canvas::Rgb(mix(0), mix(1), mix(2));
}

// A slow pearl shimmer around a focused box: 2 px of the palette, drifting around the edge.
void Core::PearlBorder(const Rect& r, uint32_t now)
{
    const int   per = 2 * (r.w + r.h);
    const float t   = float(now % 4000) / 4000.f;
    auto        put = [&](int x, int y, int k) {
        const uint16_t c = Pearl(float(k) / float(per) + t);
        Px(x, y, c);
        Px(x + (x == r.x ? 1 : x == r.x + r.w - 1 ? -1 : 0), y + (y == r.y ? 1 : y == r.y + r.h - 1 ? -1 : 0), c);
    };
    int k = 0;
    for(int x = r.x; x < r.x + r.w; x++)
        put(x, r.y, k++);
    for(int y = r.y; y < r.y + r.h; y++)
        put(r.x + r.w - 1, y, k++);
    for(int x = r.x + r.w - 1; x >= r.x; x--)
        put(x, r.y + r.h - 1, k++);
    for(int y = r.y + r.h - 1; y >= r.y; y--)
        put(r.x, y, k++);
}

// ------------------------------------------------------------------ outline animations
static float EaseOut(float t)
{
    t = Clampf(t, 0.f, 1.f);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}
static float EaseIn(float t)
{
    t = Clampf(t, 0.f, 1.f);
    return t * t;
}
// draw-on from the top centre around both sides, hold, then erase the same way
static bool Timeline(float t, float& c0, float& c1)
{
    if(t < 0.f || t >= 1.f)
        return false;
    c1 = EaseOut(t / 0.4f);
    c0 = t > 0.6f ? EaseIn((t - 0.6f) / 0.4f) : 0.f;
    return c1 > c0;
}

void Core::FlashBox(int knob, uint32_t now)
{
    if(a_box_.on && a_box_.index == knob)
    {
        const float t = float(now - a_box_.t0) / float(a_box_.ms);
        if(t < 0.6f) // still turning: keep the outline fully drawn
        {
            if(t > 0.4f)
                a_box_.t0 = now - uint32_t(0.4f * float(a_box_.ms));
            return;
        }
    }
    a_box_.on = true, a_box_.index = knob, a_box_.t0 = now, a_box_.ms = kBoxAnimMs;
}

void Core::FlashTab(int i, uint32_t now) { a_tab_.on = true, a_tab_.index = i, a_tab_.t0 = now, a_tab_.ms = kTabAnimMs; }

void Core::StartWipe(const Rect& from, const Rect& to, uint32_t now)
{
    a_wipe_.on = true, a_wipe_.t0 = now, a_wipe_.ms = kWipeMs, a_wipe_.from = from, a_wipe_.to = to;
}

int Core::CollectOutlines(Outline* o, uint32_t now) const
{
    int   n = 0;
    float c0, c1;
    if(a_boot_.on)
    {
        const float tb = float(now - a_boot_.t0) / float(a_boot_.ms);
        if(Timeline(tb, c0, c1))
            o[n++] = {{0, 0, Canvas::kW, Canvas::kH}, 3, c0, c1};
        if(screen_ == HOME)
            for(int i = 0; i < 8; i++) // page 1
            {
                static const int order[8] = {0, 1, 2, 3, 7, 6, 5, 4}; // around the grid
                const float      t = (float(now - a_boot_.t0) - 200.f - float(order[i]) * 110.f) / 950.f;
                if(Timeline(t, c0, c1))
                    o[n++] = {TabRect(i), 3, c0, c1};
            }
    }
    if(a_wipe_.on)
    {
        const float t = float(now - a_wipe_.t0) / float(a_wipe_.ms);
        if(t < 1.f)
        {
            const float e  = EaseOut(t / 0.7f);
            auto        lp = [e](int a, int b) { return a + int(float(b - a) * e + 0.5f); };
            const Rect  r  = {lp(a_wipe_.from.x, a_wipe_.to.x), lp(a_wipe_.from.y, a_wipe_.to.y), lp(a_wipe_.from.w, a_wipe_.to.w),
                              lp(a_wipe_.from.h, a_wipe_.to.h)};
            c1             = EaseOut(t / 0.25f);
            c0             = t > 0.7f ? EaseIn((t - 0.7f) / 0.3f) : 0.f;
            if(c1 > c0)
                o[n++] = {r, 4, c0, c1};
        }
    }
    if(a_tab_.on && screen_ == HOME && a_tab_.index / 8 == focus_ / 8 && Timeline(float(now - a_tab_.t0) / float(a_tab_.ms), c0, c1))
        o[n++] = {TabRect(a_tab_.index), 3, c0, c1};
    if(a_box_.on && screen_ == PAGE && Timeline(float(now - a_box_.t0) / float(a_box_.ms), c0, c1))
        o[n++] = {ParamRect(a_box_.index), 3, c0, c1};
    return n;
}

void Core::AnimFrame(uint32_t now)
{
    Outline cur[16];
    const int n = CollectOutlines(cur, now);
    // retire finished animations
    if(a_boot_.on && now - a_boot_.t0 >= a_boot_.ms)
        a_boot_.on = false;
    if(a_wipe_.on && now - a_wipe_.t0 >= a_wipe_.ms)
        a_wipe_.on = false;
    if(a_tab_.on && now - a_tab_.t0 >= a_tab_.ms)
        a_tab_.on = false;
    if(a_box_.on && now - a_box_.t0 >= a_box_.ms)
        a_box_.on = false;
    // re-push where the outlines were (fb_ underneath is clean) and where they are now
    auto strips = [this](const Outline& o) {
        const Rect& r = o.r;
        Dirty(r.x, r.y, r.w, o.thick);
        Dirty(r.x, r.y + r.h - o.thick, r.w, o.thick);
        Dirty(r.x, r.y + o.thick, o.thick, r.h - 2 * o.thick);
        Dirty(r.x + r.w - o.thick, r.y + o.thick, o.thick, r.h - 2 * o.thick);
    };
    for(int i = 0; i < n_prev_ol_; i++)
        strips(prev_ol_[i]);
    for(int i = 0; i < n; i++)
        strips(cur[i]), prev_ol_[i] = cur[i];
    n_prev_ol_ = n;
    ol_clock_  = now;
    last_anim_ = now;
}

void Core::Compose(uint16_t* dst, int x, int y, int w, int h) const
{
    for(int i = 0; i < n_prev_ol_; i++)
    {
        const Outline& o = prev_ol_[i];
        const Rect&    r = o.r;
        const int      ix0 = x > r.x ? x : r.x, iy0 = y > r.y ? y : r.y;
        const int      ix1 = (x + w < r.x + r.w ? x + w : r.x + r.w), iy1 = (y + h < r.y + r.h ? y + h : r.y + r.h);
        if(ix0 >= ix1 || iy0 >= iy1)
            continue;
        const int   per  = 2 * (r.w + r.h);
        const float half = float(per) * 0.5f, pc = float(r.w) * 0.5f;
        const float flow = float(ol_clock_ % 6000) / 6000.f;
        for(int yy = iy0; yy < iy1; yy++)
        {
            const bool top = yy < r.y + o.thick, bottom = yy >= r.y + r.h - o.thick;
            for(int xx = ix0; xx < ix1; xx++)
            {
                const bool left = xx < r.x + o.thick, right = xx >= r.x + r.w - o.thick;
                if(!(top || bottom || left || right))
                {
                    xx = r.x + r.w - o.thick - 1; // skip the inside of the row
                    continue;
                }
                float p; // position clockwise from the top-left corner
                if(top)
                    p = float(xx - r.x);
                else if(bottom)
                    p = float(r.w + r.h) + float(r.x + r.w - 1 - xx);
                else if(right)
                    p = float(r.w) + float(yy - r.y);
                else
                    p = float(2 * r.w + r.h) + float(r.y + r.h - 1 - yy);
                float dd = fabsf(p - pc);
                if(dd > half)
                    dd = float(per) - dd;
                const float d = dd / half;
                if(d < o.c0 || d >= o.c1)
                    continue;
                dst[(yy - y) * w + (xx - x)] = Pearl(p / float(per) * 1.5f - flow * 3.f);
            }
        }
    }
}

int Core::Push(Canvas& c)
{
    static uint16_t tmp[Canvas::kW * 12]; // UI thread only
    int             pushed = 0;
    for(int i = 0; i < n_dirty_; i++)
    {
        const Rect& r    = dirty_[i];
        const int   rows = int(sizeof(tmp) / sizeof(tmp[0])) / r.w;
        for(int y = r.y; y < r.y + r.h; y += rows)
        {
            const int h = (r.y + r.h - y) < rows ? (r.y + r.h - y) : rows;
            for(int k = 0; k < h; k++)
                memcpy(tmp + k * r.w, fb_ + (y + k) * Canvas::kW + r.x, size_t(r.w) * 2);
            Compose(tmp, r.x, y, r.w, h);
            if(fade_ < 0.999f) // fading in from black
            {
                const int f = int(fade_ * 256.f);
                for(int k = 0; k < r.w * h; k++)
                {
                    const uint16_t p = tmp[k];
                    tmp[k] = uint16_t(((((p >> 11) & 31) * f >> 8) << 11) | ((((p >> 5) & 63) * f >> 8) << 5) | ((p & 31) * f >> 8));
                }
            }
            c.Blit(r.x, y, r.w, h, tmp, r.w);
        }
        pushed += r.w * r.h;
    }
    n_dirty_ = 0;
    return pushed;
}

// ------------------------------------------------------------------ display rest
// After 5 idle minutes the picture is replaced by a near-black screen with a small drifting mark
// (no static image burning in); after 20 the backlight goes off. Any control wakes it.
void Core::DrawRest(uint32_t now)
{
    last_rest_draw_ = now;
    FillRect(rest_rect_.x, rest_rect_.y, rest_rect_.w, rest_rect_.h, 0);
    Dirty(rest_rect_.x, rest_rect_.y, rest_rect_.w, rest_rect_.h);
    const uint32_t slot = now / 20000u; // a new place every 20 s
    const int      x = int((slot * 97u) % 240u) + 8, y = int((slot * 57u) % 190u) + 12;
    rest_rect_       = {x, y, 72, 24};
    TextFb(x + 4, y + 2, "pg-1", Font_7x10, Pearl(float(now % 20000) / 20000.f));
    const float lv = Clampf((20.f * log10f(fmaxf(out_pk_[0], out_pk_[1]) + 1e-9f) + 60.f) / 60.f, 0.f, 1.f);
    FillRect(x + 4, y + 16, int(60.f * lv), 2, Canvas::Rgb(0, 0, 120));
    Dirty(x, y, 72, 24);
}

// ------------------------------------------------------------------ touch d-pad
// Big on-screen buttons for when the touch targets feel small and the knobs aren't wanted. On / off in
// config (off by default); tap the page title to show it; it hides itself after 10 s unused.
Core::Rect Core::PadRect(int b) const
{
    const int s = 30, cx = 247, cy = 85; // the middle button's corner (the cross sits right of centre)
    static const int dx[5] = {0, -1, 0, 1, 0}, dy[5] = {-1, 0, 0, 0, 1};
    return {cx + dx[b] * s, cy + dy[b] * s, s - 2, s - 2};
}

int Core::PadButtonAt(int x, int y) const
{
    for(int b = 0; b < 5; b++)
    {
        const Rect r = PadRect(b);
        if(x >= r.x - 2 && x < r.x + r.w + 2 && y >= r.y - 2 && y < r.y + r.h + 2)
            return b;
    }
    return -1;
}

void Core::PadAction(int b, bool down, uint32_t now)
{
    pad_use_t_ = now, pad_dirty_ = true;
    if(screen_ == HOME) // arrows move around the tab grid, the middle opens
    {
        if(!down)
        {
            if(b == 2)
                OpenTab(focus_, now);
            return;
        }
        int f = focus_;
        if(b == 1)
            f = (f + kTabs - 1) % kTabs;
        else if(b == 3)
            f = (f + 1) % kTabs;
        else if(b == 0 && (f % 8) >= 4)
            f -= 4;
        else if(b == 4 && (f % 8) < 4 && f + 4 < kTabs)
            f += 4;
        if(f != focus_)
            focus_ = f, FlashTab(f, now);
        return;
    }
    if(b == 1 || b == 3) // pick a value box
    {
        if(down)
            pad_sel_ = (pad_sel_ + (b == 3 ? 1 : kKnobs - 1)) % kKnobs, redraw_panel_ = true;
        return;
    }
    if(b == 0 || b == 4) // change it, like turning its knob (not affected by "knobs: reverse")
    {
        if(down)
            KnobTurn(pad_sel_, (b == 0 ? 1 : -1) * (params_[P_CF_KNOBS].value ? -1 : 1), now);
        return;
    }
    KnobPress(pad_sel_, down, now); // the middle = that knob's push (hold it: home)
}

void Core::DrawPad(uint32_t now)
{
    (void)now;
    int x0 = Canvas::kW, y0 = Canvas::kH, x1 = 0, y1 = 0;
    for(int b = 0; b < 5; b++)
    {
        const Rect r = PadRect(b);
        const bool held = b == pad_btn_;
        FillRect(r.x, r.y, r.w, r.h, held ? ui::kYellow : ui::kBlueDeep);
        FrameRect(r.x, r.y, r.w, r.h, ui::kCream);
        const uint16_t c = held ? ui::kBlue : ui::kCream;
        const int      cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        for(int i = 0; i < 6; i++) // a solid arrow (or a dot in the middle)
        {
            if(b == 0)
                FillRect(cx - i, cy - 4 + i, 2 * i + 1, 1, c);
            else if(b == 4)
                FillRect(cx - i, cy + 4 - i, 2 * i + 1, 1, c);
            else if(b == 1)
                FillRect(cx - 4 + i, cy - i, 1, 2 * i + 1, c);
            else if(b == 3)
                FillRect(cx + 4 - i, cy - i, 1, 2 * i + 1, c);
        }
        if(b == 2)
            FillRect(cx - 4, cy - 4, 9, 9, c);
        x0 = r.x < x0 ? r.x : x0, y0 = r.y < y0 ? r.y : y0;
        x1 = r.x + r.w > x1 ? r.x + r.w : x1, y1 = r.y + r.h > y1 ? r.y + r.h : y1;
    }
    Dirty(x0, y0, x1 - x0, y1 - y0);
}

// ------------------------------------------------------------------ first power-up tour
void Core::DrawTour(uint32_t now)
{
    static const char* const lines[4][2] = {
        {"welcome to your pg-1", "turn any knob to move, push it to open"},
        {"fs-1 bypass  fs-2 next  fs-3 a/b", "hold fs-2 on a tab to switch it on/off"},
        {"more tabs on page 2", "turn past the last tab, or tap the dots"},
        {"you're set: it saves itself", "a '!' in the title = something to check"},
    };
    const int step = int((now - tour_t0_) / 3200u);
    if(step == tour_drawn_ || step > 3)
        return;
    if(tour_drawn_ >= 0)
        DrawHome(now); // clear the last card
    tour_drawn_ = step;
    const Rect r = {16, 90, Canvas::kW - 32, 58};
    FillRect(r.x, r.y, r.w, r.h, ui::kBlueDeep);
    FrameRect(r.x, r.y, r.w, r.h, ui::kCream);
    PearlBorder({r.x + 2, r.y + 2, r.w - 4, r.h - 4}, now);
    TextFb(r.x + (r.w - TextW(lines[step][0], Font_7x10)) / 2, r.y + 12, lines[step][0], Font_7x10, ui::kWhite);
    TextFb(r.x + (r.w - TextW(lines[step][1], Font_6x8)) / 2, r.y + 30, lines[step][1], Font_6x8, ui::kCream);
    for(int k = 0; k < 4; k++) // which card of 4
        FillRect(r.x + r.w / 2 - 22 + k * 12, r.y + r.h - 10, 8, 3, k == step ? ui::kYellow : ui::kGrid);
    Dirty(r.x, r.y, r.w, r.h);
}

// ------------------------------------------------------------------ title + home
void Core::DrawTitle(uint32_t)
{
    FillRect(0, 0, Canvas::kW, 20, ui::kGrey);
    char buf[24];
    if(screen_ == HOME)
        TextFb(6, 5, "pg audio pg-1", Font_7x10, ui::kBlue);
    else
    {
        TextFb(6, 5, "< home", Font_7x10, ui::kBlue);
        const char* t = ui::kTabTitles[tab_];
        TextFb((Canvas::kW - TextW(t, Font_7x10)) / 2, 5, t, Font_7x10, ui::kBlue);
    }
    // right chip: on home the whole pedal, on a page this stage
    bool lit = effect_on_;
    if(!effect_on_)
        snprintf(buf, sizeof(buf), "bypass");
    else if(screen_ == HOME || tab_ == T_VIS)
        snprintf(buf, sizeof(buf), "on");
    else if(tab_ == T_SAFETY)
        snprintf(buf, sizeof(buf), "always on");
    else
        snprintf(buf, sizeof(buf), "%s", StageOn(tab_) ? "on" : "off"), lit = StageOn(tab_);
    if(!effect_on_ && screen_ == PAGE && tab_ == T_SAFETY)
        snprintf(buf, sizeof(buf), "always on"), lit = true;
    const int w = TextW(buf, Font_7x10);
    int chip_x = Canvas::kW - w - 14; // further chips go to the left of the on/off chip
    auto chip  = [&](const char* t, uint16_t bg) {
        const int cw = TextW(t, Font_7x10);
        chip_x -= cw + 10;
        FillRect(chip_x, 3, cw + 8, 14, bg);
        TextFb(chip_x + 4, 5, t, Font_7x10, ui::kBlue);
    };
    if(clip_lit_) // the source is hotter than the converter: turn it down at the source
        chip("in clip", ui::kYellow);
    if(ab_) // fs-3: listening to the untouched input
        chip("a/b dry", ui::kCyan);
    alert_chip_ = {0, 0, 0, 0};
    if(drawn_alerts_ > 0) // health: something looks off (tap it for the health page)
    {
        bool fault = false;
        for(int k = 0; k < kChecks; k++)
            fault = fault || (hs_[k].on && CheckSev(k) == 2);
        char a[8];
        snprintf(a, sizeof(a), "! %d", drawn_alerts_);
        chip(a, fault ? ui::kPink : ui::kYellow);
        alert_chip_ = {chip_x, 0, TextW(a, Font_7x10) + 8, 20};
    }
    FillRect(Canvas::kW - w - 12, 3, w + 8, 14, lit ? ui::kBlue : ui::kGreyDark);
    TextFb(Canvas::kW - w - 8, 5, buf, Font_7x10, lit ? ui::kYellow : ui::kGrey);
    Dirty(0, 0, Canvas::kW, 20);
}

Core::Rect Core::TabRect(int i) const { return {8 + (i % 4) * 78, (i % 8) < 4 ? 28 : 124, 72, 90}; }

void Core::DrawIcon(int i, int cx, int cy, uint16_t c)
{
    const uint16_t dim = ui::kDimText;
    switch(i)
    {
        case T_HUM: // a wave with notches pulled out of it
            for(int x = -18; x < 18; x++)
                Px(cx + x, cy + 2 - int(5.f * sinf(float(x) * 0.5f)), dim);
            for(int k = 0; k < 4; k++)
                Line(cx - 14 + k * 9, cy - 12, cx - 14 + k * 9, cy + 12 - k * 4, c);
            break;
        case T_EQ: // a bell curve over spectrum bars
            for(int k = 0; k < 7; k++)
                FillRect(cx - 17 + k * 5, cy + 10 - (k * 7 % 9), 3, 4 + (k * 7 % 9), ui::kSpecFill);
            for(int x = -18; x < 18; x++)
            {
                const int y = cy - 2 - int(9.f * expf(-float(x * x) / 40.f));
                Px(cx + x, y, c), Px(cx + x, y + 1, c);
            }
            break;
        case T_COMP: // a knee
            Line(cx - 16, cy + 12, cx, cy - 2, c), Line(cx, cy - 2, cx + 16, cy - 8, c);
            Line(cx - 16, cy + 12, cx + 16, cy - 20, dim);
            break;
        case T_CLARITY: // a deep low hump and bright sparkle
            for(int x = -18; x < 2; x++)
                Px(cx + x, cy + 10 - int(14.f * expf(-float((x + 9) * (x + 9)) / 30.f)), c), Px(cx + x, cy + 11, dim);
            for(int k = 0; k < 4; k++)
            {
                const int sx = cx + 10 + (k % 2) * 6, sy = cy - 10 + k * 5;
                Px(sx, sy, c), Px(sx - 1, sy, c), Px(sx + 1, sy, c), Px(sx, sy - 1, c), Px(sx, sy + 1, c);
            }
            break;
        case T_SAT: // a squashed sine
            for(int x = -18; x < 18; x++)
            {
                const float s = sinf(float(x) * 0.35f) * 2.2f;
                Px(cx + x, cy - int(12.f * s / (1.f + fabsf(s))), c);
            }
            break;
        case T_DEHARSH: // a spiky line with its peaks smoothed off
            for(int k = 0; k < 6; k++)
            {
                const int x0 = cx - 18 + k * 6, h = (k % 2) ? 14 : 4;
                Line(x0, cy + 8, x0 + 3, cy + 8 - h, dim), Line(x0 + 3, cy + 8 - h, x0 + 6, cy + 8, dim);
            }
            Line(cx - 18, cy - 1, cx + 18, cy - 1, c), Line(cx - 18, cy, cx + 18, cy, c);
            break;
        case T_SAFETY: // a shield
            for(int y = -14; y <= 14; y++)
            {
                const int half = y < 2 ? 12 : 12 - (y - 2);
                Px(cx - half, cy + y, c), Px(cx + half, cy + y, c);
            }
            Line(cx - 12, cy - 14, cx + 12, cy - 14, c);
            Line(cx - 5, cy, cx - 1, cy + 5, c), Line(cx - 1, cy + 5, cx + 6, cy - 5, c);
            break;
        case T_INPUT: // a small wave and a big one brought to the same height
            for(int x = -18; x < 18; x++)
            {
                const float a = x < 0 ? 4.f : 11.f;
                Px(cx + x, cy - int(a * sinf(float(x) * 0.7f)), x < 0 ? dim : c);
            }
            Line(cx - 18, cy - 11, cx + 18, cy - 11, c), Line(cx - 18, cy + 11, cx + 18, cy + 11, c);
            break;
        case T_TAKEBACK: // an arrow coming back round
            for(int a = 30; a < 300; a += 6)
            {
                const float r = 11.f, t = float(a) * kPi / 180.f;
                Px(cx + int(r * cosf(t)), cy - int(r * sinf(t)), c), Px(cx + int(r * cosf(t)), cy - int(r * sinf(t)) + 1, c);
            }
            Line(cx + 9, cy - 6, cx + 10, cy + 1, c), Line(cx + 9, cy - 6, cx + 3, cy - 3, c);
            break;
        case T_WIDTH: // two arrows spreading apart
            Line(cx - 3, cy, cx - 17, cy, c), Line(cx - 17, cy, cx - 12, cy - 5, c), Line(cx - 17, cy, cx - 12, cy + 5, c);
            Line(cx + 3, cy, cx + 17, cy, c), Line(cx + 17, cy, cx + 12, cy - 5, c), Line(cx + 17, cy, cx + 12, cy + 5, c);
            FillRect(cx - 1, cy - 8, 2, 16, dim);
            break;
        case T_LOUD: // a speaker with waves
            FillRect(cx - 14, cy - 4, 5, 8, c);
            Line(cx - 9, cy - 4, cx - 3, cy - 10, c), Line(cx - 9, cy + 4, cx - 3, cy + 10, c), Line(cx - 3, cy - 10, cx - 3, cy + 10, c);
            for(int w = 0; w < 3; w++)
                for(int a = -40; a <= 40; a += 8)
                {
                    const float t = float(a) * kPi / 180.f, r = 5.f + float(w) * 5.f;
                    Px(cx + int(r * cosf(t)), cy + int(r * sinf(t)), w ? dim : c);
                }
            break;
        case T_MBAND: // four bands, each with its own ceiling
            for(int k = 0; k < 4; k++)
            {
                const int x = cx - 16 + k * 9, h = 8 + ((k * 7 + 3) % 4) * 4;
                FillRect(x, cy + 12 - h, 6, h, k % 2 ? dim : c);
                FillRect(x - 1, cy + 12 - h - 4, 8, 1, c);
            }
            break;
        case T_CONFIG: // sliders
            for(int k = 0; k < 3; k++)
            {
                Line(cx - 14, cy - 8 + k * 8, cx + 14, cy - 8 + k * 8, dim);
                FillRect(cx - 10 + k * 9, cy - 10 + k * 8, 4, 5, c);
            }
            break;
        case T_HEALTH: // a heartbeat line
        {
            const int px[] = {-18, -8, -5, -2, 2, 5, 8, 18}, py[] = {0, 0, -12, 12, -6, 0, 0, 0};
            for(int k = 0; k < 7; k++)
                Line(cx + px[k], cy + py[k], cx + px[k + 1], cy + py[k + 1], c);
            break;
        }
        default: // visualizer: bars in the waterfall colours
            for(int k = 0; k < 8; k++)
            {
                const int h = 6 + ((k * 5 + 3) % 9) * 2;
                for(int y = 0; y < h; y++)
                    Px(cx - 17 + k * 5, cy + 12 - y, Heat(float(y) / 24.f)), Px(cx - 16 + k * 5, cy + 12 - y, Heat(float(y) / 24.f));
            }
            break;
    }
}

void Core::DrawTab(int i, uint32_t now)
{
    const Rect r     = TabRect(i);
    const bool focus = i == focus_;
    const bool on    = StageOn(i);
    FillRect(r.x, r.y, r.w, r.h, ui::kBlue);
    FrameRect(r.x, r.y, r.w, r.h, ui::kGrey);
    FrameRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, focus ? ui::kYellow : ui::kLtBlue);
    DrawIcon(i, r.x + r.w / 2, r.y + 34, focus ? ui::kYellow : (on ? ui::kCyan : ui::kDimText));
    const char* name = ui::kTabNames[i];
    TextFb(r.x + (r.w - TextW(name, Font_7x10)) / 2, r.y + 60, name, Font_7x10, focus ? ui::kYellow : ui::kWhite);
    const int   alerts = ActiveAlerts();
    const char* st     = i == T_VIS ? "view" : (i == T_SAFETY ? "always" : (i == T_CONFIG ? "setup" : (on ? "on" : "off")));
    uint16_t    stc    = on ? ui::kCyan : ui::kDimText;
    if(i == T_HEALTH)
        st = alerts ? "check!" : "all ok", stc = alerts ? ui::kYellow : ui::kCyan;
    TextFb(r.x + (r.w - TextW(st, Font_6x8)) / 2, r.y + 75, st, Font_6x8, stc);
    if(focus)
        PearlBorder(r, now);
    Dirty(r.x, r.y, r.w, r.h);
    pad_dirty_ = true; // (the d-pad sits on top of the tabs)
}

void Core::DrawHome(uint32_t now)
{
    FillRect(0, 20, Canvas::kW, Canvas::kH - 20, ui::kGrey);
    const int page = focus_ / 8;
    for(int i = page * 8; i < kTabs && i < page * 8 + 8; i++)
        DrawTab(i, now);
    DrawFooter(now);
    drawn_page_ = page;
    drawn_focus_ = focus_;
    Dirty(0, 20, Canvas::kW, Canvas::kH - 20);
}

// ------------------------------------------------------------------ frame
int Core::DrawUi(Canvas& c, uint32_t now)
{
    if(fb_ == nullptr)
        return 0;
    if(pending_boot_) // power-on: the tabs appear, each traced by a pearl outline
    {
        pending_boot_ = false;
        last_input_   = now;
        screen_       = HOME;
        splash_on_ = true, splash_t0_ = now, splash_ui_drawn_ = false, fade_ = 1.f; // the logo first
        redraw_all_ = false;
    }
    if(splash_on_)
    {
        DrawSplash(now);
        if(!splash_on_ && params_[P_TOUR_DONE].value == 0) // the very first power-up: a short tour next
            tour_on_ = true, tour_t0_ = now + 2400, tour_drawn_ = -1;
        return Push(c);
    }
    if(tour_on_)
    {
        if(tour_skip_ || now - tour_t0_ > 4u * 3200u && int32_t(now - tour_t0_) > 0)
        {
            tour_on_ = false, tour_skip_ = false;
            SetParam(P_TOUR_DONE, 1); // never again (it's saved)
            if(screen_ == HOME)
                redraw_all_ = true;
        }
        else if(screen_ == HOME && int32_t(now - tour_t0_) >= 0)
            DrawTour(now);
    }

    // ---- display rest
    if(wake_)
    {
        wake_ = false;
        if(resting_)
            resting_ = false, redraw_all_ = true, backlight_ = true;
    }
    if(!resting_ && now - last_input_ > kRestMs)
    {
        resting_ = true;
        FillRect(0, 0, Canvas::kW, Canvas::kH, 0);
        Dirty(0, 0, Canvas::kW, Canvas::kH);
        rest_rect_      = {0, 0, 0, 0};
        last_rest_draw_ = 0;
        n_prev_ol_      = 0;
        a_boot_.on = a_wipe_.on = a_tab_.on = a_box_.on = false;
    }
    if(resting_)
    {
        backlight_ = now - last_input_ < kDarkMs;
        if(backlight_ && now - last_rest_draw_ >= 500)
            DrawRest(now);
        return Push(c);
    }
    backlight_ = true;
    UpdateHealth(now);
    if(pending_learn_)
        pending_learn_ = false, StartLearn(now);
    if(pending_load_ || pending_save_) // config tab
    {
        const int k = params_[P_CF_SLOT].value;
        if(pending_save_)
            TakeSnap(slots_[k]), slots_[k].used = 1, state_dirty_ = true, cfg_msg_ = "saved", cfg_msg_t0_ = now;
        else if(slots_[k].used)
            PutSnap(slots_[k]), cfg_msg_ = "loaded", cfg_msg_t0_ = now, redraw_all_ = true;
        else
            cfg_msg_ = "that config is empty: save into it first", cfg_msg_t0_ = now;
        pending_load_ = pending_save_ = false;
    }
    if(params_[P_CF_THEME].value != drawn_theme_) // theme changed: repaint everything in the new colours
    {
        drawn_theme_ = params_[P_CF_THEME].value;
        ui::SetTheme(drawn_theme_);
        redraw_all_ = true;
    }
    UpdateLearn(now);

    // ---- events from the controls (they may come from an interrupt): handled here, on the UI thread
    if(pending_home_)
        pending_home_ = false, GoHome(now);
    if(pending_open_ >= 0)
    {
        const int t   = pending_open_;
        pending_open_ = -1;
        OpenTab(t, now);
    }
    if(pending_stage_toggle_)
    {
        pending_stage_toggle_ = false;
        const int t           = screen_ == HOME ? focus_ : tab_;
        SetStageOn(t, !stage_on_[t]); // (safety, visual, health, config can't be switched off)
        if(screen_ == HOME)
            DrawTab(t, now), FlashTab(t, now);
    }
    if(pending_flash_kind_ >= 0)
    {
        if(pending_flash_kind_ == 0)
            FlashTab(pending_flash_index_, now);
        else
            FlashBox(pending_flash_index_, now);
        pending_flash_kind_ = -1;
    }

    const bool clip = clips_ != 0 && now - clip_t_ < 2000;
    if(clip != clip_lit_)
        clip_lit_ = clip, redraw_title_ = true;
    if(redraw_all_)
    {
        redraw_all_ = false;
        DrawTitle(now);
        if(screen_ == HOME)
            DrawHome(now);
        else
        {
            DrawPage(now);
            last_graph_ = now;
        }
        redraw_title_ = false, redraw_panel_ = false, param_changed_ = 0;
    }
    else
    {
        if(redraw_title_)
            redraw_title_ = false, DrawTitle(now);
        if(screen_ == HOME)
        {
            if(drawn_focus_ != focus_ && focus_ / 8 != drawn_page_) // turned onto the other page
                DrawHome(now);
            if(drawn_focus_ != focus_)
            {
                const int old = drawn_focus_;
                drawn_focus_  = focus_;
                if(old >= 0)
                    DrawTab(old, now);
                DrawTab(focus_, now);
            }
            if(footer_alert_ != (alert_check_ >= 0 && now - alert_t0_ < 5000)) // an alert line comes / goes
                DrawFooter(now);
            if(now - last_border_ >= 70 && !tour_on_) // the focused tab's pearl edge drifts slowly (not under the tour card)
            {
                last_border_ = now;
                const Rect r = TabRect(focus_);
                PearlBorder(r, now);
                pad_dirty_ = true;
                Dirty(r.x, r.y, r.w, 2), Dirty(r.x, r.y + r.h - 2, r.w, 2), Dirty(r.x, r.y, 2, r.h), Dirty(r.x + r.w - 2, r.y, 2, r.h);
            }
        }
        else
        {
            const bool waterfall = tab_ == T_VIS && params_[P_V_MODE].value == 1;
            if(now - last_graph_ >= (waterfall ? FrameMs() : FrameMs() + (ScreenFast() ? 0u : 10u))) // live graphs: ~60 fps fast
            {
                last_graph_ = now;
                DrawGraph(now);
                if(now - last_strip_ >= 66) // live numbers at ~15 a second (the graph gets the link's speed)
                {
                    last_strip_ = now;
                    if(tab_ != T_EQ)
                        DrawStrip(now);
                    else
                        DrawParamBox(2, now), DrawParamBox(3, now); // live level / movement in the boxes
                }
            }
            if(redraw_panel_ || drawn_band_ != band_)
            {
                redraw_panel_ = false, param_changed_ = 0;
                DrawStrip(now);
                for(int k = 0; k < kKnobs; k++)
                    DrawParamBox(k, now);
            }
            else
            {
                const uint32_t ch = param_changed_;
                param_changed_ &= ~ch;
                for(int k = 0; k < kKnobs; k++)
                    if(ch & (1u << k))
                        DrawParamBox(k, now);
            }
        }
    }
    if(pad_shown_ && ((!params_[P_DPAD].value) || (pad_btn_ < 0 && now - pad_use_t_ > 10000))) // d-pad: hide after 10 s
        pad_shown_ = false, redraw_all_ = true;
    if(pad_shown_ && pad_dirty_)
        pad_dirty_ = false, DrawPad(now);
    if(now - last_anim_ >= FrameMs()) // outline animations at ~60 fps (fast) / 25 (safe)
        AnimFrame(now);
    if(repush_)
        repush_ = false, Dirty(0, 0, Canvas::kW, Canvas::kH);
    return Push(c);
}

// ------------------------------------------------------------------ touch
void Core::Touch(bool touching, int x, int y, uint32_t now)
{
    if(touching && !ts_.down) // finger down
    {
        if(Waking(now))
        {
            ts_.down = true, ts_.moved = true; // swallow this touch
            return;
        }
        if(params_[P_DPAD].value) // touch d-pad mode
        {
            const int b = pad_shown_ ? PadButtonAt(x, y) : -1;
            if(b >= 0 || (y < 20 && x >= 70 && x < Canvas::kW - 90))
            {
                ts_      = TouchState();
                ts_.down = true, ts_.moved = true, ts_.t0 = now; // (handled here, not as a normal tap)
                if(b >= 0)
                    pad_btn_ = b, pad_btn_t0_ = pad_rep_t_ = now, PadAction(b, true, now);
                else // the page title shows / hides the d-pad
                {
                    pad_shown_ = !pad_shown_, pad_use_t_ = now, pad_dirty_ = true;
                    if(!pad_shown_)
                        redraw_all_ = true;
                }
                return;
            }
        }
        ts_      = TouchState();
        ts_.down = true;
        ts_.x0 = ts_.x = x;
        ts_.y0 = ts_.y = y;
        ts_.t0         = now;
        ts_.node       = (screen_ == PAGE && tab_ == T_EQ) ? NodeAt(x, y) : -1;
        if(ts_.node >= 0 && ts_.node != band_)
            band_ = ts_.node, redraw_panel_ = true;
        return;
    }
    if(touching && ts_.down && pad_btn_ >= 0) // holding a d-pad arrow: repeat
    {
        if(pad_btn_ != 2 && now - pad_btn_t0_ > 400 && now - pad_rep_t_ >= 120)
            pad_rep_t_ = now, PadAction(pad_btn_, true, now);
        return;
    }
    if(!touching && ts_.down && pad_btn_ >= 0) // let go of a d-pad button
    {
        ts_.down = false;
        PadAction(pad_btn_, false, now);
        pad_btn_ = -1, pad_dirty_ = true;
        return;
    }
    if(touching && ts_.down) // finger moving: drag a band node (x = frequency, y = gain)
    {
        last_input_ = now;
        ts_.x = x, ts_.y = y;
        if(abs(x - ts_.x0) > 4 || abs(y - ts_.y0) > 4)
            ts_.moved = true;
        if(ts_.node >= 0 && ts_.moved)
        {
            const float hz   = FX(x < ui::kGx ? ui::kGx : (x >= ui::kGx + ui::kGw ? ui::kGx + ui::kGw - 1 : x), 20.f, 3.f);
            const int   semi = int(lroundf(12.f * log2f(hz / 20.f)));
            SetParam(ParamIndex(ts_.node, B_FREQ), semi);
            const float db = float(ui::kGy + ui::kGh / 2 - y) * 18.f / float(ui::kGh / 2 - 6);
            SetParam(ParamIndex(ts_.node, B_GAIN), int(lroundf(db * 10.f / 5.f)) * 5);
            redraw_panel_ = true;
        }
        return;
    }
    if(!touching && ts_.down) // finger up
    {
        ts_.down = false;
        if(ts_.moved)
            return;
        if(alert_chip_.w > 0 && y < 20 && x >= alert_chip_.x && x < alert_chip_.x + alert_chip_.w) // "! n": what's wrong?
        {
            for(int k = 0; k < kChecks; k++)
                if(hs_[k].on && CheckSev(k) >= 1)
                {
                    SetParam(P_H_ROW, k);
                    break;
                }
            if(!(screen_ == PAGE && tab_ == T_HEALTH))
                OpenTab(T_HEALTH, now);
            return;
        }
        const bool chip = y < 20 && x >= Canvas::kW - 80;
        if(screen_ == HOME)
        {
            if(chip)
            {
                effect_on_ = !effect_on_, redraw_title_ = true;
                return;
            }
            if(y >= 220 && x >= Canvas::kW - 40) // page dots: the other page
            {
                focus_ = focus_ / 8 + 1 < (kTabs + 7) / 8 ? (focus_ / 8 + 1) * 8 : 0;
                return;
            }
            for(int i = (focus_ / 8) * 8; i < kTabs && i < (focus_ / 8) * 8 + 8; i++)
            {
                const Rect r = TabRect(i);
                if(x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
                {
                    focus_ = i;
                    OpenTab(i, now);
                    return;
                }
            }
            return;
        }
        if(y < 20 && x < 70) // "< home"
        {
            GoHome(now);
            return;
        }
        if(chip) // this stage on / off
        {
            pending_stage_toggle_ = true;
            return;
        }
        if(tab_ == T_EQ && ts_.node >= 0)
        {
            if(ts_.node == last_tap_node_ && now - last_tap_time_ < 350) // double-tap a node: gain back to 0
            {
                SetParam(ParamIndex(ts_.node, B_GAIN), 0);
                redraw_panel_  = true;
                last_tap_node_ = -1;
            }
            else
                last_tap_node_ = ts_.node, last_tap_time_ = now;
            return;
        }
        if(tab_ == T_EQ && y >= 164 && y < 182) // band chips
        {
            const int b = (x - 4) / 79;
            if(b >= 0 && b < kBands)
                band_ = b, redraw_panel_ = true;
            return;
        }
        if(tab_ == T_CLARITY && y >= ui::kGy && y < ui::kGy + 16 && x >= ui::kGx + ui::kGw - 102) // "mode" button
        {
            SetParam(P_CL_MODE, (params_[P_CL_MODE].value + 1) % 3);
            param_changed_ |= 1u;
            return;
        }
        if(tab_ == T_CONFIG && y >= ui::kGy + 108 && y < ui::kGy + 124 && x < ui::kGx + 156) // screen fast / safe
        {
            SetParam(P_SCR_FAST, params_[P_SCR_FAST].value ? 0 : 1);
            redraw_all_ = true;
            return;
        }
        if(tab_ == T_CONFIG && y >= ui::kGy + 108 && y < ui::kGy + 124 && x >= ui::kGx + 160) // touch d-pad on / off
        {
            SetParam(P_DPAD, params_[P_DPAD].value ? 0 : 1);
            redraw_all_ = true;
            return;
        }
        if(tab_ == T_CONFIG && y >= ui::kGy && y < ui::kGy + 8 * 13 + 4 && x < ui::kGx + 156) // tap a config row to pick it
        {
            const int row = (y - ui::kGy - 4) / 13;
            if(row >= 0 && row < kSlots)
                SetParam(P_CF_SLOT, row);
            return;
        }
        if(tab_ == T_HUM && y >= ui::kGy && y < ui::kGy + 16 && x >= ui::kGx + ui::kGw - 56) // "learn" button
        {
            SetParam(P_HUM_MAINS, 3);
            StartLearn(now);
            return;
        }
        if(tab_ == T_VIS && y >= ui::kGy && y < ui::kGy + ui::kGh) // tap the display: next view
        {
            SetParam(P_V_MODE, (params_[P_V_MODE].value + 1) % 4);
            redraw_all_ = true;
            return;
        }
        for(int k = 0; k < kKnobs; k++) // a value box: what does this knob do?
        {
            const Rect r = ParamRect(k);
            if(x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
                FlashBox(k, now);
        }
    }
}
} // namespace pg
