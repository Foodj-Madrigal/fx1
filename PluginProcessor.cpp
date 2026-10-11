#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // durée en temps (noires) de chaque division, dans l'ordre de delayDivisionNames()
    constexpr float kDivBeats[] = { 2.0f, 1.0f, 1.5f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f, 1.0f / 6.0f };
}

juce::StringArray MultiFxAudioProcessor::delayDivisionNames()
{
    return { "1/2", "1/4", "1/4 dotted", "1/8", "1/8 dotted", "1/8 triplet",
             "1/16", "1/16 dotted", "1/16 triplet" };
}

MultiFxAudioProcessor::MultiFxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MultiFxAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto F = [&] (const char* id, const char* name, float lo, float hi, float step, float def, float skew = 1.0f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                        NormalisableRange<float> (lo, hi, step, skew), def));
    };
    auto B = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto I = [&] (const char* id, const char* name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
    };

    // Wah
    B ("wah_on",    "Wah On",         false);
    F ("wah_pedal", "Wah Pedal",      0.0f, 1.0f, 0.001f, 0.5f);
    F ("wah_auto",  "Wah Auto",       0.0f, 1.0f, 0.001f, 0.0f);
    F ("wah_rate",  "Wah Rate",       0.1f, 8.0f, 0.01f, 1.5f, 0.5f);
    F ("wah_q",     "Wah Resonance",  1.0f, 10.0f, 0.01f, 4.0f);
    F ("wah_mix",   "Wah Mix",        0.0f, 100.0f, 0.1f, 100.0f);

    // Harmonizer
    B ("harm_on",   "Harmonizer On",  false);
    I ("h1_semi",   "Voice 1 Interval", -12, 12, 4);
    F ("h1_lvl",    "Voice 1 Level",  0.0f, 100.0f, 0.1f, 70.0f);
    I ("h2_semi",   "Voice 2 Interval", -12, 12, 7);
    F ("h2_lvl",    "Voice 2 Level",  0.0f, 100.0f, 0.1f, 70.0f);
    F ("h_dry",     "Harmonizer Dry", 0.0f, 100.0f, 0.1f, 100.0f);

    // Delay
    B ("dly_on",    "Delay On",       true);
    F ("dly_time",  "Delay Time",     20.0f, 1500.0f, 1.0f, 380.0f, 0.5f);
    B ("dly_sync",  "Delay Sync",     false);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "dly_div", 1 }, "Delay Division",
                                                         delayDivisionNames(), 3));
    F ("dly_fb",    "Delay Feedback", 0.0f, 95.0f, 0.1f, 35.0f);
    F ("dly_tone",  "Delay Damp",     500.0f, 16000.0f, 1.0f, 6000.0f, 0.4f);
    B ("dly_ping",  "Delay Ping-Pong", false);
    F ("dly_mix",   "Delay Mix",      0.0f, 100.0f, 0.1f, 25.0f);

    // Spring reverb
    B ("spr_on",    "Spring On",      true);
    F ("spr_decay", "Spring Decay",   0.0f, 100.0f, 0.1f, 50.0f);
    F ("spr_tone",  "Spring Damp",    1500.0f, 10000.0f, 1.0f, 5000.0f, 0.5f);
    F ("spr_boing", "Spring Boing",   0.0f, 100.0f, 0.1f, 60.0f);
    F ("spr_mix",   "Spring Mix",     0.0f, 100.0f, 0.1f, 25.0f);

    // Sortie
    F ("out_gain",  "Output",         -24.0f, 12.0f, 0.01f, 0.0f);

    return { p.begin(), p.end() };
}

bool MultiFxAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in  = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in == juce::AudioChannelSet::mono())   return true;               // mono -> mono ou mono -> stéréo
    if (in == juce::AudioChannelSet::stereo()) return out == juce::AudioChannelSet::stereo();
    return false;
}

void MultiFxAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    wah.prepare (sampleRate);
    harmonizer.prepare (sampleRate);
    delay.prepare (sampleRate);
    spring.prepare (sampleRate);
    prevOutGain = juce::Decibels::decibelsToGain (param ("out_gain"));
}

void MultiFxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n      = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);
    if (numIn == 1 && numOut >= 2)                       // piste mono -> les deux côtés
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // Ordre de la chaîne : Wah -> Harmonizer -> Delay -> Spring Reverb
    if (param ("wah_on") > 0.5f)
        wah.process (buffer, param ("wah_pedal"), param ("wah_auto"), param ("wah_rate"),
                     param ("wah_q"), param ("wah_mix") * 0.01f);

    if (param ("harm_on") > 0.5f)
        harmonizer.process (buffer, param ("h1_semi"), param ("h1_lvl") * 0.01f,
                            param ("h2_semi"), param ("h2_lvl") * 0.01f, param ("h_dry") * 0.01f);

    if (param ("dly_on") > 0.5f)
    {
        float ms = param ("dly_time");
        if (param ("dly_sync") > 0.5f)
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm())
                        bpm = *b;
            const int idx = juce::jlimit (0, (int) std::size (kDivBeats) - 1, (int) param ("dly_div"));
            ms = (float) (60000.0 / juce::jmax (20.0, bpm)) * kDivBeats[idx];
        }
        delay.process (buffer, ms * 0.001f * (float) currentSampleRate, param ("dly_fb") * 0.01f,
                       param ("dly_tone"), param ("dly_ping") > 0.5f, param ("dly_mix") * 0.01f);
    }

    if (param ("spr_on") > 0.5f)
        spring.process (buffer, param ("spr_decay") * 0.01f, param ("spr_tone"),
                        param ("spr_boing") * 0.01f, param ("spr_mix") * 0.01f);

    const float g = juce::Decibels::decibelsToGain (param ("out_gain"));
    buffer.applyGainRamp (0, n, prevOutGain, g);
    prevOutGain = g;
}

juce::AudioProcessorEditor* MultiFxAudioProcessor::createEditor()
{
    return new MultiFxEditor (*this);
}

void MultiFxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MultiFxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiFxAudioProcessor();
}
