#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PgCore.h"

/** Runs the pedal's own core (the same code as the Seed3 firmware) on the host's audio. */
class PG1Processor : public juce::AudioProcessor
{
public:
    PG1Processor();

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PG1Processor)
};
