#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PgCore.h"
#include "Seed3Runner.h"
#include "Seed3Costs.h"

/** Runs the pedal's own core (the same code as the Seed3 firmware) on the host's audio.
    Like the pedal's flash, everything (settings, the 8 configs, learned hum) is also kept in a file
    (~/.config/PG-1 Pedal/pedal-state.bin), so a new instance starts where the last one left off.
    A host session that has its own saved state still wins. */
class PG1Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    PG1Processor();
    ~PG1Processor() override;

    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PG-1 Pedal"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    pg::Core& core() noexcept { return pedal; }
    /** the Seed3's audio load with the stages that are on now (1.0 = no time left), from Seed3Costs.h */
    float seed3Load() const noexcept
    {
        int instr = seed3::kBase;
        for (int t = 0; t < pg::kTabs && t < 16; ++t)
            if (pedal.StageOn (t))
                instr += seed3::kStage[t];
        return (float) instr * seed3::kClocksPerInstr / seed3::kClocksPerSample;
    }
    static juce::uint32 nowMs() noexcept { return juce::Time::getMillisecondCounter(); }

private:
    std::unique_ptr<pg::Core> pedalPtr { new pg::Core() };
    pg::Core& pedal = *pedalPtr;
    std::vector<uint16_t> framebuffer = std::vector<uint16_t> ((size_t) (pg::Canvas::kW * pg::Canvas::kH), 0); // the screen picture
    juce::AudioBuffer<float> scratch, outScratch;
    Seed3Runner runner;   // the core always runs at the Seed3's 48 kHz, in its 48-sample blocks

    void timerCallback() override;      // the pedal's autosave: 4 s after the last change
    void saveToDisk();
    static juce::File stateFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PG1Processor)
};
