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
#include "../core/PgCore.h"
#include "util/PersistentStorage.h"
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
constexpr Pin kLcdDc = seed::D11, kLcdRst = seed::D12, kLcdLed = seed::D16;
// XPT2046 touch (screen pins 10-14), bit-banged so it never fights the screen's SPI
constexpr Pin kTClk = seed::D22, kTCs = seed::D23, kTDin = seed::D24, kTDout = seed::D25, kTIrq = seed::D26;
} // namespace pins

constexpr bool kFlipScreen     = false; // set true if the picture is upside down
constexpr bool kReverseEncoder = false; // set true if turning right makes values go down
// Touch calibration: raw 0-4095 readings at the screen edges. If taps land in the wrong place,
// flip these (swap first, then the flips) until the bar follows your finger.
constexpr bool kTouchSwapXY = true, kTouchFlipX = false, kTouchFlipY = false;
constexpr int  kTouchRawMin = 300, kTouchRawMax = 3800;

DaisySeed hw;
Encoder   enc[pg::kKnobs];
Switch    fs[3];
Ili9341   lcd;
pg::Core  core;
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
    core.Process(in, out, size, now);
    // health: how much of the block's time the work took (1.0 = none left)
    core.ReportLoad(float(System::GetUs() - t0) * hw.AudioSampleRate() / (1e6f * float(size)));
}

// ------------------------------------------------------------------ touch (XPT2046)
GPIO t_clk, t_cs, t_din, t_dout, t_irq;

void TouchInit()
{
    t_clk.Init(pins::kTClk, GPIO::Mode::OUTPUT);
    t_cs.Init(pins::kTCs, GPIO::Mode::OUTPUT);
    t_din.Init(pins::kTDin, GPIO::Mode::OUTPUT);
    t_dout.Init(pins::kTDout, GPIO::Mode::INPUT);
    t_irq.Init(pins::kTIrq, GPIO::Mode::INPUT, GPIO::Pull::PULLUP);
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
    if(t_irq.Read()) // high = not touched
        return false;
    int a = TouchRaw(0xD0), b = TouchRaw(0x90); // 12-bit X and Y channels
    if(t_irq.Read())
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
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

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
    lcd.Init({pins::kLcdCs, pins::kLcdDc, pins::kLcdRst, pins::kLcdLed, pins::kLcdSck, pins::kLcdMosi}, kFlipScreen);
    lcd.Backlight(false); // dark until the first real frame is on the glass (no flash of garbage)

    hw.StartAudio(AudioCallback);

    const Ili9341::Pins lcd_pins = {pins::kLcdCs, pins::kLcdDc, pins::kLcdRst, pins::kLcdLed, pins::kLcdSck, pins::kLcdMosi};
    uint32_t            last_touch = 0, last_refresh = 0, last_repush = 0;
    bool                backlight = true;
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
        const uint32_t now = System::GetNow();
        // ---- display safety
        if(lcd.Faulted()) // SPI keeps failing: start the panel over and send the whole picture
        {
            lcd.Init(lcd_pins, kFlipScreen);
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
