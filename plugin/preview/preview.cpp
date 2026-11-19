// Offline window picture and plugin checks (not part of the plugin; only built with -DOGDOAD_BUILD_PREVIEW=ON).
//   1. draws the window to picture files (default, running, held);
//   2. checks that all 13 ids exist, each has exactly one control, and each control moves exactly its own parameter;
//   3. checks saving and reloading (and loading a 0.1.0 project and a 0.2.0 one, and the Lights | Waves choice);
//   4. checks the MIDI: notes and other messages pass through untouched, the LFOs come out as CC 20-27 / 52-59 on the
//      chosen channel, the mod wheel and sustain pedal work, bypass adds nothing, and Logic's block size makes no difference;
//   5. checks Ogdoad Ex in the real plugin: the sixteen outputs, Ex off, the trig chain, CC pulses, notes, both.
// Usage: OgdoadPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <functional>
#include <iostream>
#include <map>
#include <set>

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const juce::String& what)
    {
        ++checks;
        if (! ok) { ++failures; std::cout << "FAIL: " << what << std::endl; }
    }

    juce::RangedAudioParameter* param (juce::AudioProcessor& p, const juce::String& id)
    {
        for (auto* x : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x))
                if (r->paramID == id) return r;
        return nullptr;
    }

    void setParam (juce::AudioProcessor& p, const juce::String& id, float real)
    {
        if (auto* r = param (p, id)) r->setValueNotifyingHost (r->convertTo0to1 (real));
        else expect (false, "no parameter " + id);
    }

    std::map<juce::String, float> snapshot (juce::AudioProcessor& p)
    {
        std::map<juce::String, float> m;
        for (auto* x : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (x)) m[r->paramID] = r->getValue();
        return m;
    }

    void snapshotTo (juce::Component& c, const juce::File& file)
    {
        auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        if (out.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (image, out); }
    }

    struct Msg { long long sample; juce::MidiMessage m; };

    std::vector<Msg> runBlocks (OgdoadProcessor& p, double seconds, int block, const std::vector<Msg>& in = {}, bool bypassed = false)
    {
        std::vector<Msg> out;
        juce::AudioBuffer<float> audio (0, 8192);
        long long pos = 0, total = (long long) (seconds * 48000.0);
        juce::Random rng (5);
        while (pos < total)
        {
            int n = block > 0 ? block : 1 + rng.nextInt (1024);
            n = (int) juce::jmin<long long> (n, total - pos);
            audio.setSize (0, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : in)
                if (e.sample >= pos && e.sample < pos + n) midi.addEvent (e.m, (int) (e.sample - pos));
            if (bypassed) p.processBlockBypassed (audio, midi); else p.processBlock (audio, midi);
            for (const auto meta : midi) out.push_back ({ pos + meta.samplePosition, meta.getMessage() });
            pos += n;
        }
        return out;
    }

    bool isLfoCc (const juce::MidiMessage& m)
    {
        if (! m.isController()) return false;
        const int n = m.getControllerNumber();
        return (n >= 20 && n <= 27) || (n >= 52 && n <= 59);
    }

    bool isTrigOff (const juce::MidiMessage& m)
    {
        return (m.isController() && m.getControllerNumber() >= 102 && m.getControllerNumber() <= 105 && m.getControllerValue() == 0)
            || (m.isNoteOff() && m.getNoteNumber() >= 36 && m.getNoteNumber() <= 39);
    }

    const char* allIds[] = { "rate", "modAmount", "modSource", "hold", "holdPedal", "drift", "outChannel",
                             "exOn", "trigUse1", "trigUse2", "trigUse3", "trigUse4", "trigMode" };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    // ---- 1 and 2: the window and its controls ------------------------------------------------------------------
    {
        OgdoadProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        expect (proc.getParameters().size() == 13, "13 parameters");
        for (auto id : allIds)
            expect (param (proc, id) != nullptr, juce::String ("parameter id ") + id);
        expect (param (proc, "exOn")->getValue() == 1.0f, "Ex starts on");
        expect (param (proc, "trigUse1")->getValue() == 1.0f && param (proc, "trigUse2")->getValue() == 1.0f
                && param (proc, "trigUse3")->getValue() == 1.0f && param (proc, "trigUse4")->getValue() == 1.0f, "all four trigs start in use");
        expect (param (proc, "trigMode")->getCurrentValueAsText() == "CC pulse", "Trig out starts on CC pulse");
        expect (proc.isMidiEffect() && proc.acceptsMidi() && proc.producesMidi(), "a MIDI effect");
        expect (proc.getTotalNumInputChannels() == 0 && proc.getTotalNumOutputChannels() == 0, "no audio channels");
        expect (param (proc, "rate")->getCurrentValueAsText() == "LFO 1 4.9 Hz", "Rate shows LFO 1's speed (" + param (proc, "rate")->getCurrentValueAsText() + ")");
        expect (param (proc, "modSource")->getCurrentValueAsText() == "LFO 8", "Mod source starts on LFO 8 (as drawn)");
        using OgdoadParams::speedText;
        expect (speedText (1.0 / 18.9982) == "19 s" && speedText (160.0) == "160 Hz" && speedText (0.72) == "0.72 Hz"
                && speedText (1.0 / 2.6) == "2.6 s" && speedText (1.0 / 1500.0) == "25.0 min" && speedText (0.0) == "held",
                "speed read-outs are short (" + speedText (1.0 / 18.9982) + ", " + speedText (160.0) + ", " + speedText (1.0 / 1500.0) + ")");
        expect (param (proc, "drift")->getCurrentValueAsText() == "Low", "Drift starts on Low");
        expect (juce::roundToInt (param (proc, "outChannel")->convertFrom0to1 (param (proc, "outChannel")->getValue())) == 16, "MIDI out starts on channel 16");

        std::unique_ptr<OgdoadEditor> ed (dynamic_cast<OgdoadEditor*> (proc.createEditor()));
        expect (ed != nullptr, "editor opens");
        expect (ed->getWidth() == OgdoadEditor::kWidth && ed->getHeight() == OgdoadEditor::kHeight, "window size");

        snapshotTo (*ed, outDir.getChildFile ("ogdoad-window-default.png"));

        auto controls = ed->controls();
        std::set<juce::String> ids;
        for (auto& c : controls) ids.insert (c.paramId);
        expect (controls.size() == 13 && ids.size() == 13, "every parameter has exactly one control");
        for (auto& c : controls)
        {
            auto before = snapshot (proc);
            if (auto* s = dynamic_cast<juce::Slider*> (c.control)) s->setValue (s->getMinimum() + (s->getMaximum() - s->getMinimum()) * 0.83, juce::sendNotificationSync);
            else if (auto* b = dynamic_cast<juce::ComboBox*> (c.control)) b->setSelectedItemIndex (2, juce::sendNotificationSync);
            else if (auto* t = dynamic_cast<juce::Button*> (c.control)) t->setToggleState (! t->getToggleState(), juce::sendNotificationSync);
            auto after = snapshot (proc);
            int changed = 0; juce::String which;
            for (auto& kv : after) if (kv.second != before[kv.first]) { ++changed; which = kv.first; }
            expect (changed == 1 && which == c.paramId, "control for " + c.paramId + " moves only " + c.paramId + " (moved " + which + ")");
            const auto inWindow = ed->getLocalArea (c.control, c.control->getLocalBounds());
            bool visible = true;
            for (auto* x = c.control; x != nullptr && x != ed.get(); x = x->getParentComponent()) visible = visible && x->isVisible();
            expect (visible && ! inWindow.isEmpty() && ed->getLocalBounds().contains (inWindow),
                    "control for " + c.paramId + " is visible inside the window");
        }

        // Back to defaults, run for a while, picture of the moving window.
        for (auto* x : proc.getParameters()) x->setValueNotifyingHost (x->getDefaultValue());
        runBlocks (proc, 1.37, 512);
        snapshotTo (*ed, outDir.getChildFile ("ogdoad-window-running.png"));
        setParam (proc, "hold", 1.0f);
        runBlocks (proc, 0.1, 512);
        snapshotTo (*ed, outDir.getChildFile ("ogdoad-window-held.png"));
        expect (proc.held.load(), "panel sees Hold");
        setParam (proc, "hold", 0.0f);

        // Combo text as drawn.
        std::vector<juce::ComboBox*> boxes;
        for (auto& c : ed->controls()) if (auto* b = dynamic_cast<juce::ComboBox*> (c.control)) boxes.push_back (b);
        expect (boxes.size() == 4 && boxes[0]->getText() == "Mod source: LFO 8" && boxes[1]->getText() == "Drift: low" && boxes[2]->getText() == "16"
                && boxes[3]->getText() == "Trig out: CC pulse",
                "menus read 'Mod source: LFO 8', 'Drift: low', '16', 'Trig out: CC pulse'");

        // Two more pictures: trig 4 switched off with Trig out on Note; and Ex switched off.
        setParam (proc, "trigUse2", 0.0f); setParam (proc, "trigMode", 1.0f);
        runBlocks (proc, 2.0, 512);
        snapshotTo (*ed, outDir.getChildFile ("ogdoad-window-ex-cascade-note.png"));
        setParam (proc, "trigUse2", 1.0f); setParam (proc, "trigMode", 0.0f);
        setParam (proc, "exOn", 0.0f);
        runBlocks (proc, 0.3, 512);
        snapshotTo (*ed, outDir.getChildFile ("ogdoad-window-ex-off.png"));
        setParam (proc, "exOn", 1.0f);

        // Lights | Waves (0.2.1): starts on Lights, switches, saves, moves no parameter.
        expect (! ed->isWavesView() && ed->panel.lightsButton.getToggleState() && ! ed->panel.wavesButton.getToggleState(), "window starts in Lights view");
        const auto beforeView = snapshot (proc);
        ed->panel.wavesButton.setToggleState (true, juce::sendNotificationSync);   // the same path as a click
        expect (ed->isWavesView() && ed->panel.wavesButton.getToggleState() && ! ed->panel.lightsButton.getToggleState()
                && (bool) proc.getValueTreeState().state.getProperty (OgdoadPanel::wavesViewProperty, false),
                "Waves button shows the waves and keeps the choice in the saved state");
        runBlocks (proc, 0.5, 512);
        snapshotTo (*ed, outDir.getChildFile ("ogdoad-window-waves.png"));
        ed->panel.lightsButton.setToggleState (true, juce::sendNotificationSync);
        expect (! ed->isWavesView() && ! (bool) proc.getValueTreeState().state.getProperty (OgdoadPanel::wavesViewProperty, false),
                "Lights button shows the lights again");
        expect (snapshot (proc) == beforeView, "switching the view moves no parameter");
        expect (ed->controls().size() == 13, "the view buttons are not parameter controls");

        // The light curve: faint at 0, clearly lit at 30% (brighter than the 0.2.0 curve), full at 1, never falling.
        bool rising = true;
        for (int k = 1; k <= 100; ++k) rising = rising && OgdoadStyle::lightAlpha (k / 100.0) >= OgdoadStyle::lightAlpha ((k - 1) / 100.0);
        expect (std::fabs (OgdoadStyle::lightAlpha (0.0) - 0.10f) < 1.0e-6f && std::fabs (OgdoadStyle::lightAlpha (1.0) - 1.0f) < 1.0e-6f
                && OgdoadStyle::lightAlpha (0.3) > 0.15f + 0.85f * 0.3f && rising && OgdoadStyle::lightAlpha (7.0) <= 1.0f,
                "light brightness curve: 0.10 at 0, 1.0 at 1, stronger than 0.2.0 at 30%, never falling");
    }

    // Resizing (0.2.2): the window scales between 80% and 200% with its proportions locked; everything stays inside.
    {
        OgdoadProcessor proc; proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<OgdoadEditor> ed (dynamic_cast<OgdoadEditor*> (proc.createEditor()));
        expect (ed->isResizable() && ed->getConstrainer() != nullptr, "the window is resizable");
        auto* con = ed->getConstrainer();
        const double ratio = (double) OgdoadEditor::kWidth / OgdoadEditor::kHeight;
        auto clamp = [&] (int w, int h)
        {
            juce::Rectangle<int> b (0, 0, w, h);
            con->checkBounds (b, juce::Rectangle<int> (0, 0, 780, 470), juce::Rectangle<int> (0, 0, 4000, 3000), false, false, false, true);
            return b;
        };
        auto big = clamp (3000, 3000), small = clamp (100, 100), mid = clamp (1000, 1000);
        expect (big.getWidth() <= 1560 && big.getHeight() <= 940 && small.getWidth() >= 624 && small.getHeight() >= 376
                && std::fabs ((double) mid.getWidth() / mid.getHeight() - ratio) < 0.01 && std::fabs ((double) big.getWidth() / big.getHeight() - ratio) < 0.01,
                "size limits 80% to 200% and the proportions stay locked");

        struct Size { int w; const char* name; } sizes[] = { { OgdoadEditor::kMaxWidth, "large" }, { OgdoadEditor::kMinWidth, "small" }, { 1170, "medium" } };
        for (auto& sz : sizes)
        {
            ed->setSize (sz.w, OgdoadEditor::heightForWidth (sz.w));
            const double scale = (double) sz.w / OgdoadEditor::kWidth;
            expect (std::fabs (ed->panel.getTransform().getScaleFactor() - scale) < 1.0e-3,
                    juce::String ("panel scale at ") + sz.name + " is " + juce::String (scale, 2));
            bool inside = true;
            for (auto& c : ed->controls())
            {
                const auto r = ed->getLocalArea (c.control, c.control->getLocalBounds());
                inside = inside && ed->getLocalBounds().contains (r) && ! r.isEmpty();
            }
            expect (inside, juce::String ("every control inside the window at ") + sz.name + " size");
            expect ((int) proc.getValueTreeState().state.getProperty (OgdoadEditor::widthProperty, -1) == sz.w, juce::String ("the ") + sz.name + " width is kept in the saved state");
            runBlocks (proc, 0.4, 512);
            snapshotTo (*ed, outDir.getChildFile (juce::String ("ogdoad-window-") + sz.name + ".png"));
        }
    }

    // ---- 3: saving and reloading ----------------------------------------------------------------------------------
    {
        OgdoadProcessor a;
        setParam (a, "rate", 0.31f); setParam (a, "modAmount", -0.4f); setParam (a, "modSource", 9.0f);
        setParam (a, "hold", 1.0f); setParam (a, "holdPedal", 1.0f); setParam (a, "drift", 2.0f); setParam (a, "outChannel", 5.0f);
        setParam (a, "exOn", 0.0f); setParam (a, "trigUse2", 0.0f); setParam (a, "trigUse4", 0.0f); setParam (a, "trigMode", 2.0f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        OgdoadProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        expect (snapshot (a) == snapshot (b), "every setting comes back after save and reload");

        // A project saved with 0.1.0 (only the first 7 settings): they come back, and Ex starts as new.
        auto xml = a.getValueTreeState().copyState().createXml();
        for (auto* child = xml->getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();
            const auto id = child->getStringAttribute ("id");
            if (id == "exOn" || id.startsWith ("trig")) xml->removeChildElement (child, true);
            child = next;
        }
        juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary (*xml, old);
        OgdoadProcessor c; c.setStateInformation (old.getData(), (int) old.getSize());
        expect (param (c, "rate")->getValue() == param (a, "rate")->getValue() && param (c, "outChannel")->getValue() == param (a, "outChannel")->getValue(),
                "0.1.0 project: its settings come back");
        expect (param (c, "exOn")->getValue() == 1.0f && param (c, "trigUse2")->getValue() == 1.0f && param (c, "trigMode")->getValue() == 0.0f,
                "0.1.0 project: Ex on, all trigs in use, CC pulse");

        // The Lights | Waves choice is saved with the project; a 0.2.0 project (no choice saved) opens on Lights.
        a.getValueTreeState().state.setProperty (OgdoadPanel::wavesViewProperty, true, nullptr);
        juce::MemoryBlock withView; a.getStateInformation (withView);
        OgdoadProcessor d; d.setStateInformation (withView.getData(), (int) withView.getSize());
        std::unique_ptr<OgdoadEditor> edD (dynamic_cast<OgdoadEditor*> (d.createEditor()));
        expect (snapshot (a) == snapshot (d) && edD->isWavesView() && edD->panel.wavesButton.getToggleState(),
                "Waves view is saved and comes back when the project is reloaded");
        auto xml020 = a.getValueTreeState().copyState().createXml();
        xml020->removeAttribute (OgdoadPanel::wavesViewProperty.toString());
        juce::MemoryBlock v020; juce::AudioProcessor::copyXmlToBinary (*xml020, v020);
        d.setStateInformation (v020.getData(), (int) v020.getSize());           // loaded while the window is open
        expect (! edD->isWavesView() && edD->panel.lightsButton.getToggleState(), "0.2.0 project (no view saved): the open window goes to Lights");
        OgdoadProcessor e; e.setStateInformation (v020.getData(), (int) v020.getSize());
        std::unique_ptr<OgdoadEditor> edE (dynamic_cast<OgdoadEditor*> (e.createEditor()));
        expect (! edE->isWavesView() && snapshot (a) == snapshot (e), "0.2.0 project opens on Lights with all its settings");

        // The window size is saved with the project (clamped to 80% to 200%); a project with none opens at the design size.
        expect (edE->getWidth() == OgdoadEditor::kWidth && edE->getHeight() == OgdoadEditor::kHeight, "0.2.0 project opens at the design size");
        for (auto pr : { std::pair<int, int> { 1170, 1170 }, { 5000, OgdoadEditor::kMaxWidth }, { 100, OgdoadEditor::kMinWidth } })
        {
            OgdoadProcessor f; f.getValueTreeState().state.setProperty (OgdoadEditor::widthProperty, pr.first, nullptr);
            juce::MemoryBlock fb; f.getStateInformation (fb);
            OgdoadProcessor g2; g2.setStateInformation (fb.getData(), (int) fb.getSize());
            std::unique_ptr<OgdoadEditor> edF (dynamic_cast<OgdoadEditor*> (g2.createEditor()));
            expect (edF->getWidth() == pr.second && edF->getHeight() == OgdoadEditor::heightForWidth (pr.second),
                    "saved window width " + juce::String (pr.first) + " reopens at " + juce::String (pr.second));
        }
    }

    // ---- 4: MIDI --------------------------------------------------------------------------------------------------
    {
        OgdoadProcessor p; p.prepareToPlay (48000.0, 512);
        setParam (p, "exOn", 0.0f);                  // first the eight LFOs alone, exactly as in 0.1.0
        std::vector<Msg> in {
            { 100, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
            { 2000, juce::MidiMessage::controllerEvent (1, 7, 90) },
            { 30000, juce::MidiMessage::noteOff (1, 60) },
            { 40000, juce::MidiMessage::pitchWheel (2, 9000) },
        };
        auto out = runBlocks (p, 2.0, 512, in);
        std::vector<Msg> others; int lfoCc = 0; bool channelOk = true; std::set<int> numbers;
        for (auto& e : out)
        {
            if (isLfoCc (e.m) && e.m.getChannel() == 16) { ++lfoCc; numbers.insert (e.m.getControllerNumber()); }
            else others.push_back (e);
            if (isLfoCc (e.m) && e.m.getChannel() != 16) channelOk = false;
        }
        bool passOk = others.size() == in.size();
        for (size_t i = 0; passOk && i < in.size(); ++i)
            passOk = others[i].sample == in[i].sample && others[i].m.getDescription() == in[i].m.getDescription();
        expect (passOk, "notes, CC and pitch bend pass through unchanged, at the same moment");
        expect (lfoCc > 1000 && channelOk, "the LFOs come out as CC on channel 16 (" + juce::String (lfoCc) + " messages in 2 s)");
        expect (numbers.size() == 16, "Ex off: CC 20-27 and 52-59 all used, nothing else");
        setParam (p, "exOn", 1.0f);

        setParam (p, "outChannel", 3.0f);
        auto out3 = runBlocks (p, 0.5, 512);
        bool all3 = ! out3.empty();
        for (auto& e : out3) all3 = all3 && (e.m.getChannel() == 3 || (isTrigOff (e.m) && e.m.getChannel() == 16));
        expect (all3, "MIDI out channel 3: the CC moves to channel 3 (a pulse already on ends on 16)");
        setParam (p, "outChannel", 16.0f);

        // Mod wheel through Mod fully anticlockwise: stall. The wheel itself still passes through.
        setParam (p, "modSource", 9.0f); setParam (p, "modAmount", -1.0f);
        auto outWheel = runBlocks (p, 0.3, 512, { { 10, juce::MidiMessage::controllerEvent (1, 1, 127) } });
        int wheelSeen = 0; for (auto& e : outWheel) if (e.m.isController() && e.m.getControllerNumber() == 1) ++wheelSeen;
        expect (wheelSeen == 1, "mod wheel passes through");
        auto afterStall = runBlocks (p, 0.5, 512);
        expect (afterStall.empty() && p.held.load() == false && p.lanes[0].hz.load() == 0.0f, "mod wheel up with Mod fully anticlockwise stalls all eight");
        runBlocks (p, 0.1, 512, { { 0, juce::MidiMessage::controllerEvent (1, 1, 0) } });
        expect (p.lanes[0].hz.load() > 4.0f, "mod wheel down: running again");
        setParam (p, "modSource", 8.0f); setParam (p, "modAmount", 0.0f);

        // Sustain pedal with Pedal on.
        setParam (p, "holdPedal", 1.0f);
        runBlocks (p, 0.1, 512, { { 0, juce::MidiMessage::controllerEvent (1, 64, 127) } });
        auto pedalHeld = runBlocks (p, 0.5, 512);
        expect (pedalHeld.empty() && p.held.load(), "sustain pedal down holds (Pedal on)");
        runBlocks (p, 0.1, 512, { { 0, juce::MidiMessage::controllerEvent (1, 64, 0) } });
        expect (! p.held.load(), "pedal up releases");
        setParam (p, "holdPedal", 0.0f);

        // Bypass: MIDI passes, nothing added (except the end of a trigger that was on).
        auto byp = runBlocks (p, 0.5, 512, { { 50, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80) } }, true);
        int bypNotes = 0; bool bypOther = true;
        for (auto& e : byp) { if (e.m.isNoteOn() && e.m.getNoteNumber() == 64) ++bypNotes; else bypOther = bypOther && isTrigOff (e.m); }
        expect (bypNotes == 1 && bypOther, "bypassed: notes pass, no CC added");

        // Hold button: silent after the last value.
        setParam (p, "hold", 1.0f);
        runBlocks (p, 0.05, 512);
        expect (runBlocks (p, 1.0, 512).empty(), "Hold: nothing sent while held");
        setParam (p, "hold", 0.0f);
    }

    // ---- block sizes inside the real plugin --------------------------------------------------------------------------
    {
        auto stream = [] (int block)
        {
            OgdoadProcessor p; p.prepareToPlay (48000.0, 1024);
            setParam (p, "modAmount", 0.35f); setParam (p, "drift", 2.0f);
            return runBlocks (p, 5.0, block);
        };
        auto ref = stream (512);
        for (int b : { 64, 333, 1024, 0 })
        {
            auto o = stream (b);
            bool same = o.size() == ref.size();
            for (size_t i = 0; same && i < o.size(); ++i) same = o[i].sample == ref[i].sample && o[i].m.getDescription() == ref[i].m.getDescription();
            expect (same, "same CC stream with block size " + (b > 0 ? juce::String (b) : juce::String ("random")));
        }
    }

    // ---- 5: Ogdoad Ex in the real plugin --------------------------------------------------------------------------
    {
        auto run = [] (std::function<void (OgdoadProcessor&)> setup, double seconds)
        {
            OgdoadProcessor p; p.prepareToPlay (48000.0, 512);
            setup (p);
            auto out = runBlocks (p, seconds, 512);
            setParam (p, "hold", 1.0f);
            auto tail = runBlocks (p, 0.1, 512);
            out.insert (out.end(), tail.begin(), tail.end());
            return out;
        };
        auto count = [] (const std::vector<Msg>& v, std::function<bool (const juce::MidiMessage&)> f)
        { int n = 0; for (auto& e : v) if (f (e.m)) ++n; return n; };

        // Default: all sixteen outputs, CC pulses, everything on channel 16.
        auto def = run ([] (OgdoadProcessor&) {}, 10.0);
        std::set<int> nums; bool ch16 = true;
        for (auto& e : def) { if (e.m.isController()) nums.insert (e.m.getControllerNumber()); ch16 = ch16 && e.m.getChannel() == 16; }
        std::set<int> want;
        for (int n = 12; n <= 31; ++n) { want.insert (n); want.insert (n + 32); }
        for (int n = 102; n <= 105; ++n) want.insert (n);
        expect (nums == want, "Ex on: LFOs, the twelve Ex CC pairs (12-19, 28-31 / 44-51, 60-63) and trig CC 102-105, nothing else");
        expect (ch16, "Ex: everything on channel 16");
        expect (count (def, [] (auto& m) { return m.isNoteOnOrOff(); }) == 0, "CC pulse mode: no notes");
        bool alt = true; std::map<int, int> state;
        for (auto& e : def)
            if (e.m.isController() && e.m.getControllerNumber() >= 102)
            {
                const int n = e.m.getControllerNumber(), v = e.m.getControllerValue();
                alt = alt && (v == 127 ? state[n] == 0 : (v == 0 && state[n] == 127));
                state[n] = v;
            }
        bool allOff = true; for (auto& kv : state) allOff = allOff && kv.second == 0;
        expect (alt && allOff && state.size() == 4, "each trig CC pulses 127 then 0, and ends at 0");

        // Note mode: notes 36-39, each with its note off, no trig CC.
        auto notes = run ([] (OgdoadProcessor& p) { setParam (p, "trigMode", 1.0f); }, 10.0);
        const int ons = count (notes, [] (auto& m) { return m.isNoteOn() && m.getNoteNumber() >= 36 && m.getNoteNumber() <= 39 && m.getVelocity() == 100; });
        const int offs = count (notes, [] (auto& m) { return m.isNoteOff() && m.getNoteNumber() >= 36 && m.getNoteNumber() <= 39; });
        expect (ons > 10 && ons == offs, "Note mode: trigger notes C1 to D#1, velocity 100, every one ended (" + juce::String (ons) + ")");
        expect (count (notes, [] (auto& m) { return m.isController() && m.getControllerNumber() >= 102 && m.getControllerNumber() <= 105; }) == 0,
                "Note mode: no trig CC");
        auto both = run ([] (OgdoadProcessor& p) { setParam (p, "trigMode", 2.0f); }, 10.0);
        expect (count (both, [] (auto& m) { return m.isNoteOn(); }) == count (both, [] (auto& m) { return m.isController() && m.getControllerNumber() >= 102 && m.getControllerValue() == 127; })
                && count (both, [] (auto& m) { return m.isNoteOn(); }) > 10, "Both: a note with every CC pulse");

        // The chain in the plugin: 2, 4 and 6 switched off, so everything comes out of trig 8.
        auto chain = run ([] (OgdoadProcessor& p) { setParam (p, "trigUse1", 0.0f); setParam (p, "trigUse2", 0.0f); setParam (p, "trigUse3", 0.0f); }, 10.0);
        const int only8 = count (chain, [] (auto& m) { return m.isController() && m.getControllerNumber() == 105 && m.getControllerValue() == 127; });
        const int others = count (chain, [] (auto& m) { return m.isController() && m.getControllerNumber() >= 102 && m.getControllerNumber() <= 104; });
        const int lfo2 = count (def, [] (auto& m) { return m.isController() && m.getControllerNumber() == 102 && m.getControllerValue() == 127; });
        expect (others == 0 && only8 >= lfo2, "2, 4, 6 switched off: their triggers come out of trig 8 (" + juce::String (only8) + " pulses)");

        // Ex off: exactly the 0.1.0 output.
        auto off = run ([] (OgdoadProcessor& p) { setParam (p, "exOn", 0.0f); setParam (p, "trigMode", 2.0f); }, 5.0);
        expect (! off.empty() && count (off, [] (auto& m) { return ! isLfoCc (m); }) == 0, "Ex off: only the eight LFOs");

        // Note mode, then bypass in the middle of a note: the note still ends.
        OgdoadProcessor p; p.prepareToPlay (48000.0, 512);
        setParam (p, "trigMode", 1.0f);
        std::vector<Msg> all;
        for (int i = 0; i < 2000 && ! p.trigOn[0].load(); ++i) { auto o = runBlocks (p, 512.0 / 48000.0, 512); all.insert (all.end(), o.begin(), o.end()); }
        auto b = runBlocks (p, 0.1, 512, {}, true);
        all.insert (all.end(), b.begin(), b.end());
        expect (count (all, [] (auto& m) { return m.isNoteOn(); }) == count (all, [] (auto& m) { return m.isNoteOff(); }) && ! all.empty(),
                "bypassed during a trigger note: the note ends");
    }

    if (failures == 0) std::cout << "preview checks: " << checks << " checks passed" << std::endl;
    else std::cout << "preview checks: " << failures << " of " << checks << " checks FAILED" << std::endl;
    return failures == 0 ? 0 : 1;
}
