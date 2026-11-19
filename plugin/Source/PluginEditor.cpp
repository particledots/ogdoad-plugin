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
    const float scopeX = 40.0f, scopeW = w - 40.0f - 120.0f;
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
        const float level = (float) std::fabs (v);
        g.setColour (knobFill);
        g.fillEllipse (20.0f, mid - 5.0f, 10.0f, 10.0f);
        g.setColour (ledColour.withAlpha (0.15f + 0.85f * level));
        g.fillEllipse (20.0f, mid - 5.0f, 10.0f, 10.0f);

        // The trace: the last two seconds, now at the right edge.
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

        g.setColour (text);
        g.drawText (OgdoadParams::speedText (hz), juce::Rectangle<float> (w - 112.0f, y, 66.0f, (float) kRowHeight), juce::Justification::centredLeft);
        g.setColour (dim);
        g.drawText ("CC " + juce::String (ogdoad::kLfoMsbCc + i), juce::Rectangle<float> (w - 44.0f, y, 44.0f, (float) kRowHeight), juce::Justification::centredLeft);
    }
}

// ================================================================================================
void ExSection::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (exPanel);
    g.fillRoundedRectangle (b, 8.0f);
    juce::Path border;
    border.addRoundedRectangle (b, 8.0f);
    juce::Path dashed;
    const float dashes[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (dim);
    g.fillPath (dashed);

    g.setColour (text);
    drawSpaced (g, "ogdoad ex", 14.0f, 30.0f, 15.0f, 2.5f);

    const juce::String badge = "dimmed until milestone 2";
    auto bf = font (12.0f);
    const float bw = juce::GlyphArrangement::getStringWidth (bf, badge) + 14.0f;
    auto badgeBox = juce::Rectangle<float> (b.getRight() - bw - 12.0f, 14.0f, bw, 20.0f);
    g.setColour (dim);
    g.drawRoundedRectangle (badgeBox, 4.0f, 0.6f);
    g.setColour (text);
    g.setFont (bf);
    g.drawText (badge, badgeBox, juce::Justification::centred);

    struct Group { const char* title; const char* items[4]; };
    const Group groups[4] = {
        { "fold \xc3\x97" "2",   { "1", "3", "5", "7" } },
        { "max / min",           { "2 \xe2\x88\xa8 3", "6 \xe2\x88\xa8 7", "2 \xe2\x88\xa7 3", "6 \xe2\x88\xa7 7" } },
        { "trig chain",          { "2 \xe2\x86\x92", "4 \xe2\x86\x93", "8", "\xe2\x86\x90 6" } },
        { "steps",               { "1\xe2\x80\x93" "4", "5\xe2\x80\x93" "8", "odd", "even" } },
    };
    const float colW = (b.getWidth() - 28.0f) * 0.5f;
    for (int gi = 0; gi < 4; ++gi)
    {
        const float gx = 14.0f + (float) (gi % 2) * colW;
        const float gy = 58.0f + (float) (gi / 2) * 108.0f;
        g.setColour (text);
        g.setFont (font (13.0f));
        g.drawText (juce::String::fromUTF8 (groups[gi].title), juce::Rectangle<float> (gx, gy, colW, 18.0f), juce::Justification::centredLeft);
        for (int k = 0; k < 4; ++k)
        {
            const float ix = gx + (float) (k % 2) * (colW * 0.5f);
            const float iy = gy + 26.0f + (float) (k / 2) * 30.0f;
            g.setColour (exText);
            g.drawEllipse (ix + 1.0f, iy + 3.0f, 10.0f, 10.0f, 1.0f);
            g.setFont (font (12.5f));
            g.drawText (juce::String::fromUTF8 (groups[gi].items[k]), juce::Rectangle<float> (ix + 17.0f, iy, colW * 0.5f - 18.0f, 16.0f), juce::Justification::centredLeft);
        }
    }
    g.setColour (dim);
    g.setFont (font (12.0f));
    g.drawFittedText (juce::String::fromUTF8 ("each output: light + CC number\neach trig: \"in use\" switch"),
                      juce::Rectangle<float> (14.0f, b.getBottom() - 44.0f, b.getWidth() - 28.0f, 34.0f).toNearestInt(),
                      juce::Justification::centredLeft, 2);
}

// ================================================================================================
OgdoadEditor::OgdoadEditor (OgdoadProcessor& p)
    : AudioProcessorEditor (&p), proc (p), lanesView (p)
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
    setSize (kWidth, kHeight);
}

OgdoadEditor::~OgdoadEditor()
{
    setLookAndFeel (nullptr);
}

std::vector<OgdoadEditor::ControlInfo> OgdoadEditor::controls()
{
    return { { &rateKnob, OgdoadParams::rate }, { &modKnob, OgdoadParams::modAmount }, { &modSourceBox, OgdoadParams::modSource },
             { &holdButton, OgdoadParams::hold }, { &pedalButton, OgdoadParams::holdPedal }, { &driftBox, OgdoadParams::drift },
             { &channelBox, OgdoadParams::outChannel } };
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
    g.setFont (font (12.0f));
    g.drawText (juce::String::fromUTF8 ("fastest at top, slowest at bottom \xc2\xb7 light: amber = up, blue = down"),
                juce::Rectangle<int> (20, lanesView.getBottom() + 4, 440, 18), juce::Justification::centredLeft);

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
    exSection.setBounds (490, top, getWidth() - 490 - 20, 316);

    const int fy = getHeight() - 38;
    channelBox.setBounds (130, fy + 7, 56, 24);
}
