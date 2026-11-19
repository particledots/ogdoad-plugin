#include "PluginEditor.h"
#include "Parameters.h"

#include <cmath>

using namespace OgdoadStyle;

namespace
{
    juce::Font font (float size) { return juce::Font (juce::FontOptions (size)); }

    // Draws text with extra space between the letters (for the title).
    void drawSpaced (juce::Graphics& g, const juce::String& s, float x, float baseline, float size, float spacing)
    {
        auto f = font (size);
        g.setFont (f);
        for (auto ch : s)
        {
            const juce::String one = juce::String::charToString (ch);
            g.drawSingleLineText (one, juce::roundToInt (x), juce::roundToInt (baseline));
            x += juce::GlyphArrangement::getStringWidth (f, one) + spacing;
        }
    }

    // A round light with a soft halo; level 0 to 1 decides how brightly it shines.
    void drawLight (juce::Graphics& g, float cx, float cy, float radius, juce::Colour colour, double level)
    {
        const float lv = (float) juce::jlimit (0.0, 1.0, level);
        if (lv > 0.15f)
        {
            g.setColour (colour.withAlpha (0.10f * lv));
            g.fillEllipse (cx - radius - 5.0f, cy - radius - 5.0f, 2.0f * (radius + 5.0f), 2.0f * (radius + 5.0f));
            g.setColour (colour.withAlpha (0.18f * lv));
            g.fillEllipse (cx - radius - 3.0f, cy - radius - 3.0f, 2.0f * (radius + 3.0f), 2.0f * (radius + 3.0f));
        }
        g.setColour (knobFill);
        g.fillEllipse (cx - radius, cy - radius, 2.0f * radius, 2.0f * radius);
        g.setColour (colour.withAlpha (lightAlpha (lv)));
        g.fillEllipse (cx - radius, cy - radius, 2.0f * radius, 2.0f * radius);
    }

    double tri (double ph)
    {
        ph -= std::floor (ph);
        return ph < 0.5 ? -1.0 + 4.0 * ph : 3.0 - 4.0 * ph;
    }
}

// ================================================================================================
OgdoadLookAndFeel::OgdoadLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff383836));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, knobFill.brighter (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, bright);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, panel);
    setColour (juce::Label::textColourId, text);
}

void OgdoadLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const float size = (float) juce::jmin (w, h) - 4.0f;
    const auto centre = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    g.setColour (knobFill);
    g.fillEllipse (centre.x - r, centre.y - r, size, size);
    g.setColour (dim);
    g.drawEllipse (centre.x - r, centre.y - r, size, size, 1.5f);

    const float angle = start + pos * (end - start);
    const bool isRate = s.getName() == "rate";
    const float len = r * 0.85f;
    const juce::Point<float> tip (centre.x + len * std::sin (angle), centre.y - len * std::cos (angle));
    g.setColour (isRate ? amber : bright);
    g.drawLine ({ centre, tip }, isRate ? 3.0f : 2.5f);

    if (! isRate)                                  // a small mark at the top: the Mod knob's zero
    {
        g.setColour (line);
        g.fillEllipse (centre.x - 1.5f, centre.y - r - 5.0f, 3.0f, 3.0f);
    }
}

void OgdoadLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
    g.setColour (box.isMouseOver() ? text.withAlpha (0.8f) : dim);
    g.drawRoundedRectangle (b, 4.0f, 0.8f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (dim);
    g.fillPath (arrow);
}

void OgdoadLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void OgdoadLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (b.getToggleState())
    {
        g.setColour (amber);
        g.fillRoundedRectangle (r, 4.0f);
    }
    else
    {
        g.setColour (over ? text.withAlpha (0.8f) : dim);
        g.drawRoundedRectangle (r, 4.0f, 0.8f);
    }
}

// ================================================================================================
LanesView::LanesView (OgdoadProcessor& p) : proc (p)
{
    setOpaque (false);
    startTimerHz (30);
}

void LanesView::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const float scopeX = 52.0f, scopeW = w - 52.0f - 120.0f;
    const double window = 2.0;                     // seconds shown in each trace
    g.setFont (font (12.5f));

    for (int i = 0; i < ogdoad::kNumLanes; ++i)
    {
        const float y = (float) (i * kRowHeight);
        const float mid = y + kRowHeight * 0.5f;
        const auto& lane = proc.lanes[(size_t) i];
        const double v = lane.value.load(), hz = lane.hz.load(), ph = lane.phase.load();

        g.setColour (text);
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (0.0f, y, 16.0f, (float) kRowHeight), juce::Justification::centredLeft);

        // The light: amber above the centre, blue below, brighter further out (like a bipolar LED).
        const auto ledColour = v >= 0.0 ? amber : blue;
        const double level = std::fabs (v);
        drawLight (g, 31.0f, mid, 8.5f, ledColour, level);

        if (! waves)
        {
            // Lights view: a light strip that shines with the same colour and brightness.
            const juce::Rectangle<float> strip (scopeX, mid - 6.0f, scopeW, 12.0f);
            g.setColour (knobFill);
            g.fillRoundedRectangle (strip, 6.0f);
            g.setColour (ledColour.withAlpha (0.08f + 0.55f * (float) std::pow (level, 0.6)));
            g.fillRoundedRectangle (strip, 6.0f);
        }
        else
        {
            // Waves view: the last two seconds, now at the right edge.
            const float top = mid - 7.0f, amp = 6.0f;
            g.setColour (amber);
            if (hz * window > 60.0)
            {
                g.setColour (amber.withAlpha (0.45f));
                g.fillRect (scopeX, top, scopeW, 14.0f);
            }
            else
            {
                juce::Path p;
                const int points = juce::jmax (2, (int) scopeW);
                for (int k = 0; k <= points; ++k)
                {
                    const double secondsAgo = window * (1.0 - (double) k / points);
                    const double val = hz > 0.0 ? tri (ph - hz * secondsAgo) : v;
                    const float px = scopeX + scopeW * (float) k / (float) points;
                    const float py = mid - amp * (float) val;
                    if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
                }
                g.strokePath (p, juce::PathStrokeType (1.0f));
            }
        }

        g.setColour (text);
        g.drawText (OgdoadParams::speedText (hz), juce::Rectangle<float> (w - 112.0f, y, 66.0f, (float) kRowHeight), juce::Justification::centredLeft);
        g.setColour (dim);
        g.drawText ("CC " + juce::String (ogdoad::kLfoMsbCc + i), juce::Rectangle<float> (w - 44.0f, y, 44.0f, (float) kRowHeight), juce::Justification::centredLeft);
    }
}

// ================================================================================================
ExSection::ExSection (OgdoadProcessor& p) : proc (p)
{
    auto& vts = proc.getValueTreeState();
    onButton.setClickingTogglesState (true);
    onButton.setTooltip ("Send the sixteen Ogdoad Ex outputs (off: only the eight LFOs are sent)");
    addAndMakeVisible (onButton);
    onAtt = std::make_unique<ButtonAttachment> (vts, OgdoadParams::exOn, onButton);

    for (int k = 0; k < ogdoad::kNumTrigs; ++k)
    {
        auto& b = useButtons[(size_t) k];
        b.setButtonText ("in use");
        b.setClickingTogglesState (true);
        b.setTooltip ("Off: this output is silent and its triggers pass on to the next one (2, 4, 6, 8)");
        addAndMakeVisible (b);
        useAtt[(size_t) k] = std::make_unique<ButtonAttachment> (vts, OgdoadParams::trigUse[k], b);
    }

    for (int k = 0; k < OgdoadParams::trigModeNames.size(); ++k) modeBox.addItem ("Trig out: " + OgdoadParams::trigModeNames[k], k + 1);
    modeBox.setTooltip ("How each trigger is sent: a CC pulse (127 for 20 ms), a short note (C1 to D#1), or both");
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, OgdoadParams::trigMode, modeBox);

    startTimerHz (30);
}

void ExSection::resized()
{
    onButton.setBounds (212, 14, 44, 22);
    const juce::Point<int> trigPos[ogdoad::kNumTrigs] = { { 14, 194 }, { 76, 194 }, { 76, 246 }, { 14, 246 } };
    for (int k = 0; k < ogdoad::kNumTrigs; ++k)
        useButtons[(size_t) k].setBounds (trigPos[k].x + 16, trigPos[k].y + 26, 44, 16);
    modeBox.setBounds (14, 300, 242, 24);
}

void ExSection::timerCallback()
{
    for (int k = 0; k < ogdoad::kNumTrigs; ++k)
    {
        const unsigned n = proc.trigPulses[(size_t) k].load();
        if (n != lastPulses[(size_t) k]) { lastPulses[(size_t) k] = n; flashFrames[(size_t) k] = kFlashFrames; }
        else if (flashFrames[(size_t) k] > 0) --flashFrames[(size_t) k];
    }
    repaint();
}

void ExSection::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (exPanel);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (line);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    g.setColour (text);
    drawSpaced (g, "ogdoad ex", 14.0f, 30.0f, 15.0f, 2.5f);

    const bool on = onButton.getToggleState();
    auto& vts = proc.getValueTreeState();

    // One output: a light, its name, and below it the CC number (or note).
    // active: sending (label bright); lit: the light shows the level.
    auto cell = [&] (float x, float cy, const juce::String& name, const juce::String& sub, double level, bool bipolar, bool active, bool lit)
    {
        if (lit)
            drawLight (g, x + 7.0f, cy, 7.0f, bipolar && level < 0.0 ? blue : amber, std::fabs (level));
        else
        {
            g.setColour (knobFill);
            g.fillEllipse (x, cy - 7.0f, 14.0f, 14.0f);
        }
        g.setColour (active ? text : dim);
        g.setFont (font (12.5f));
        g.drawText (name, juce::Rectangle<float> (x + 19.0f, cy - 8.5f, 46.0f, 17.0f), juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (11.0f));
        g.drawText (sub, juce::Rectangle<float> (x + 19.0f, cy + 8.0f, 52.0f, 14.0f), juce::Justification::centredLeft);
    };

    auto title = [&] (float x, float y, const char* t)
    {
        g.setColour (text);
        g.setFont (font (13.0f));
        g.drawText (juce::String::fromUTF8 (t), juce::Rectangle<float> (x, y, 124.0f, 18.0f), juce::Justification::centredLeft);
    };

    auto value = [&] (int i) { return (double) proc.exValues[(size_t) i].load(); };
    auto ccText = [] (int i) { return "CC " + juce::String (ogdoad::kExMsbCc[i]); };

    title (14.0f, 52.0f, "fold \xc3\x97" "2");
    title (142.0f, 52.0f, "max \xe2\x88\xa8  min \xe2\x88\xa7");
    title (14.0f, 164.0f, "trig chain");
    title (142.0f, 164.0f, "steps");

    // fold x2: 1, 3 / 5, 7
    const char* foldNames[4] = { "1", "3", "5", "7" };
    const float foldX[4] = { 14, 76, 14, 76 }, foldY[4] = { 82, 82, 118, 118 };
    for (int k = 0; k < 4; ++k) cell (foldX[k], foldY[k], foldNames[k], ccText (k), value (k), false, on, on);

    // max / min: top row max, bottom row min; left 2 and 3, right 6 and 7
    const char* mmNames[4] = { "2 \xe2\x88\xa8 3", "2 \xe2\x88\xa7 3", "6 \xe2\x88\xa8 7", "6 \xe2\x88\xa7 7" };
    const float mmX[4] = { 142, 142, 204, 204 }, mmY[4] = { 82, 118, 82, 118 };
    for (int k = 0; k < 4; ++k)
        cell (mmX[k], mmY[k], juce::String::fromUTF8 (mmNames[k]), ccText (4 + k), value (4 + k), true, on, on);

    // steps
    const char* stNames[4] = { "1\xe2\x80\x93" "4", "5\xe2\x80\x93" "8", "odd", "even" };
    const float stX[4] = { 142, 204, 142, 204 }, stY[4] = { 194, 194, 230, 230 };
    for (int k = 0; k < 4; ++k)
        cell (stX[k], stY[k], juce::String::fromUTF8 (stNames[k]), ccText (8 + k), value (8 + k), false, on, on);

    // trig chain, clockwise from top left: 2 -> 4 -> 6 -> 8
    const int mode = juce::roundToInt (vts.getRawParameterValue (OgdoadParams::trigMode)->load());
    const char* trNames[4] = { "2 \xe2\x86\x92", "4 \xe2\x86\x93", "\xe2\x86\x90 6", "8" };
    const char* noteNames[4] = { "C1", "C#1", "D1", "D#1" };
    const float trX[4] = { 14, 76, 76, 14 }, trY[4] = { 194, 194, 246, 246 };
    for (int k = 0; k < 4; ++k)
    {
        const bool inUse = useButtons[(size_t) k].getToggleState();
        juce::String sub = mode == 0 ? "CC " + juce::String (ogdoad::kTrigCc[k])
                         : mode == 1 ? "note " + juce::String (noteNames[k])
                         : juce::String (ogdoad::kTrigCc[k]) + juce::String::fromUTF8 (" \xc2\xb7 ") + noteNames[k];
        const double flashLevel = proc.trigOn[(size_t) k].load() ? 1.0 : (double) flashFrames[(size_t) k] / (double) kFlashFrames;
        cell (trX[k], trY[k], juce::String::fromUTF8 (trNames[k]), sub, flashLevel, false, on && inUse, on && inUse);
    }

    g.setColour (dim);
    g.setFont (font (11.5f));
    g.drawText ("switched off: silent, its triggers pass on to the next",
                juce::Rectangle<float> (14.0f, 330.0f, 250.0f, 16.0f), juce::Justification::centredLeft);
}

// ================================================================================================
OgdoadEditor::OgdoadEditor (OgdoadProcessor& p)
    : AudioProcessorEditor (&p), proc (p), lanesView (p), exSection (p)
{
    setLookAndFeel (&lnf);
    auto& vts = proc.getValueTreeState();

    for (auto* k : { &rateKnob, &modKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (juce::MathConstants<float>::pi * -0.75f, juce::MathConstants<float>::pi * 0.75f, true);
        k->setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (*k);
    }
    rateKnob.setName ("rate");
    modKnob.setName ("mod");
    rateKnob.setDoubleClickReturnValue (true, 0.5);
    modKnob.setDoubleClickReturnValue (true, 0.0);
    rateAtt = std::make_unique<SliderAttachment> (vts, OgdoadParams::rate, rateKnob);
    modAtt  = std::make_unique<SliderAttachment> (vts, OgdoadParams::modAmount, modKnob);

    rateLabel.setText ("Rate", juce::dontSendNotification);
    modLabel.setText (juce::String::fromUTF8 ("Mod \xe2\x88\x92 +"), juce::dontSendNotification);
    for (auto* l : { &rateLabel, &modLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (font (12.5f));
        addAndMakeVisible (*l);
    }

    for (int k = 0; k < OgdoadParams::modSourceNames.size(); ++k) modSourceBox.addItem ("Mod source: " + OgdoadParams::modSourceNames[k], k + 1);
    for (int k = 0; k < OgdoadParams::driftNames.size(); ++k) driftBox.addItem ("Drift: " + OgdoadParams::driftNames[k].toLowerCase(), k + 1);
    for (int ch = 1; ch <= 16; ++ch) channelBox.addItem (juce::String (ch), ch);
    modSourceBox.setTooltip ("What moves the speed through the Mod knob: one of the LFOs (self-patch) or the mod wheel");
    for (auto* b : { &modSourceBox, &driftBox, &channelBox }) addAndMakeVisible (*b);
    modSourceAtt = std::make_unique<ComboAttachment> (vts, OgdoadParams::modSource, modSourceBox);
    driftAtt     = std::make_unique<ComboAttachment> (vts, OgdoadParams::drift, driftBox);
    channelAtt   = std::make_unique<ComboAttachment> (vts, OgdoadParams::outChannel, channelBox);

    holdButton.setClickingTogglesState (true);
    pedalButton.setClickingTogglesState (true);
    pedalButton.setTooltip ("Also hold while the sustain pedal is down");
    addAndMakeVisible (holdButton);
    addAndMakeVisible (pedalButton);
    holdAtt  = std::make_unique<ButtonAttachment> (vts, OgdoadParams::hold, holdButton);
    pedalAtt = std::make_unique<ButtonAttachment> (vts, OgdoadParams::holdPedal, pedalButton);

    addAndMakeVisible (lanesView);
    addAndMakeVisible (exSection);

    // Lights | Waves: one or the other; the choice is kept in the saved state.
    lightsButton.setRadioGroupId (4711);
    wavesButton.setRadioGroupId (4711);
    lightsButton.setClickingTogglesState (true);
    wavesButton.setClickingTogglesState (true);
    lightsButton.setTooltip ("Show the eight LFOs as lights");
    wavesButton.setTooltip ("Show the eight LFOs as moving waves");
    lightsButton.onClick = [this] { if (lightsButton.getToggleState()) proc.getValueTreeState().state.setProperty (wavesViewProperty, false, nullptr); };
    wavesButton.onClick  = [this] { if (wavesButton.getToggleState())  proc.getValueTreeState().state.setProperty (wavesViewProperty, true, nullptr); };
    addAndMakeVisible (lightsButton);
    addAndMakeVisible (wavesButton);
    showView ((bool) vts.state.getProperty (wavesViewProperty, false));
    vts.state.addListener (this);

    setSize (kWidth, kHeight);
}

OgdoadEditor::~OgdoadEditor()
{
    proc.getValueTreeState().state.removeListener (this);
    setLookAndFeel (nullptr);
}

void OgdoadEditor::showView (bool waves)
{
    lanesView.setWaves (waves);
    lightsButton.setToggleState (! waves, juce::dontSendNotification);
    wavesButton.setToggleState (waves, juce::dontSendNotification);
}

void OgdoadEditor::valueTreeRedirected (juce::ValueTree& tree)
{
    if (tree == proc.getValueTreeState().state)
        showView ((bool) tree.getProperty (wavesViewProperty, false));
}

void OgdoadEditor::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == wavesViewProperty && tree == proc.getValueTreeState().state)
        showView ((bool) tree.getProperty (wavesViewProperty, false));
}

std::vector<OgdoadEditor::ControlInfo> OgdoadEditor::controls()
{
    return { { &rateKnob, OgdoadParams::rate }, { &modKnob, OgdoadParams::modAmount }, { &modSourceBox, OgdoadParams::modSource },
             { &holdButton, OgdoadParams::hold }, { &pedalButton, OgdoadParams::holdPedal }, { &driftBox, OgdoadParams::drift },
             { &channelBox, OgdoadParams::outChannel },
             { &exSection.onButton, OgdoadParams::exOn },
             { &exSection.useButtons[0], OgdoadParams::trigUse[0] }, { &exSection.useButtons[1], OgdoadParams::trigUse[1] },
             { &exSection.useButtons[2], OgdoadParams::trigUse[2] }, { &exSection.useButtons[3], OgdoadParams::trigUse[3] },
             { &exSection.modeBox, OgdoadParams::trigMode } };
}

void OgdoadEditor::paint (juce::Graphics& g)
{
    g.fillAll (panel);

    // Header
    g.setColour (bright);
    drawSpaced (g, "ogdoad", 20.0f, 40.0f, 22.0f, 9.0f);
    g.setColour (dim);
    g.setFont (font (12.5f));
    g.drawText (juce::String::fromUTF8 ("eight drifting LFOs \xe2\x86\x92 MIDI CC"), juce::Rectangle<int> (getWidth() - 320, 22, 300, 22), juce::Justification::centredRight);
    g.setColour (line);
    g.fillRect (20.0f, 54.0f, (float) getWidth() - 40.0f, 0.6f);

    // Lane legend
    g.setColour (dim);
    g.setFont (font (11.5f));
    g.drawText (juce::String::fromUTF8 ("fastest at top, slowest at bottom \xc2\xb7 amber = up, blue = down"),
                juce::Rectangle<int> (20, lanesView.getBottom() + 4, 340, 18), juce::Justification::centredLeft);

    // Footer
    const float fy = (float) getHeight() - 38.0f;
    g.setColour (line);
    g.fillRect (20.0f, fy, (float) getWidth() - 40.0f, 0.6f);
    g.setColour (dim);
    g.setFont (font (12.5f));
    g.drawText ("MIDI out: channel", juce::Rectangle<int> (20, (int) fy + 8, 110, 22), juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("\xc2\xb7 14-bit CC \xc2\xb7 your notes pass through"),
                juce::Rectangle<int> (channelBox.getRight() + 8, (int) fy + 8, 300, 22), juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (JucePlugin_VersionString), juce::Rectangle<int> (getWidth() - 120, (int) fy + 8, 100, 22), juce::Justification::centredRight);
}

void OgdoadEditor::resized()
{
    const int left = 20, top = 70;
    rateKnob.setBounds (left, top, 64, 64);
    rateLabel.setBounds (left - 8, top + 64, 80, 18);
    modKnob.setBounds (left + 86, top + 8, 46, 46);
    modLabel.setBounds (left + 74, top + 64, 70, 18);

    const int cx = left + 160, cw = 290;
    modSourceBox.setBounds (cx, top + 6, cw, 26);
    holdButton.setBounds (cx, top + 42, 64, 26);
    pedalButton.setBounds (cx + 72, top + 42, 64, 26);
    driftBox.setBounds (cx + 144, top + 42, cw - 144, 26);

    lanesView.setBounds (left, top + 98, 450, LanesView::kRowHeight * ogdoad::kNumLanes);
    exSection.setBounds (490, top, ExSection::kWidth, ExSection::kHeight);
    lightsButton.setBounds (left + 450 - 108, lanesView.getBottom() + 3, 52, 20);
    wavesButton.setBounds (left + 450 - 52, lanesView.getBottom() + 3, 52, 20);

    const int fy = getHeight() - 38;
    channelBox.setBounds (130, fy + 7, 56, 24);
}
