#include "PluginEditor.h"

MultiFxEditor::MultiFxEditor (MultiFxAudioProcessor& p)
    : AudioProcessorEditor (&p),
      wah    (p.apvts, "WAH",           "wah_on",  juce::Colour (0xffffb347)),
      harm   (p.apvts, "HARMONIZER",    "harm_on", juce::Colour (0xff6ec6ff)),
      delay  (p.apvts, "DELAY",         "dly_on",  juce::Colour (0xff9be15d)),
      spring (p.apvts, "SPRING REVERB", "spr_on",  juce::Colour (0xffd38cff)),
      outKnob (p.apvts, "out_gain", "OUTPUT", " dB", juce::Colour (0xffffffff))
{
    wah.addKnob ("wah_pedal", "Pedal");
    wah.addKnob ("wah_auto",  "Auto");
    wah.addKnob ("wah_rate",  "Rate", " Hz");
    wah.addKnob ("wah_q",     "Resonance");
    wah.addKnob ("wah_mix",   "Mix", " %");

    harm.addKnob ("h1_semi", "Voice 1", " st");
    harm.addKnob ("h1_lvl",  "Level 1", " %");
    harm.addKnob ("h2_semi", "Voice 2", " st");
    harm.addKnob ("h2_lvl",  "Level 2", " %");
    harm.addKnob ("h_dry",   "Dry", " %");

    delay.addToggle ("dly_sync", "Tempo Sync");
    delay.addToggle ("dly_ping", "Ping-Pong");
    delay.addCombo  ("dly_div", MultiFxAudioProcessor::delayDivisionNames());
    delay.addKnob ("dly_time", "Time", " ms");
    delay.addKnob ("dly_fb",   "Feedback", " %");
    delay.addKnob ("dly_tone", "Damp", " Hz");
    delay.addKnob ("dly_mix",  "Mix", " %");

    spring.addKnob ("spr_decay", "Decay", " %");
    spring.addKnob ("spr_tone",  "Damp", " Hz");
    spring.addKnob ("spr_boing", "Boing", " %");
    spring.addKnob ("spr_mix",   "Mix", " %");

    addAndMakeVisible (wah);
    addAndMakeVisible (harm);
    addAndMakeVisible (delay);
    addAndMakeVisible (spring);
    addAndMakeVisible (outKnob);

    setSize (860, 540);
}

void MultiFxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1c21));
    g.setColour (juce::Colours::grey);
    g.setFont (13.0f);
    g.drawText ("Studio Multi FX   -   Wah > Harmonizer > Delay > Spring Reverb",
                10, getHeight() - 24, getWidth() - 20, 18, juce::Justification::centredLeft);
}

void MultiFxEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    r.removeFromBottom (22);

    const int rowH = (r.getHeight() - 10) / 2;
    auto top = r.removeFromTop (rowH);
    r.removeFromTop (10);
    auto bottom = r;

    const int halfW = (top.getWidth() - 10) / 2;
    wah.setBounds  (top.removeFromLeft (halfW));
    top.removeFromLeft (10);
    harm.setBounds (top);

    auto outArea = bottom.removeFromRight (110);
    bottom.removeFromRight (10);
    const int bw = (bottom.getWidth() - 10) / 2;
    delay.setBounds  (bottom.removeFromLeft (bw));
    bottom.removeFromLeft (10);
    spring.setBounds (bottom);

    outKnob.setBounds (outArea.reduced (0, 20));
}
