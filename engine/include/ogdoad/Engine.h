// The Ogdoad engine: eight free-running triangle LFOs, fastest at the top, slowest at the bottom,
// with one global Rate, a Mod amount (-1 to +1) fed by a chosen source (one of the LFOs, or the mod wheel),
// Hold, and a slow random Drift so the eight never repeat exactly. Ogdoad Ex adds sixteen more outputs worked out from
// the eight (fold x2, max / min, steps, trig chain; see ExOutputs.h).
//
// Plain C++17. Nothing here knows about JUCE, Logic or MIDI devices; the plugin hands in the control values and
// the incoming mod wheel / sustain pedal, and gets back CC messages (see CcStream.h) placed at exact sample positions.
// The engine works in fixed ticks of 32 samples, counted from the start, so its output does not depend on how Logic
// slices the audio into blocks.

#pragma once

#include "ogdoad/CcStream.h"
#include "ogdoad/ExOutputs.h"
#include "ogdoad/Rates.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ogdoad
{
    enum class ModSource : int { off = 0, lfo1 = 1, /* lfo2..lfo8 = 2..8 */ modWheel = 9 };
    inline constexpr int kNumModSources = 10;          // Off, LFO 1..8, Mod wheel

    enum class Drift : int { off = 0, low = 1, more = 2 };

    struct Settings
    {
        double rate      = 0.5;     // the Rate knob, 0..1 (0.5 = centre: LFO 1 at about 4.9 Hz)
        double modAmount = 0.0;     // the Mod knob, -1..+1 (attenuverter)
        int    modSource = 8;       // 0 = off, 1..8 = LFO 1..8 (self-patch), 9 = mod wheel
        bool   hold      = false;   // freeze all eight
        bool   holdPedal = false;   // also freeze while the sustain pedal is down
        int    drift     = 1;       // 0 off, 1 low, 2 more

        // Ogdoad Ex
        bool   exOn      = true;    // send the sixteen Ex outputs
        bool   trigUse[kNumTrigs] { true, true, true, true };   // "in use": off = silent, triggers pass on to the next
        int    trigMode  = 0;       // 0 CC pulse, 1 note, 2 both
    };

    // Something that happened in the incoming MIDI, at a sample position inside the block.
    struct InputEvent
    {
        int    offset = 0;
        enum Kind { modWheel, pedal } kind = modWheel;
        double value  = 0.0;        // mod wheel 0..1; pedal 0 (up) or 1 (down)
    };

    class Engine
    {
    public:
        static constexpr int kTickSamples = 32;

        void prepare (double sampleRate);
        void reset();                                  // phases back to their starting spread, drift restarted

        void setSettings (const Settings& s) { settings = s; }
        const Settings& getSettings() const  { return settings; }

        // Runs one block. Input events must be sorted by offset. CC messages go to the sink with offsets inside the block.
        void render (int numSamples, const InputEvent* events, int numEvents, CcSink& sink);

        // For the panel and the tests (read after render).
        double laneValue (int i) const      { return value[(std::size_t) i]; }      // -1..+1
        double laneHz (int i) const         { return hz[(std::size_t) i]; }         // 0 when held or stalled
        bool   laneRising (int i) const     { return phase[(std::size_t) i] < 0.5; }
        bool   isHeld() const               { return held; }
        double lanePhase (int i) const      { return phase[(std::size_t) i]; }
        uint64_t ticksDone() const          { return tickCount; }
        double secondsDone() const          { return (double) tickCount * kTickSamples / sampleRate; }

        CcStream& ccStream()                { return cc; }

        // Ogdoad Ex, for the panel and the tests.
        double exValue (int i) const        { return exValues[(std::size_t) i]; }   // fold / steps 0..1, max / min -1..+1
        bool   trigOn (int k) const         { return trig[(std::size_t) k].ccOn || trig[(std::size_t) k].noteOn; }
        uint64_t trigPulses (int k) const   { return trig[(std::size_t) k].pulses; }   // pulses started (sent) so far
        uint64_t trigArrivals (int k) const { return trig[(std::size_t) k].arrivals; } // ticks a trigger reached output k

        // Ends any trigger pulse or note now (used when the plugin is bypassed), so nothing is left hanging.
        void closeTriggers (int offset, CcSink& sink);

    private:
        void tick (int offset, CcSink& sink);
        double driftOctaves (int lane, double tSeconds);

        Settings settings;
        double sampleRate = 48000.0;
        int samplesToTick = 0;
        uint64_t tickCount = 0;

        std::array<double, kNumLanes> phase {}, value {}, hz {};
        double modWheel = 0.0;
        bool pedalDown = false;
        bool held = false;

        // Drift: per lane, a smooth random wander between points drawn at fixed times.
        struct Wander { double t0 = 0, t1 = 0, a = 0, b = 0; uint32_t rng = 1; };
        std::array<Wander, kNumLanes> wander {};

        void tickEx (int offset, CcSink& sink, const std::array<bool, kNumLanes>& wrapped);
        void endPulse (int k, int offset, CcSink& sink);

        std::array<double, kNumExSmooth> exValues {};
        struct Trig { int ticksLeft = 0; bool ccOn = false, noteOn = false; uint64_t pulses = 0, arrivals = 0; };
        std::array<Trig, kNumTrigs> trig {};
        int pulseTicks = 30;

        CcStream cc;
    };
}
