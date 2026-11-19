// The Ogdoad engine: eight free-running triangle LFOs, fastest at the top, slowest at the bottom,
// with one global Rate, a Mod amount (-1 to +1) fed by a chosen source (one of the LFOs, or the mod wheel),
// Hold, and a slow random Drift so the eight never repeat exactly.
//
// Plain C++17. Nothing here knows about JUCE, Logic or MIDI devices; the plugin hands in the control values and
// the incoming mod wheel / sustain pedal, and gets back CC messages (see CcStream.h) placed at exact sample positions.
// The engine works in fixed ticks of 32 samples, counted from the start, so its output does not depend on how Logic
// slices the audio into blocks.

#pragma once

#include "ogdoad/CcStream.h"
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

        CcStream cc;
    };
}
