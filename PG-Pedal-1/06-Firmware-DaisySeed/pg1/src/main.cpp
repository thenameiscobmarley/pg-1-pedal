// PG-1 firmware for the pg audio PG pedal (Daisy Seed3, libDaisy v9+).
//
// This file is only the hardware: it reads the knobs, footswitches and touchscreen, feeds the
// audio, and draws on the ILI9341. Everything the pedal *does* (parameters, pages, pop-ups,
// gestures, DSP) is in core/PgCore - the same code the Carla plugin (10-Carla-Plugin) runs.
//
// Controls (details in ../README.md):
//   home: 8 tabs (hum, dyn eq, comp, clarity, saturate, de-harsh, safety, visual). turn = move, push / tap = open
//   page: the 4 knobs = the 4 boxes; double-push = default; long-push = home; push = what does it do
//   fs-1 = bypass (safety stays on); fs-2 = next band (eq) / next page; hold fs-2 = this stage on / off
//   fs-3 = fair A/B: the untouched input at the same loudness (tap again to come back)
//   fs-1 + fs-2 held 2 s = USB flash mode
//
// Pin map matches 05-Wiring-and-Schematics/WIRING.md. Change it in ONE place: the pins block.
#include "daisy_seed.h"
#include "ili9341.h"
#include "isoaudio.h"
#include "../core/PgCore.h"
#include "util/PersistentStorage.h"
#include <cmath>
#include <cstring>

using namespace daisy;

extern "C" uint32_t HAL_GetUIDw0(void), HAL_GetUIDw1(void), HAL_GetUIDw2(void); // the chip's unique serial

// ------------------------------------------------------------------ pins
namespace pins
{
constexpr Pin kPg1A = seed::D1, kPg1B = seed::D2, kPg1Sw = seed::D3;
constexpr Pin kPg2A = seed::D4, kPg2B = seed::D5, kPg2Sw = seed::D6;
constexpr Pin kPg3A = seed::D13, kPg3B = seed::D14, kPg3Sw = seed::D15;
constexpr Pin kPg4A = seed::D19, kPg4B = seed::D20, kPg4Sw = seed::D21;
constexpr Pin kFs1 = seed::D17, kFs2 = seed::D18, kFs3 = seed::D9;
constexpr Pin kLcdCs = seed::D7, kLcdSck = seed::D8, kLcdMosi = seed::D10;
constexpr Pin kLcdDc = seed::D30, kLcdRst = seed::D0, kLcdLed = seed::D16;
// XPT2046 touch (screen pins 10-13), bit-banged so it never fights the screen's SPI; polled (no IRQ wire)
constexpr Pin kTClk = seed::D22, kTCs = seed::D23, kTDin = seed::D24, kTDout = seed::D29;
// v3 carrier board: I2C1 (codec through the ISO1540 + the knob ADC) and SAI2 (I2S through the ISO7741)
constexpr Pin kScl = seed::D11, kSda = seed::D12;
constexpr Pin kSaiFs = seed::D27, kSaiSck = seed::D28, kSaiTx = seed::D26, kSaiRx = seed::D25, kSaiMclk = seed::D24;
} // namespace pins

constexpr bool kFlipScreen     = false; // set true if the picture is upside down
constexpr bool kReverseEncoder = false; // set true if turning right makes values go down
// Touch calibration: raw 0-4095 readings at the screen edges. If taps land in the wrong place,
// flip these (swap first, then the flips) until the bar follows your finger.
constexpr bool kTouchSwapXY = true, kTouchFlipX = false, kTouchFlipY = false;
constexpr int  kTouchRawMin = 300, kTouchRawMax = 3800;
constexpr int  kTouchZMin   = 150; // pressure (Z1) above this = a finger is down

// ------------------------------------------------------------------ levels (v3 isolated carrier board)
// The core works in "pedal units": 1.0 = 2.0 V peak at a jack, in and out. Path sensitivity from the board:
//   in:  jack x0.5 (1M / 1M) -> buffer -> pg-line stage (x1 at its centre click, and it inverts) -> x1/3 (20k / 10k)
//        -> codec IN2 through 20k (-6 dB) -> ADC (0 dBFS = 0.707 V peak at 10k): ADC = -V_jack * 0.118
//   out: DAC 0 dBFS -> about 1.41 V peak on the line outputs -> pg-hp stage (x1 at its centre click, inverting) ->
//        TPA6139A2 x-2: out jack = DAC * 2.82 V peak
// So with both knobs on their centre clicks, what comes out is as loud as what went in, processed. The firmware's two
// gains below are FIXED (no auto-ranging): measure once on the real board and correct them here if needed.
// ADC -> pedal units (1.0 = 2 V peak at the jack); the minus undoes the inversion. 0.1883 = codec volts per jack volt
// at pg-line's centre click, from the board simulation (10-Carla-Plugin/Source/sim AC analysis): input divider 1M / 470k
// (x0.32: clean to ~3.4 V rms at the jack) and 4.7k / 10k to the codec (x0.68: the ADC's range used up to ~-4 dBFS).
constexpr float kInGain  = -1.f / (2.0f * 0.1883f);
// pedal units -> DAC, for unity through pg-hp (x1) and the TPA (x2); x1.046 = +0.39 dB for the analog leveller's
// 4.7k into the 100k across each LDR (simulated on the routed board)
constexpr float kOutGain = 2.0f / (2.0f * 1.41f) * 1.046f;

DaisySeed hw;
Encoder   enc[pg::kKnobs];
Switch    fs[3];
Ili9341   lcd;
pg::Core  core;
I2CHandle    i2c;
pg::IsoCodec codec;
pg::AnalogFx afx; // the board's analog level + filter (unity / open unless the DSP asks)

// The page selector: an 8-way rotary switch on a PCF8574 I/O board on the expansion header (Seed3 I2C, pins 12/13).
// Switch position k (1..8) -> PCF8574 pin P(k-1), the switch's common -> GND; the PCF8574's own pull-ups hold the
// others high, so the selected position reads low. Any of its 16 addresses is found by itself.
struct PageSelectorIn
{
    uint8_t addr = 0, last = 0xFF;
    int     pos  = 0;
    void    Find(I2CHandle& bus)
    {
        for(uint8_t a : {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F})
        {
            uint8_t all_in = 0xFF; // writing 1s makes every pin an input (with its weak pull-up)
            if(bus.TransmitBlocking(a, &all_in, 1, 2) == I2CHandle::Result::OK)
            {
                addr = a;
                return;
            }
        }
    }
    // the switch position 1..8, 0 while between positions / nothing fitted
    int Read(I2CHandle& bus)
    {
        if(!addr)
            return 0;
        uint8_t v = 0xFF;
        if(bus.ReceiveBlocking(addr, &v, 1, 2) != I2CHandle::Result::OK)
            return pos;
        const uint8_t low = uint8_t(~v);
        if(v == last && low && !(low & (low - 1))) // exactly one pin low, the same twice in a row: settled
        {
            int k = 0;
            while(!(low & (1u << k)))
                k++;
            pos = k + 1;
        }
        last = v;
        return pos;
    }
};
PageSelectorIn page_sel;
pg::AnalogLeveller leveller;          // the analog leveller on the carrier board (U15 DAC -> LEDs -> LDRs)
static volatile float out_ms = 0.f;   // the output's mean square (DAC units), for the leveller
// set by the main loop, used by the audio callback
static volatile float g_in = kInGain, g_out = 0.f, in_peak = 0.f; // g_out stays 0 (silent) until the codec is up
// the screen picture the core draws into: 150 KB, in the Seed3's 64 MB SDRAM (plain array, no
// constructor, so it is safe to place there; SDRAM is ready after hw.Init())
static uint16_t DSY_SDRAM_BSS framebuffer[pg::Canvas::kW * pg::Canvas::kH];

// Everything the pedal remembers over power-off (settings, the 8 configs, the learned hum / hiss), as one
// block in the QSPI flash at 7 MB - well clear of the program, which the bootloader keeps near the start.
struct StateBlob
{
    uint8_t data[pg::Core::kStateBytes];
    bool    operator!=(const StateBlob& o) const { return memcmp(data, o.data, sizeof(data)) != 0; }
    bool    operator==(const StateBlob& o) const { return !(*this != o); }
};
constexpr uint32_t                 kStateOffset = 0x700000;
static PersistentStorage<StateBlob> storage(hw.qspi);
static StateBlob                    blob;

// Runs at 1 kHz (48 samples per block at 48 kHz): controls + audio.
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    const uint32_t now = System::GetNow(), t0 = System::GetUs();
    for(int i = 0; i < pg::kKnobs; i++)
    {
        enc[i].Debounce();
        if(enc[i].RisingEdge())
            core.KnobPress(i, true, now);
        const int inc = enc[i].Increment();
        core.KnobTurn(i, kReverseEncoder ? -inc : inc, now);
        if(enc[i].FallingEdge())
            core.KnobPress(i, false, now);
    }
    for(int i = 0; i < 3; i++)
    {
        fs[i].Debounce();
        if(fs[i].RisingEdge())
            core.Footswitch(i, true, now);
        if(fs[i].FallingEdge())
            core.Footswitch(i, false, now);
    }
    // in: the codec's raw samples -> pedal units (the range and the input-gain knob are undone / applied here)
    static float  ibuf[2][48];
    const float   gi = g_in;
    float         pk = 0.f;
    const size_t  n  = size > 48 ? 48 : size;
    for(size_t c = 0; c < 2; c++)
        for(size_t i = 0; i < n; i++)
        {
            const float x = in[c][i];
            pk            = fabsf(x) > pk ? fabsf(x) : pk;
            ibuf[c][i]    = x * gi;
        }
    if(pk > in_peak)
        in_peak = pk;
    const float* ip[2] = {ibuf[0], ibuf[1]};
    core.Process(ip, out, n, now);
    // out: pedal units -> DAC, with the output-level knob; glided per block (no zipper), never past full scale
    static float go = 0.f;
    const float  gt = g_out, g0 = go;
    go += (gt - go) * 0.2f;
    for(size_t c = 0; c < 2; c++)
        for(size_t i = 0; i < n; i++)
        {
            const float y = out[c][i] * (g0 + (go - g0) * float(i) / float(n));
            out[c][i]     = y > 1.f ? 1.f : (y < -1.f ? -1.f : y);
        }
    float ms = 0.f; // what's leaving, for the analog leveller (one cheap pass)
    for(size_t i = 0; i < n; i++)
        ms += 0.5f * (out[0][i] * out[0][i] + out[1][i] * out[1][i]);
    out_ms = out_ms + (ms / float(n) - out_ms) * 0.05f;
    // health: how much of the block's time the work took (1.0 = none left)
    core.ReportLoad(float(System::GetUs() - t0) * hw.AudioSampleRate() / (1e6f * float(size)));
}

// ------------------------------------------------------------------ watchdog
// The STM32's independent watchdog runs on its own clock: if the firmware ever hangs, it resets the Seed3 in ~2 s.
// (The output is safe meanwhile: the codec mutes itself when the digital audio stops, and pg-hp is analog.)
static void WatchdogStart()
{
    IWDG1->KR  = 0xCCCC; // start
    IWDG1->KR  = 0x5555; // unlock the settings
    IWDG1->PR  = 4;      // 32 kHz / 64 = 2 ms per count
    IWDG1->RLR = 1000;   // 2 s
    while(IWDG1->SR) {}
    IWDG1->KR = 0xAAAA;
}
static inline void WatchdogKick() { IWDG1->KR = 0xAAAA; }

// ------------------------------------------------------------------ touch (XPT2046)
GPIO t_clk, t_cs, t_din, t_dout;

void TouchInit()
{
    t_clk.Init(pins::kTClk, GPIO::Mode::OUTPUT);
    t_cs.Init(pins::kTCs, GPIO::Mode::OUTPUT);
    t_din.Init(pins::kTDin, GPIO::Mode::OUTPUT);
    t_dout.Init(pins::kTDout, GPIO::Mode::INPUT);
    t_cs.Write(true);
    t_clk.Write(false);
}

uint8_t TouchXfer(uint8_t out)
{
    uint8_t in = 0;
    for(int b = 7; b >= 0; b--)
    {
        t_din.Write((out >> b) & 1);
        System::DelayUs(1);
        t_clk.Write(true); // controller max ~2.5 MHz; ~500 kHz here
        System::DelayUs(1);
        in = uint8_t((in << 1) | (t_dout.Read() ? 1 : 0));
        t_clk.Write(false);
    }
    return in;
}

int TouchRaw(uint8_t cmd)
{
    int sum = 0;
    for(int i = 0; i < 4; i++)
    {
        t_cs.Write(false);
        TouchXfer(cmd);
        const int hi = TouchXfer(0), lo = TouchXfer(0);
        t_cs.Write(true);
        sum += ((hi << 8) | lo) >> 3;
    }
    return sum / 4;
}

// Returns true while touched, with screen pixel coordinates.
bool TouchRead(int& x, int& y)
{
    if(TouchRaw(0xB0) < kTouchZMin) // pressure (Z1): no finger
        return false;
    int a = TouchRaw(0xD0), b = TouchRaw(0x90); // 12-bit X and Y channels
    if(TouchRaw(0xB0) < kTouchZMin)
        return false; // released while reading
    if(kTouchSwapXY)
    {
        const int t = a;
        a           = b;
        b           = t;
    }
    auto map = [](int raw, int size, bool flip) {
        int v = (raw - kTouchRawMin) * size / (kTouchRawMax - kTouchRawMin);
        v     = v < 0 ? 0 : (v >= size ? size - 1 : v);
        return flip ? size - 1 - v : v;
    };
    x = map(a, pg::Canvas::kW, kTouchFlipX);
    y = map(b, pg::Canvas::kH, kTouchFlipY);
    return true;
}

int main(void)
{
    hw.Init(true); // 480 MHz: more room for the DSP, and a 192 MHz PLL1Q for the screen link
    {   // the screen's SPI from PLL1Q (192 MHz) instead of libDaisy's 25 MHz PLL2P, so it can run at 48 MHz
        RCC_PeriphCLKInitTypeDef pc = {};
        pc.PeriphClockSelection     = RCC_PERIPHCLK_SPI123;
        pc.Spi123ClockSelection     = RCC_SPI123CLKSOURCE_PLL;
        HAL_RCCEx_PeriphCLKConfig(&pc);
    }
    // audio: SAI2 (I2S to the isolated codec), 48 kHz, 48-sample blocks. The Seed3's own codec (SAI1) isn't used.
    {
        SaiHandle::Config sc;
        sc.periph          = SaiHandle::Config::Peripheral::SAI_2;
        sc.sr              = SaiHandle::Config::SampleRate::SAI_48KHZ;
        sc.bit_depth       = SaiHandle::Config::BitDepth::SAI_24BIT;
        sc.a_sync          = SaiHandle::Config::Sync::MASTER;
        sc.b_sync          = SaiHandle::Config::Sync::SLAVE;
        sc.a_dir           = SaiHandle::Config::Direction::TRANSMIT;
        sc.b_dir           = SaiHandle::Config::Direction::RECEIVE;
        sc.pin_config.fs   = pins::kSaiFs;
        sc.pin_config.sck  = pins::kSaiSck;
        sc.pin_config.sa   = pins::kSaiTx;
        sc.pin_config.sb   = pins::kSaiRx;
        sc.pin_config.mclk = pins::kSaiMclk; // not wired (the codec makes its clock from BCLK); TouchInit takes the pin back
        SaiHandle sai2;
        sai2.Init(sc);
        AudioHandle::Config ac;
        ac.blocksize  = 48;
        ac.samplerate = SaiHandle::Config::SampleRate::SAI_48KHZ;
        ac.postgain   = 1.f;
        hw.audio_handle.Init(ac, sai2);
        I2CHandle::Config ic;
        ic.periph         = I2CHandle::Config::Peripheral::I2C_1;
        ic.speed          = I2CHandle::Config::Speed::I2C_400KHZ;
        ic.mode           = I2CHandle::Config::Mode::I2C_MASTER;
        ic.pin_config.scl = pins::kScl;
        ic.pin_config.sda = pins::kSda;
        i2c.Init(ic);
    }

    enc[0].Init(pins::kPg1A, pins::kPg1B, pins::kPg1Sw);
    enc[1].Init(pins::kPg2A, pins::kPg2B, pins::kPg2Sw);
    enc[2].Init(pins::kPg3A, pins::kPg3B, pins::kPg3Sw);
    enc[3].Init(pins::kPg4A, pins::kPg4B, pins::kPg4Sw);
    fs[0].Init(pins::kFs1);
    fs[1].Init(pins::kFs2);
    fs[2].Init(pins::kFs3);
    core.Init(hw.AudioSampleRate(), framebuffer);
    core.SetDeviceId(HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2()); // this pedal's id (config: about)
    memset(&blob, 0, sizeof(blob));
    storage.Init(blob, kStateOffset);
    if(storage.GetState() == PersistentStorage<StateBlob>::State::USER) // something was saved: bring it back
        core.LoadState(storage.GetSettings().data, pg::Core::kStateBytes);

    TouchInit();
    lcd.Init({pins::kLcdCs, pins::kLcdDc, pins::kLcdRst, pins::kLcdLed, pins::kLcdSck, pins::kLcdMosi}, kFlipScreen, core.ScreenFast());
    lcd.Backlight(false); // dark until the first real frame is on the glass (no flash of garbage)

    hw.StartAudio(AudioCallback); // the bit clock is running now: the codec's PLL can lock on it
    bool codec_ok = codec.Init(&i2c);
    if(!codec_ok)
        core.ReportCodecFault();
    afx.Init(&i2c); // only answers if an add-on board with an MCP4461 sits in the fx loop (J22); harmless if not
    page_sel.Find(i2c);   // the page selector's PCF8574 (if it's plugged in)
    leveller.Init(&i2c);  // the analog leveller (its DAC powers up at 0 V: LEDs dark, sound untouched)
    uint32_t last_sel = 0;
    const uint32_t audio_t0 = System::GetNow();
    WatchdogStart();

    const Ili9341::Pins lcd_pins = {pins::kLcdCs, pins::kLcdDc, pins::kLcdRst, pins::kLcdLed, pins::kLcdSck, pins::kLcdMosi};
    uint32_t            last_touch = 0, last_refresh = 0, last_repush = 0;
    bool                backlight = true, screen_fast = core.ScreenFast();
    while(1)
    {
        if(core.WantsFlashMode())
        {
            hw.StopAudio();
            lcd.Fill(0, 0, pg::Canvas::kW, pg::Canvas::kH, pg::Canvas::Rgb(0, 0, 170));
            lcd.Text(12, 100, "usb flash mode", Font_11x18, pg::Canvas::Rgb(255, 255, 85), pg::Canvas::Rgb(0, 0, 170));
            System::Delay(800);
            lcd.Sleep(); // panel off cleanly before the reset
#ifdef BOOT_APP
            System::ResetToBootloader(System::DAISY_INFINITE_TIMEOUT); // Daisy bootloader: `make program-dfu`
#else
            System::ResetToBootloader(); // STM32 DFU
#endif
        }
        WatchdogKick();
        if(System::GetNow() - last_sel >= 30) // the page selector, ~33 times a second
        {
            last_sel = System::GetNow();
            core.PageSelector(page_sel.Read(i2c), last_sel);
            // gentle 2:1 above -14 dBFS rms: the LDRs ride the level in the analog path. Dark for the first 10 s (its
            // 10 uF capacitors finish charging through the LDRs' 100k: a squeeze before that would thump)
            if(leveller.Present() && last_sel > 10000)
            {
                const float db = 10.f * log10f(out_ms + 1e-12f);
                leveller.SetCut(db > -14.f ? (db + 14.f) * 0.5f : 0.f);
            }
        }
        const uint32_t now = System::GetNow();
        // ---- display safety
        if(lcd.Faulted() || core.ScreenFast() != screen_fast) // SPI failing, or fast / safe changed: start over
        {
            const bool fault = lcd.Faulted();
            screen_fast      = core.ScreenFast();
            lcd.Init(lcd_pins, kFlipScreen, screen_fast);
            if(!fault)
            {
                core.RepushScreen();
                continue;
            }
            core.RepushScreen();
            core.ReportDisplayFault();
        }
        if(now - last_refresh >= 2000) // mode registers re-sent every 2 s
            last_refresh = now, lcd.Refresh();
        if(now - last_repush >= 60000) // every pixel re-sent once a minute
            last_repush = now, core.RepushScreen();
        if(core.BacklightOn() != backlight) // display rest: backlight off after 20 idle minutes
            backlight = core.BacklightOn(), lcd.Backlight(backlight);
        if(now - last_touch >= 20) // ~50 Hz touch scan
        {
            last_touch = now;
            int        x = 0, y = 0;
            const bool down = TouchRead(x, y);
            core.Touch(down, x, y, now);
        }
        // ---- isolated audio: nothing here sets a level. Both knobs are analog gain stages on the carrier board
        // (centre click = unity); the codec runs at one fixed gain. The firmware only unmutes once after power-up.
        static uint32_t last_alive = 0;
        static bool     unmuted    = false;
        if(!unmuted && codec_ok && now - audio_t0 > 600) // the headphone amp's own power-up mute is over
            unmuted = codec.SetMute(false), g_out = kOutGain;
        if(now - last_alive >= 1000) // the codec must keep answering; if it stops, set it up again
        {
            last_alive = now;
            if(!codec.Alive())
            {
                core.ReportCodecFault();
                codec_ok = codec.Init(&i2c), unmuted = false;
            }
            else if(!codec_ok)
                codec_ok = codec.Init(&i2c);
        }
        if(core.WantsSave(now)) // 4 s after the last change: write it to the flash (only if it differs)
        {
            memset(&blob, 0, sizeof(blob));
            if(core.SaveState(blob.data, pg::Core::kStateBytes) > 0)
            {
                storage.GetSettings() = blob;
                storage.Save();
            }
            core.SaveDone();
        }
        core.DrawUi(lcd, now);
        if(backlight && now > 300)
            lcd.Backlight(true);
    }
}
