#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PgCore.h"

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
    static juce::uint32 nowMs() noexcept { return juce::Time::getMillisecondCounter(); }

private:
    std::unique_ptr<pg::Core> pedalPtr { new pg::Core() };
    pg::Core& pedal = *pedalPtr;
    std::vector<uint16_t> framebuffer = std::vector<uint16_t> ((size_t) (pg::Canvas::kW * pg::Canvas::kH), 0); // the screen picture
    juce::AudioBuffer<float> scratch;

    void timerCallback() override;      // the pedal's autosave: 4 s after the last change
    void saveToDisk();
    static juce::File stateFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PG1Processor)
};
