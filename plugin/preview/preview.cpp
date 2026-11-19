// Offline window picture and plugin checks (not part of the plugin; only built with -DOGDOAD_BUILD_PREVIEW=ON).
//   1. draws the window to picture files (default, running, held);
//   2. checks that all 7 ids exist, each has exactly one control, and each control moves exactly its own parameter;
//   3. checks saving and reloading;
//   4. checks the MIDI: notes and other messages pass through untouched, the LFOs come out as CC 20-27 / 52-59 on the
//      chosen channel, the mod wheel and sustain pedal work, bypass adds nothing, and Logic's block size makes no difference.
// Usage: OgdoadPreview <folder for the pictures>

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    bool isOgdoadCc (const juce::MidiMessage& m)
    {
        if (! m.isController()) return false;
        const int n = m.getControllerNumber();
        return (n >= 20 && n <= 27) || (n >= 52 && n <= 59);
    }
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
        expect (proc.getParameters().size() == 7, "7 parameters");
        for (auto id : { "rate", "modAmount", "modSource", "hold", "holdPedal", "drift", "outChannel" })
            expect (param (proc, id) != nullptr, juce::String ("parameter id ") + id);
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
        expect (controls.size() == 7 && ids.size() == 7, "every parameter has exactly one control");
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
            expect (c.control->isVisible() && ! c.control->getBounds().isEmpty()
                    && ed->getLocalBounds().contains (c.control->getBounds()), "control for " + c.paramId + " is visible inside the window");
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
        expect (boxes.size() == 3 && boxes[0]->getText() == "Mod source: LFO 8" && boxes[1]->getText() == "Drift: low" && boxes[2]->getText() == "16",
                "menus read 'Mod source: LFO 8', 'Drift: low', '16'");
    }

    // ---- 3: saving and reloading ----------------------------------------------------------------------------------
    {
        OgdoadProcessor a;
        setParam (a, "rate", 0.31f); setParam (a, "modAmount", -0.4f); setParam (a, "modSource", 9.0f);
        setParam (a, "hold", 1.0f); setParam (a, "holdPedal", 1.0f); setParam (a, "drift", 2.0f); setParam (a, "outChannel", 5.0f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        OgdoadProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        expect (snapshot (a) == snapshot (b), "every setting comes back after save and reload");
    }

    // ---- 4: MIDI --------------------------------------------------------------------------------------------------
    {
        OgdoadProcessor p; p.prepareToPlay (48000.0, 512);
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
            if (isOgdoadCc (e.m) && e.m.getChannel() == 16) { ++lfoCc; numbers.insert (e.m.getControllerNumber()); }
            else others.push_back (e);
            if (isOgdoadCc (e.m) && e.m.getChannel() != 16) channelOk = false;
        }
        bool passOk = others.size() == in.size();
        for (size_t i = 0; passOk && i < in.size(); ++i)
            passOk = others[i].sample == in[i].sample && others[i].m.getDescription() == in[i].m.getDescription();
        expect (passOk, "notes, CC and pitch bend pass through unchanged, at the same moment");
        expect (lfoCc > 1000 && channelOk, "the LFOs come out as CC on channel 16 (" + juce::String (lfoCc) + " messages in 2 s)");
        expect (numbers.size() == 16, "CC 20-27 and 52-59 all used");

        setParam (p, "outChannel", 3.0f);
        auto out3 = runBlocks (p, 0.5, 512);
        bool all3 = ! out3.empty();
        for (auto& e : out3) all3 = all3 && e.m.getChannel() == 3;
        expect (all3, "MIDI out channel 3: the CC moves to channel 3");
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

        // Bypass: MIDI passes, nothing added.
        auto byp = runBlocks (p, 0.5, 512, { { 50, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80) } }, true);
        expect (byp.size() == 1 && byp[0].m.isNoteOn(), "bypassed: notes pass, no CC added");

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

    if (failures == 0) std::cout << "preview checks: " << checks << " checks passed" << std::endl;
    else std::cout << "preview checks: " << failures << " of " << checks << " checks FAILED" << std::endl;
    return failures == 0 ? 0 : 1;
}
