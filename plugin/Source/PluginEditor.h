// The Ogdoad window, as approved in the drafts (2026-11-18, and 2026-11-19 for Ogdoad Ex): Rate, Mod, Mod source,
// Hold, Pedal, Drift and the eight LFO lanes on the left; the Ogdoad Ex section on the right (sixteen outputs with
// their lights and CC numbers, the On switch, the trig "in use" switches and Trig out); the MIDI out channel and the
// version at the bottom.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
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

    // How strongly a light shines for a level from 0 to 1: a faint glow at 0, clearly lit already at 30%, full at 1.
    inline float lightAlpha (double level)
    {
        return 0.10f + 0.90f * (float) std::pow (juce::jlimit (0.0, 1.0, level), 0.6);
    }
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
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (h < 20 ? 11.0f : 12.5f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (13.0f)); }
};

// The eight lanes: number, light, then either a light strip (Lights view, the default) or the moving trace (Waves
// view), speed, CC number.
class LanesView : public juce::Component, private juce::Timer
{
public:
    explicit LanesView (OgdoadProcessor& p);
    ~LanesView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void setWaves (bool shouldShowWaves) { if (waves != shouldShowWaves) { waves = shouldShowWaves; repaint(); } }
    bool isWaves() const { return waves; }
    static constexpr int kRowHeight = 23;

private:
    void timerCallback() override { repaint(); }
    OgdoadProcessor& proc;
    bool waves = false;
};

// Ogdoad Ex: the sixteen outputs (light, name, CC number), the On switch, the four "in use" switches and Trig out.
class ExSection : public juce::Component, private juce::Timer
{
public:
    explicit ExSection (OgdoadProcessor& p);
    ~ExSection() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::TextButton onButton { "On" };
    std::array<juce::TextButton, ogdoad::kNumTrigs> useButtons;
    juce::ComboBox modeBox;

    static constexpr int kWidth = 270, kHeight = 350;
    static constexpr int kFlashFrames = 8;                  // about 270 ms at 30 frames a second

private:
    void timerCallback() override;
    OgdoadProcessor& proc;
    std::array<unsigned, ogdoad::kNumTrigs> lastPulses {};
    std::array<int, ogdoad::kNumTrigs> flashFrames {};      // counts down from kFlashFrames: the trig light fades out

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> onAtt;
    std::array<std::unique_ptr<ButtonAttachment>, ogdoad::kNumTrigs> useAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
};

// The whole panel at its design size (780 x 470). The editor below scales it to the window.
class OgdoadPanel : public juce::Component, private juce::ValueTree::Listener
{
public:
    explicit OgdoadPanel (OgdoadProcessor&);
    ~OgdoadPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the offline checks: every control and the parameter it moves.
    struct ControlInfo { juce::Component* control; juce::String paramId; };
    std::vector<ControlInfo> controls();

    // Lights | Waves: which view the eight lanes show. Saved with the project (not an audio parameter).
    juce::TextButton lightsButton { "Lights" }, wavesButton { "Waves" };
    bool isWavesView() const { return lanesView.isWaves(); }
    static inline const juce::Identifier wavesViewProperty { "wavesView" };

    static constexpr int kWidth = 780, kHeight = 470;      // the design size; everything is laid out at this size

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void showView (bool waves);

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OgdoadPanel)
};

// The plugin window: resizable between 80% and 200% of the design size, proportions locked. The panel is drawn at its
// design size and scaled to fit, so every control, light and letter grows together. The width is saved with the project.
class OgdoadEditor : public juce::AudioProcessorEditor
{
public:
    explicit OgdoadEditor (OgdoadProcessor&);
    ~OgdoadEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the offline checks (they forward to the panel).
    std::vector<OgdoadPanel::ControlInfo> controls() { return panel.controls(); }
    bool isWavesView() const { return panel.isWavesView(); }

    static constexpr int kWidth = OgdoadPanel::kWidth, kHeight = OgdoadPanel::kHeight;
    static constexpr int kMinWidth = 624, kMaxWidth = 1560;     // 80% and 200%
    static inline const juce::Identifier widthProperty { "editorWidth" };
    static int heightForWidth (int width) { return juce::roundToInt ((double) width * kHeight / kWidth); }

    OgdoadPanel panel;

private:
    OgdoadProcessor& proc;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OgdoadEditor)
};
