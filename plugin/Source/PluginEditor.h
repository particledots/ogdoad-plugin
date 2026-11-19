// The Ogdoad window, as approved in the draft (2026-11-18): Rate, Mod, Mod source, Hold, Pedal, Drift and the
// eight LFO lanes on the left; the Ogdoad Ex section on the right, dimmed until milestone 2; the MIDI out
// channel and the version at the bottom. Milestone 2 brings the Ex section to life in this same window.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace OgdoadStyle
{
    inline const juce::Colour panel     { 0xff2c2c2a };
    inline const juce::Colour exPanel   { 0xff363634 };
    inline const juce::Colour knobFill  { 0xff444441 };
    inline const juce::Colour line      { 0xff5f5e5a };
    inline const juce::Colour dim       { 0xff888780 };
    inline const juce::Colour exText    { 0xffb4b2a9 };
    inline const juce::Colour text      { 0xffd3d1c7 };
    inline const juce::Colour bright    { 0xfff1efe8 };
    inline const juce::Colour amber     { 0xfffac775 };
    inline const juce::Colour amberDeep { 0xffef9f27 };
    inline const juce::Colour blue      { 0xff378add };
}

class OgdoadLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OgdoadLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.0f)); }
};

// The eight lanes: number, light, moving trace, speed, CC number.
class LanesView : public juce::Component, private juce::Timer
{
public:
    explicit LanesView (OgdoadProcessor& p);
    ~LanesView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    static constexpr int kRowHeight = 23;

private:
    void timerCallback() override { repaint(); }
    OgdoadProcessor& proc;
};

// Ogdoad Ex: shown, dimmed, until milestone 2.
class ExSection : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
};

class OgdoadEditor : public juce::AudioProcessorEditor
{
public:
    explicit OgdoadEditor (OgdoadProcessor&);
    ~OgdoadEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the offline checks: every control and the parameter it moves.
    struct ControlInfo { juce::Component* control; juce::String paramId; };
    std::vector<ControlInfo> controls();

    static constexpr int kWidth = 780, kHeight = 470;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    OgdoadProcessor& proc;
    OgdoadLookAndFeel lnf;

    juce::Slider rateKnob, modKnob;
    juce::Label rateLabel, modLabel;
    juce::ComboBox modSourceBox, driftBox, channelBox;
    juce::TextButton holdButton { "Hold" }, pedalButton { "Pedal" };
    LanesView lanesView;
    ExSection exSection;

    std::unique_ptr<SliderAttachment> rateAtt, modAtt;
    std::unique_ptr<ComboAttachment> modSourceAtt, driftAtt, channelAtt;
    std::unique_ptr<ButtonAttachment> holdAtt, pedalAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OgdoadEditor)
};
