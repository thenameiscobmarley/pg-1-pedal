#include "PluginProcessor.h"
#include "PluginEditor.h"

PG1Processor::PG1Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    pedal.Init (48000.0f, framebuffer.data());
    setLatencySamples (pedal.LatencySamples());
    juce::MemoryBlock mb;   // where the last instance left off (a host session loaded later overrides it)
    if (stateFile().existsAsFile() && stateFile().loadFileAsData (mb))
        pedal.LoadState ((const uint8_t*) mb.getData(), (int) mb.getSize());
    startTimer (1000);
}

PG1Processor::~PG1Processor()
{
    stopTimer();
    saveToDisk();
}

juce::File PG1Processor::stateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PG-1 Pedal").getChildFile ("pedal-state.bin");
}

void PG1Processor::timerCallback()
{
    if (pedal.WantsSave (nowMs()))
        saveToDisk();
}

void PG1Processor::saveToDisk()
{
    std::vector<uint8_t> st ((size_t) pg::Core::kStateBytes);
    const int n = pedal.SaveState (st.data(), (int) st.size());
    if (n <= 0)
        return;
    const auto f = stateFile();
    f.getParentDirectory().createDirectory();
    juce::TemporaryFile tmp (f);   // write next to it, then swap in: a crash mid-write can't leave half a file
    if (tmp.getFile().replaceWithData (st.data(), (size_t) n) && tmp.overwriteTargetFileWithTemporary())
        pedal.SaveDone();
}

bool PG1Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    const auto in  = l.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void PG1Processor::prepareToPlay (double sampleRate, int blockSize)
{
    // Init resets everything to defaults: keep the user's values across a host restart of the audio
    std::array<int, pg::P_COUNT> keep {};
    std::array<bool, pg::kTabs> stages {};
    for (int p = 0; p < pg::P_COUNT; ++p) keep[(size_t) p] = pedal.GetParam (p).value;
    for (int t = 0; t < pg::kTabs; ++t) stages[(size_t) t] = pedal.StageOn (t);
    const auto prof = pedal.Profile();
    pedal.Init ((float) sampleRate, framebuffer.data());
    if (prof.valid)
        pedal.SetProfile (prof);
    for (int p = 0; p < pg::P_COUNT; ++p) pedal.SetParam (p, keep[(size_t) p]);
    for (int t = 0; t < pg::kTabs; ++t) pedal.SetStageOn (t, stages[(size_t) t]);
    setLatencySamples (pedal.LatencySamples());   // the safety limiter's 1 ms look-ahead
    scratch.setSize (2, juce::jmax (1, blockSize));
}

void PG1Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    struct Load   // health: report how much of the block's time the work took, however we leave
    {
        pg::Core& c; juce::int64 t0; int n; double sr;
        ~Load() { c.ReportLoad ((float) (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * sr / juce::jmax (1, n))); }
    } load { pedal, t0, buffer.getNumSamples(), getSampleRate() };
    const int n = buffer.getNumSamples(), inCh = getTotalNumInputChannels(), outCh = buffer.getNumChannels();
    if (scratch.getNumSamples() < n)
        scratch.setSize (2, n, false, false, true);

    // the core is stereo in / stereo out, like the pedal's jacks: a mono input feeds both sides
    for (int ch = 0; ch < 2; ++ch)
        scratch.copyFrom (ch, 0, buffer, juce::jmin (ch, juce::jmax (0, inCh - 1)), 0, n);

    const float* in[2] = { scratch.getReadPointer (0), scratch.getReadPointer (1) };
    float* out[2]      = { buffer.getWritePointer (0), buffer.getWritePointer (juce::jmin (1, outCh - 1)) };
    if (outCh == 1)
    {
        float* tmp[2] = { buffer.getWritePointer (0), scratch.getWritePointer (1) };   // in[1] is read before written
        pedal.Process (in, tmp, (size_t) n, nowMs());
        return;
    }
    pedal.Process (in, out, (size_t) n, nowMs());
}

juce::AudioProcessorEditor* PG1Processor::createEditor() { return new PG1Editor (*this); }

void PG1Processor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement x ("PG1");
    {   // the whole pedal state (settings, 8 configs, learned hum / hiss) in the pedal's own format
        std::vector<uint8_t> st ((size_t) pg::Core::kStateBytes);
        const int n = pedal.SaveState (st.data(), (int) st.size());
        if (n > 0)
            x.setAttribute ("state", juce::Base64::toBase64 (st.data(), (size_t) n));
    }
    for (int p = 0; p < pg::P_COUNT; ++p)
        x.setAttribute (juce::String ("p") + juce::String (p), pedal.GetParam (p).value);
    x.setAttribute ("on", pedal.EffectOn() ? 1 : 0);
    for (int t = 0; t < pg::kTabs; ++t)
        x.setAttribute ("stage" + juce::String (t), pedal.StageOn (t) ? 1 : 0);
    const auto& prof = pedal.Profile();   // the hum tab's learned noise
    if (prof.valid)
    {
        x.setAttribute ("lt_n", prof.n_tones);
        for (int k = 0; k < pg::NoiseProfile::kTones; ++k)
            x.setAttribute ("lt_f" + juce::String (k), prof.tone_hz[k]), x.setAttribute ("lt_d" + juce::String (k), prof.tone_db[k]);
        for (int k = 0; k < pg::NoiseProfile::kBands; ++k)
            x.setAttribute ("lf" + juce::String (k), prof.floor_db[k]);
    }
    copyXmlToBinary (x, dest);
}

void PG1Processor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size); x != nullptr && x->hasTagName ("PG1"))
    {
        if (x->hasAttribute ("state"))
        {
            juce::MemoryOutputStream mo;
            if (juce::Base64::convertFromBase64 (mo, x->getStringAttribute ("state"))
                && pedal.LoadState ((const uint8_t*) mo.getData(), (int) mo.getDataSize()))
            {
                pedal.ForceRedraw();
                return;
            }
        }
        for (int p = 0; p < pg::P_COUNT; ++p)
            pedal.SetParam (p, x->getIntAttribute (juce::String ("p") + juce::String (p), pedal.GetParam (p).value));
        pedal.SetEffectOn (x->getIntAttribute ("on", 1) != 0);
        if (x->hasAttribute ("lt_n"))
        {
            pg::NoiseProfile prof;
            prof.n_tones = juce::jlimit (0, pg::NoiseProfile::kTones, x->getIntAttribute ("lt_n"));
            for (int k = 0; k < pg::NoiseProfile::kTones; ++k)
                prof.tone_hz[k] = (float) x->getDoubleAttribute ("lt_f" + juce::String (k), 1000.0),
                prof.tone_db[k] = (float) x->getDoubleAttribute ("lt_d" + juce::String (k), 0.0);
            for (int k = 0; k < pg::NoiseProfile::kBands; ++k)
                prof.floor_db[k] = (float) x->getDoubleAttribute ("lf" + juce::String (k), -120.0);
            prof.valid = true;
            pedal.SetProfile (prof);
        }
        for (int t = 0; t < pg::kTabs; ++t)
            pedal.SetStageOn (t, x->getIntAttribute ("stage" + juce::String (t), pedal.StageOn (t) ? 1 : 0) != 0);
        pedal.ForceRedraw();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PG1Processor(); }
