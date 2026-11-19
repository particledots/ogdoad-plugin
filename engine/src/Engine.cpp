#include "ogdoad/Engine.h"

#include <algorithm>
#include <cmath>

namespace ogdoad
{
    namespace
    {
        // Where each LFO starts, so the eight do not all begin at the bottom together.
        constexpr double kStartPhase[kNumLanes] = { 0.00, 0.37, 0.71, 0.13, 0.52, 0.89, 0.26, 0.64 };

        // Drift: how far each LFO wanders from its speed (in octaves), and how long each wander takes.
        constexpr double kDriftOctaves[3] = { 0.0, 0.05, 0.18 };
        double wanderSeconds (int lane) { return 3.0 + 0.9 * lane; }

        uint32_t nextRandom (uint32_t& s)
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return s;
        }

        double randomBipolar (uint32_t& s) { return (double) (nextRandom (s) & 0xFFFFFF) / (double) 0x7FFFFF - 1.0; }

        double triangle (double ph)       // phase 0..1 -> -1..+1, bottom at phase 0, top at 0.5
        {
            return ph < 0.5 ? -1.0 + 4.0 * ph : 3.0 - 4.0 * ph;
        }
    }

    void Engine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        for (int i = 0; i < kNumLanes; ++i)
            cc.setLane (i, kLfoMsbCc + i, kLfoLsbCc + i);
        for (int i = 0; i < kNumExSmooth; ++i)
            cc.setLane (kNumLanes + i, kExMsbCc[i], kExLsbCc[i]);
        pulseTicks = std::max (1, (int) std::lround (kTrigSeconds * sampleRate / kTickSamples));
        reset();
    }

    void Engine::reset()
    {
        samplesToTick = 0;
        tickCount = 0;
        held = false;
        modWheel = 0.0;
        pedalDown = false;
        for (int i = 0; i < kNumLanes; ++i)
        {
            phase[(std::size_t) i] = kStartPhase[i];
            value[(std::size_t) i] = triangle (kStartPhase[i]);
            hz[(std::size_t) i] = 0.0;
            auto& w = wander[(std::size_t) i];
            w.rng = 0x9E3779B9u ^ (uint32_t) (i * 0x85EBCA6Bu + 1u);
            w.t0 = 0.0;
            w.t1 = wanderSeconds (i);
            w.a = 0.0;
            w.b = randomBipolar (w.rng);
        }
        for (auto& t : trig)
        {
            t.ticksLeft = 0;            // a pulse still on is ended at the next tick, so no note is left hanging
            t.pulses = t.arrivals = 0;
        }
        exValues.fill (0.0);
        cc.reset();
    }

    double Engine::driftOctaves (int lane, double t)
    {
        auto& w = wander[(std::size_t) lane];
        while (t >= w.t1)
        {
            w.a = w.b;
            w.t0 = w.t1;
            w.t1 = w.t0 + wanderSeconds (lane);
            w.b = randomBipolar (w.rng);
        }
        double x = (t - w.t0) / (w.t1 - w.t0);
        x = x * x * (3.0 - 2.0 * x);
        return w.a + (w.b - w.a) * x;
    }

    void Engine::render (int numSamples, const InputEvent* events, int numEvents, CcSink& sink)
    {
        int pos = 0, ev = 0;
        while (pos < numSamples)
        {
            if (samplesToTick == 0)
            {
                while (ev < numEvents && events[ev].offset <= pos)
                {
                    if (events[ev].kind == InputEvent::modWheel) modWheel = std::clamp (events[ev].value, 0.0, 1.0);
                    else pedalDown = events[ev].value >= 0.5;
                    ++ev;
                }
                tick (pos, sink);
                samplesToTick = kTickSamples;
            }
            const int n = std::min (samplesToTick, numSamples - pos);
            pos += n;
            samplesToTick -= n;
        }
        // Events after the last tick of this block are applied now (they would be first at the next tick anyway).
        for (; ev < numEvents; ++ev)
        {
            if (events[ev].kind == InputEvent::modWheel) modWheel = std::clamp (events[ev].value, 0.0, 1.0);
            else pedalDown = events[ev].value >= 0.5;
        }
    }

    void Engine::tick (int offset, CcSink& sink)
    {
        const double dt = (double) kTickSamples / sampleRate;
        const double t = (double) tickCount * dt;
        held = settings.hold || (settings.holdPedal && pedalDown);

        const int src = std::clamp (settings.modSource, 0, kNumModSources - 1);
        double srcValue = 0.0;
        if (src >= 1 && src <= kNumLanes) srcValue = value[(std::size_t) (src - 1)];
        else if (src == (int) ModSource::modWheel) srcValue = modWheel;

        const double control = std::clamp (settings.rate, 0.0, 1.0) + std::clamp (settings.modAmount, -1.0, 1.0) * srcValue;
        const double driftAmount = kDriftOctaves[std::clamp (settings.drift, 0, 2)];

        std::array<bool, kNumLanes> wrapped {};
        for (int i = 0; i < kNumLanes; ++i)
        {
            const double d = driftOctaves (i, t) * driftAmount;      // always advanced, so switching Drift on is smooth
            const double f = laneFrequency (i, control, d);
            hz[(std::size_t) i] = held ? 0.0 : f;
            if (! held && f > 0.0)
            {
                double p = phase[(std::size_t) i] + f * dt;
                p -= std::floor (p);
                wrapped[(std::size_t) i] = p < phase[(std::size_t) i];     // passed the bottom: starts rising
                phase[(std::size_t) i] = p;
            }
            value[(std::size_t) i] = triangle (phase[(std::size_t) i]);
            cc.update (i, value[(std::size_t) i], dt, offset, sink);
        }
        tickEx (offset, sink, wrapped);
        ++tickCount;
    }

    void Engine::tickEx (int offset, CcSink& sink, const std::array<bool, kNumLanes>& wrapped)
    {
        const double dt = (double) kTickSamples / sampleRate;
        auto rising = [this] (int lane) { return phase[(std::size_t) lane] < 0.5; };

        // The smooth and stepped outputs.
        for (int k = 0; k < kNumFold; ++k)
            exValues[(std::size_t) k] = std::fabs (value[(std::size_t) kFoldLfo[k]]);
        for (int k = 0; k < kNumMaxMin; ++k)
        {
            const double a = value[(std::size_t) kMaxMinA[k]], b = value[(std::size_t) kMaxMinB[k]];
            exValues[(std::size_t) (kNumFold + k)] = kMaxMinIsMax[k] ? std::max (a, b) : std::min (a, b);
        }
        for (int k = 0; k < kNumSteps; ++k)
        {
            int level = 0;
            for (int bit = 0; bit < 4; ++bit)
                level = level * 2 + (rising (kStepsBits[k][bit]) ? 1 : 0);
            exValues[(std::size_t) (kNumFold + kNumMaxMin + k)] = level / 15.0;
        }
        if (settings.exOn)
            for (int i = 0; i < kNumExSmooth; ++i)
            {
                const double v = exValues[(std::size_t) i];
                cc.update (kNumLanes + i, kExBipolar[i] ? v : v * 2.0 - 1.0, dt, offset, sink);
            }

        // The trig chain: a trigger reaching an output that is not in use passes on to the next one.
        const int mode = std::clamp (settings.trigMode, 0, 2);
        bool carried = false;
        for (int k = 0; k < kNumTrigs; ++k)
        {
            auto& tr = trig[(std::size_t) k];
            const bool arrive = wrapped[(std::size_t) kTrigLfo[k]] || carried;
            const bool inUse = settings.trigUse[k];
            carried = arrive && ! inUse;
            if (arrive) ++tr.arrivals;

            const bool active = tr.ccOn || tr.noteOn;
            if (arrive && inUse && settings.exOn)
            {
                if (! active)
                {
                    if (mode != (int) TrigMode::note) { sink.cc (offset, kTrigCc[k], 127); tr.ccOn = true; }
                    if (mode != (int) TrigMode::ccPulse) { sink.note (offset, kTrigNote[k], kTrigVelocity); tr.noteOn = true; }
                    ++tr.pulses;
                }
                tr.ticksLeft = pulseTicks;          // a trigger during a pulse keeps it on (the two merge)
            }
            else if (active && --tr.ticksLeft <= 0)
                endPulse (k, offset, sink);
        }
    }

    void Engine::endPulse (int k, int offset, CcSink& sink)
    {
        auto& tr = trig[(std::size_t) k];
        if (tr.ccOn) sink.cc (offset, kTrigCc[k], 0);
        if (tr.noteOn) sink.note (offset, kTrigNote[k], 0);
        tr.ccOn = tr.noteOn = false;
        tr.ticksLeft = 0;
    }

    void Engine::closeTriggers (int offset, CcSink& sink)
    {
        for (int k = 0; k < kNumTrigs; ++k)
            if (trig[(std::size_t) k].ccOn || trig[(std::size_t) k].noteOn)
                endPulse (k, offset, sink);
    }
}
