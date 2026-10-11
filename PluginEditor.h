#pragma once
#include "PluginProcessor.h"

class MultiFxEditor : public juce::AudioProcessorEditor
{
public:
    explicit MultiFxEditor (MultiFxAudioProcessor&);
    ~MultiFxEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Apvts = juce::AudioProcessorValueTreeState;

    struct Knob : public juce::Component
    {
        Knob (Apvts& s, const juce::String& id, const juce::String& text,
              const juce::String& suffix, juce::Colour accent)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
            slider.setTextValueSuffix (suffix);
            slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            slider.setColour (juce::Slider::thumbColourId, accent);
            label.setText (text, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (slider);
            addAndMakeVisible (label);
            attachment = std::make_unique<Apvts::SliderAttachment> (s, id, slider);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (18));
            slider.setBounds (r);
        }
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<Apvts::SliderAttachment> attachment;
    };

    struct Panel : public juce::Component
    {
        Panel (Apvts& s, const juce::String& t, const juce::String& onId, juce::Colour c)
            : state (s), title (t), accent (c)
        {
            power.setButtonText ("ON");
            power.setColour (juce::ToggleButton::tickColourId, accent);
            addAndMakeVisible (power);
            powerAtt = std::make_unique<Apvts::ButtonAttachment> (s, onId, power);
        }

        void addKnob (const juce::String& id, const juce::String& text, const juce::String& suffix = {})
        {
            knobs.push_back (std::make_unique<Knob> (state, id, text, suffix, accent));
            addAndMakeVisible (*knobs.back());
        }
        void addToggle (const juce::String& id, const juce::String& text)
        {
            auto b = std::make_unique<juce::ToggleButton> (text);
            b->setColour (juce::ToggleButton::tickColourId, accent);
            addAndMakeVisible (*b);
            toggleAtts.push_back (std::make_unique<Apvts::ButtonAttachment> (state, id, *b));
            toggles.push_back (std::move (b));
        }
        void addCombo (const juce::String& id, const juce::StringArray& items)
        {
            combo = std::make_unique<juce::ComboBox>();
            combo->addItemList (items, 1);
            addAndMakeVisible (*combo);
            comboAtt = std::make_unique<Apvts::ComboBoxAttachment> (state, id, *combo);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff2a2c34));
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.75f), 10.0f, 1.5f);
            g.setColour (juce::Colours::white);
            g.setFont (18.0f);
            g.drawText (title, 66, 8, getWidth() - 76, 24, juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto header = r.removeFromTop (28);
            power.setBounds (header.removeFromLeft (56));

            if (! toggles.empty() || combo != nullptr)
            {
                auto row = r.removeFromTop (26);
                for (auto& t : toggles) t->setBounds (row.removeFromLeft (100));
                if (combo != nullptr) combo->setBounds (row.removeFromLeft (110).reduced (0, 2));
            }
            if (! knobs.empty())
            {
                const int w = r.getWidth() / (int) knobs.size();
                for (auto& k : knobs) k->setBounds (r.removeFromLeft (w));
            }
        }

        Apvts& state;
        juce::String title;
        juce::Colour accent;
        juce::ToggleButton power;
        std::vector<std::unique_ptr<Knob>> knobs;
        std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<Apvts::ButtonAttachment> powerAtt;
        std::vector<std::unique_ptr<Apvts::ButtonAttachment>> toggleAtts;
        std::unique_ptr<Apvts::ComboBoxAttachment> comboAtt;
    };

    Panel wah, harm, delay, spring;
    Knob outKnob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiFxEditor)
};
