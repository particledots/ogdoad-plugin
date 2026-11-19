// Engine self-test. Runs on the GitHub build (Linux) before the Mac build, and prints one summary line
// "engine tests: N checks passed" or the failures.

#include "ogdoad/Engine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace ogdoad;

namespace
{
    int checks = 0, failures = 0;

    void expect (bool ok, const std::string& what)
    {
        ++checks;
        if (! ok) { ++failures; std::printf ("FAIL: %s\n", what.c_str()); }
    }

    struct Event { long long sample; int cc; int value; };

    struct Collector : CcSink
    {
        long long blockStart = 0;
        std::vector<Event> events;           // CC
        std::vector<Event> notes;            // notes: cc = note number, value = velocity (0 = off)
        void cc (int offset, int number, int v) override { events.push_back ({ blockStart + offset, number, v }); }
        void note (int offset, int number, int v) override { notes.push_back ({ blockStart + offset, number, v }); }
    };

    struct NullSink : CcSink { long count = 0; void cc (int, int, int) override { ++count; } };

    // Runs an engine for a number of seconds in blocks of a given size (0 = random sizes 1..1024).
    void run (Engine& e, Collector& c, double seconds, double sr, int block, unsigned seed = 1)
    {
        std::mt19937 rng (seed);
        long long total = (long long) std::llround (seconds * sr);
        while (total > 0)
        {
            int n = block > 0 ? block : (int) (rng() % 1024) + 1;
            if (n > total) n = (int) total;
            e.render (n, nullptr, 0, c);
            c.blockStart += n;
            total -= n;
        }
    }

    Settings plain (double rate = 0.5)
    {
        Settings s;
        s.rate = rate; s.modAmount = 0.0; s.modSource = 0; s.drift = 0;
        return s;
    }

    double circularDiff (double a, double b)
    {
        double d = std::fabs (a - b);
        d -= std::floor (d);
        return std::fmin (d, 1.0 - d);
    }

    // ---------------------------------------------------------------------------------------------
    void testRateLaw()
    {
        expect (std::fabs (laneFrequency (0, 0.5) - 4.9) < 1e-9, "centre: LFO 1 at 4.9 Hz");
        expect (laneFrequency (0, 0.5) >= 3.0 && laneFrequency (0, 0.5) <= 7.0, "centre: LFO 1 inside the manual's 3 to 7 Hz");
        expect (std::fabs (1.0 / laneFrequency (7, 0.5) - 19.0) < 1e-6, "centre: LFO 8 one cycle in 19 s");
        expect (std::fabs (laneFrequency (0, 1.0) - 160.0) < 1e-6, "fully clockwise: LFO 1 at 160 Hz");
        expect (std::fabs (1.0 / laneFrequency (7, 0.0) - 1500.0) < 1e-3, "fully anticlockwise: LFO 8 one cycle in 25 minutes");
        expect (laneFrequency (3, -0.01) == 0.0, "control below zero stalls");
        for (int i = 0; i + 1 < kNumLanes; ++i)
        {
            const double r = kCentreHz[i] / kCentreHz[i + 1];
            expect (r > 1.8 && r < 2.0, "each LFO about 1.9 times slower than the one above (" + std::to_string (i + 1) + ")");
            for (int j = i + 1; j < kNumLanes && j <= i + 2; ++j)     // near neighbours: where locking would be heard
            {
                const double q = kCentreHz[i] / kCentreHz[j];
                expect (std::fabs (q - std::round (q)) > 0.03, "neighbour speed ratio is not a whole number (" + std::to_string (i + 1) + "/" + std::to_string (j + 1) + ")");
            }
        }
        // Measured: count wraps of the phase for each lane over a long run.
        Engine e; e.prepare (48000.0); e.setSettings (plain (0.5));
        NullSink n;
        std::vector<int> wraps (kNumLanes, 0);
        std::vector<double> last (kNumLanes);
        for (int i = 0; i < kNumLanes; ++i) last[(size_t) i] = e.lanePhase (i);
        const int ticks = (int) (200.0 * 48000.0 / Engine::kTickSamples);
        for (int t = 0; t < ticks; ++t)
        {
            e.render (Engine::kTickSamples, nullptr, 0, n);
            for (int i = 0; i < kNumLanes; ++i)
            {
                if (e.lanePhase (i) < last[(size_t) i]) ++wraps[(size_t) i];
                last[(size_t) i] = e.lanePhase (i);
            }
        }
        for (int i = 0; i < kNumLanes; ++i)
        {
            const double expected = kCentreHz[i] * 200.0;
            expect (std::fabs (wraps[(size_t) i] - expected) <= 1.0 + expected * 0.002,
                    "measured speed of LFO " + std::to_string (i + 1) + " (" + std::to_string (wraps[(size_t) i]) + " cycles, expected " + std::to_string (expected) + ")");
        }
    }

    void testTriangle()
    {
        Engine e; e.prepare (48000.0); e.setSettings (plain (0.5));
        NullSink n;
        double lo = 2, hi = -2, maxStep = 0;
        double prev = e.laneValue (3);
        const double perTick = 4.0 * 0.72 * Engine::kTickSamples / 48000.0;
        bool linear = true;
        for (int t = 0; t < 48000 * 4 / Engine::kTickSamples; ++t)
        {
            e.render (Engine::kTickSamples, nullptr, 0, n);
            const double v = e.laneValue (3);
            lo = std::fmin (lo, v); hi = std::fmax (hi, v);
            const double step = std::fabs (v - prev);
            maxStep = std::fmax (maxStep, step);
            // Away from the turning points the slope is exactly constant.
            if (std::fabs (v) < 0.9 && std::fabs (prev) < 0.9 && std::fabs (step - perTick) > 1e-9) linear = false;
            prev = v;
        }
        expect (lo < -0.995 && hi > 0.995, "triangle reaches -1 and +1");
        expect (lo >= -1.0 && hi <= 1.0, "triangle never goes past -1 or +1");
        expect (linear, "triangle slopes are straight lines");
        expect (maxStep <= perTick + 1e-9, "triangle has no jumps");
    }

    void testHold()
    {
        Engine e; e.prepare (48000.0); e.setSettings (plain (0.5));
        Collector c;
        run (e, c, 1.0, 48000.0, 256);
        Settings s = plain (0.5); s.hold = true;
        e.setSettings (s);
        run (e, c, 0.05, 48000.0, 256);            // settle the last value; any trigger pulse ends within 20 ms
        std::vector<double> frozen (kNumLanes);
        for (int i = 0; i < kNumLanes; ++i) frozen[(size_t) i] = e.laneValue (i);
        const size_t before = c.events.size();
        run (e, c, 3.0, 48000.0, 256);
        bool same = true;
        for (int i = 0; i < kNumLanes; ++i) same = same && e.laneValue (i) == frozen[(size_t) i];
        expect (same, "Hold freezes all eight");
        expect (c.events.size() == before, "nothing is sent while held");
        expect (e.isHeld(), "engine reports held");
        for (int i = 0; i < kNumLanes; ++i) expect (e.laneHz (i) == 0.0, "held lane shows 0 Hz");

        e.setSettings (plain (0.5));
        e.render (Engine::kTickSamples, nullptr, 0, c);
        bool smooth = true;
        for (int i = 0; i < kNumLanes; ++i)
            smooth = smooth && std::fabs (e.laneValue (i) - frozen[(size_t) i]) <= 4.0 * kCentreHz[i] * Engine::kTickSamples / 48000.0 + 1e-9;
        expect (smooth, "releasing Hold carries on from where it stopped, no jump");

        // Sustain pedal hold.
        Engine p; p.prepare (48000.0);
        Settings ps = plain (0.5); ps.holdPedal = true; p.setSettings (ps);
        NullSink n;
        InputEvent down { 0, InputEvent::pedal, 1.0 };
        p.render (Engine::kTickSamples, &down, 1, n);
        const double v = p.laneValue (0);
        for (int t = 0; t < 100; ++t) p.render (Engine::kTickSamples, nullptr, 0, n);
        expect (p.laneValue (0) == v, "sustain pedal down holds when Pedal is on");
        InputEvent up { 0, InputEvent::pedal, 0.0 };
        p.render (Engine::kTickSamples, &up, 1, n);
        expect (p.laneValue (0) != v, "pedal up releases");

        Engine q; q.prepare (48000.0); q.setSettings (plain (0.5));
        q.render (Engine::kTickSamples, &down, 1, n);
        const double w = q.laneValue (0);
        q.render (Engine::kTickSamples, nullptr, 0, n);
        expect (q.laneValue (0) != w, "pedal does nothing when Pedal is off");
    }

    void testModulation()
    {
        // Mod wheel through the Mod knob turned fully anticlockwise: the stall / track and hold patch.
        Engine e; e.prepare (48000.0);
        Settings s = plain (0.5); s.modSource = (int) ModSource::modWheel; s.modAmount = -1.0;
        e.setSettings (s);
        NullSink n;
        InputEvent wheel { 0, InputEvent::modWheel, 1.0 };
        e.render (Engine::kTickSamples, &wheel, 1, n);
        const double v = e.laneValue (2);
        for (int t = 0; t < 200; ++t) e.render (Engine::kTickSamples, nullptr, 0, n);
        expect (e.laneValue (2) == v && e.laneHz (2) == 0.0, "mod wheel with Mod fully anticlockwise stalls the LFOs");
        InputEvent wheelDown { 0, InputEvent::modWheel, 0.0 };
        e.render (Engine::kTickSamples, &wheelDown, 1, n);
        expect (std::fabs (e.laneHz (0) - 4.9) < 1e-9, "mod wheel back down: speeds back to normal");

        // Positive mod speeds up past the knob's range.
        Settings f = plain (1.0); f.modSource = (int) ModSource::modWheel; f.modAmount = 1.0;
        Engine g; g.prepare (48000.0); g.setSettings (f);
        g.render (Engine::kTickSamples, &wheel, 1, n);
        expect (g.laneHz (0) > 160.0, "positive mod can push past 160 Hz");
        expect (g.laneHz (0) < 2000.0, "but stays capped");

        // Every self-patch, every amount, every rate: stable.
        bool stable = true, bounded = true, moved = true;
        for (int src = 1; src <= 9; ++src)
            for (double amt : { -1.0, -0.5, 0.5, 1.0 })
                for (double rate : { 0.0, 0.3, 0.5, 1.0 })
                {
                    Engine x; x.prepare (44100.0);
                    Settings xs; xs.rate = rate; xs.modAmount = amt; xs.modSource = src; xs.drift = 2;
                    x.setSettings (xs);
                    NullSink nn;
                    InputEvent half { 0, InputEvent::modWheel, 0.6 };
                    x.render (64, &half, 1, nn);
                    for (int b = 0; b < (int) (60.0 * 44100.0 / 512.0); ++b) x.render (512, nullptr, 0, nn);
                    for (int i = 0; i < kNumLanes; ++i)
                    {
                        if (! std::isfinite (x.laneValue (i)) || ! std::isfinite (x.lanePhase (i))) stable = false;
                        if (std::fabs (x.laneValue (i)) > 1.0) bounded = false;
                    }
                    if (rate >= 0.5 && amt > 0 && nn.count == 0) moved = false;
                }
        expect (stable, "feedback self-patch: no runaway values");
        expect (bounded, "feedback self-patch: outputs stay within -1..+1");
        expect (moved, "feedback self-patch: still moving");

        // The self-patch actually changes the shape (the lane modulating itself is no longer a straight triangle).
        Engine a; a.prepare (48000.0);
        Settings as = plain (0.5); as.modSource = 4; as.modAmount = 0.8; a.setSettings (as);
        bool varied = false; double prevHz = -1;
        for (int t = 0; t < 3000; ++t)
        {
            a.render (Engine::kTickSamples, nullptr, 0, n);
            if (prevHz >= 0 && std::fabs (a.laneHz (0) - prevHz) > 1e-6) varied = true;
            prevHz = a.laneHz (0);
        }
        expect (varied, "self-patch from LFO 4 sweeps the speeds");
    }

    void testDrift()
    {
        Engine a; a.prepare (48000.0);
        Settings s = plain (0.5); s.drift = 1; a.setSettings (s);
        NullSink n;
        double minHz = 1e9, maxHz = 0;
        for (int t = 0; t < (int) (60.0 * 48000.0 / Engine::kTickSamples); ++t)
        {
            a.render (Engine::kTickSamples, nullptr, 0, n);
            minHz = std::fmin (minHz, a.laneHz (0)); maxHz = std::fmax (maxHz, a.laneHz (0));
        }
        expect (maxHz > 4.9 * 1.005 || minHz < 4.9 / 1.005, "Drift Low wanders");
        expect (maxHz <= 4.9 * std::exp2 (0.05) + 1e-9 && minHz >= 4.9 * std::exp2 (-0.05) - 1e-9, "Drift Low stays within 0.05 octave");

        Engine b; b.prepare (48000.0);
        Settings m = plain (0.5); m.drift = 2; b.setSettings (m);
        double lo = 1e9, hi = 0;
        for (int t = 0; t < (int) (60.0 * 48000.0 / Engine::kTickSamples); ++t)
        {
            b.render (Engine::kTickSamples, nullptr, 0, n);
            lo = std::fmin (lo, b.laneHz (0)); hi = std::fmax (hi, b.laneHz (0));
        }
        expect (hi / lo > maxHz / minHz, "Drift More wanders further than Low");

        Engine c; c.prepare (48000.0); c.setSettings (plain (0.5));
        bool steady = true;
        for (int t = 0; t < 2000; ++t) { c.render (Engine::kTickSamples, nullptr, 0, n); steady = steady && c.laneHz (0) == 4.9; }
        expect (steady, "Drift Off: exact speeds");
    }

    void testSampleRates()
    {
        // The same settings give the same movement in time at 44.1, 48 and 96 kHz (drift on).
        struct Trace { std::vector<double> t; std::vector<std::vector<double>> ph; };
        auto capture = [] (double sr)
        {
            Engine e; e.prepare (sr);
            Settings s; s.rate = 0.62; s.drift = 1; s.modSource = 0;
            e.setSettings (s);
            NullSink n;
            Trace tr;
            for (int k = 1; k <= 20; ++k)
            {
                const long long target = (long long) std::llround (k * 0.5 * sr);
                while ((long long) e.ticksDone() * Engine::kTickSamples < target) e.render (Engine::kTickSamples, nullptr, 0, n);
                tr.t.push_back (e.secondsDone());
                std::vector<double> p;
                for (int i = 0; i < kNumLanes; ++i) p.push_back (e.lanePhase (i));
                tr.ph.push_back (p);
            }
            return tr;
        };
        auto a = capture (44100.0), b = capture (48000.0), c = capture (96000.0);
        double worst = 0;
        for (size_t k = 0; k < a.t.size(); ++k)
            for (int i = 0; i < kNumLanes; ++i)
            {
                // Bring each to the same moment (k * 0.5 s) using the lane's speed at that point.
                const double f = laneFrequency (i, 0.62);
                const double T = (double) (k + 1) * 0.5;
                const double pa = a.ph[k][(size_t) i] + f * (T - a.t[k]);
                const double pb = b.ph[k][(size_t) i] + f * (T - b.t[k]);
                const double pc = c.ph[k][(size_t) i] + f * (T - c.t[k]);
                worst = std::fmax (worst, std::fmax (circularDiff (pa, pb), circularDiff (pb, pc)));
            }
        expect (worst < 0.003, "same movement at 44.1, 48 and 96 kHz (worst phase difference " + std::to_string (worst) + " cycles)");
    }

    void testBlockSizes()
    {
        auto events = [] (int block, unsigned seed)
        {
            Engine e; e.prepare (48000.0);
            Settings s; s.rate = 0.55; s.modSource = 8; s.modAmount = 0.4; s.drift = 2;
            s.trigMode = (int) TrigMode::both; s.trigUse[1] = false;      // Ex on, notes and pulses, a cascade
            e.setSettings (s);
            Collector c;
            run (e, c, 20.0, 48000.0, block, seed);
            for (auto& n : c.notes) c.events.push_back ({ n.sample, 1000 + n.cc, n.value });
            return c.events;
        };
        auto ref = events (512, 1);
        expect (ref.size() > 1000, "a 20 s run sends CC");
        for (int block : { 1, 7, 32, 64, 333, 1024, 4096, 0 })
        {
            auto other = events (block, 7);
            bool same = other.size() == ref.size();
            for (size_t i = 0; same && i < ref.size(); ++i)
                same = other[i].sample == ref[i].sample && other[i].cc == ref[i].cc && other[i].value == ref[i].value;
            expect (same, "identical CC stream with block size " + (block > 0 ? std::to_string (block) : std::string ("random")));
        }
    }

    void testCcStream()
    {
        const double sr = 48000.0;
        // Slowest possible: Rate fully anticlockwise. LFO 8 then never jumps more than one fine step.
        {
            Engine e; e.prepare (sr); e.setSettings (plain (0.0));
            Collector c;
            run (e, c, 300.0, sr, 512);
            int last = -1, msb = -1, maxJump = 0, count = 0;
            for (auto& ev : c.events)
            {
                if (ev.cc == kLfoMsbCc + 7) msb = ev.value;
                if (ev.cc == kLfoLsbCc + 7)
                {
                    const int v = msb * 128 + ev.value;
                    if (last >= 0) maxJump = std::max (maxJump, std::abs (v - last));
                    last = v; ++count;
                }
            }
            expect (count > 100, "slowest LFO 8 sends updates");
            expect (maxJump <= 1, "slowest LFO 8 never jumps more than one step (largest " + std::to_string (maxJump) + ")");
        }
        // Centre, every lane: no jump bigger than the LFO really moved since the last message, plus one step.
        Engine e; e.prepare (sr);
        Settings s; s.rate = 0.5; s.drift = 1; s.modSource = 0; s.exOn = false; e.setSettings (s);   // the eight LFOs alone
        Collector c;
        run (e, c, 60.0, sr, 0, 3);
        std::vector<int> msb (128, -1), last (kNumLanes, -1);
        std::vector<long long> lastSample (kNumLanes, -1);
        bool orderOk = true, capOk = true, jumpOk = true, changedOnly = true;
        int perSecondMax = 0;
        std::vector<int> perSecond (61, 0);
        for (size_t k = 0; k < c.events.size(); ++k)
        {
            const auto& ev = c.events[k];
            if (ev.value < 0 || ev.value > 127) orderOk = false;
            if (ev.cc >= kLfoMsbCc && ev.cc < kLfoMsbCc + kNumLanes)
            {
                // A coarse part must be followed straight away by the same lane's fine part, at the same sample.
                if (k + 1 >= c.events.size() || c.events[k + 1].cc != ev.cc + (kLfoLsbCc - kLfoMsbCc) || c.events[k + 1].sample != ev.sample)
                    orderOk = false;
                msb[(size_t) ev.cc] = ev.value;
            }
            else if (ev.cc >= kLfoLsbCc && ev.cc < kLfoLsbCc + kNumLanes)
            {
                const int lane = ev.cc - kLfoLsbCc;
                if (msb[(size_t) (kLfoMsbCc + lane)] < 0) orderOk = false;
                const int v = msb[(size_t) (kLfoMsbCc + lane)] * 128 + ev.value;
                if (lastSample[(size_t) lane] >= 0)
                {
                    const double dtSec = (double) (ev.sample - lastSample[(size_t) lane]) / sr;
                    if (dtSec < kMinIntervalSeconds - 1e-9) capOk = false;
                    const double maxMove = 4.0 * kCentreHz[lane] * std::exp2 (0.05) * dtSec * 16383.0 / 2.0;
                    if (std::abs (v - last[(size_t) lane]) > maxMove + 1.0) jumpOk = false;
                    if (v == last[(size_t) lane]) changedOnly = false;
                }
                last[(size_t) lane] = v;
                lastSample[(size_t) lane] = ev.sample;
                ++perSecond[(size_t) std::min<long long> (60, ev.sample / (long long) sr)];
            }
            else orderOk = false;
        }
        for (int x : perSecond) perSecondMax = std::max (perSecondMax, x);
        expect (orderOk, "coarse part always straight before its fine part, only CC 20-27 and 52-59 used");
        expect (capOk, "no lane sends more often than every 6 ms");
        expect (jumpOk, "no jump bigger than the LFO really moved (+1 step)");
        expect (changedOnly, "nothing sent when the value did not change");
        expect (perSecondMax <= kNumLanes * (int) (1.0 / kMinIntervalSeconds + 2), "message rate capped (" + std::to_string (perSecondMax) + " fine updates in the busiest second)");
        // The fine updates for the last value of each lane match the engine.
        bool match = true;
        for (int i = 0; i < kNumLanes; ++i) match = match && last[(size_t) i] == e.ccStream().lastSent (i);
        expect (match, "receiver's view equals what the engine last sent");
    }

    // ---- Ogdoad Ex ------------------------------------------------------------------------------------------------
    bool isLfoCc (int n) { return (n >= kLfoMsbCc && n < kLfoMsbCc + kNumLanes) || (n >= kLfoLsbCc && n < kLfoLsbCc + kNumLanes); }

    double mapped (int i, double v) { return kExBipolar[i] ? v : v * 2.0 - 1.0; }

    void testExValues()
    {
        Engine e; e.prepare (48000.0);
        Settings s = plain (0.5); s.drift = 1; e.setSettings (s);
        NullSink n;
        bool foldOk = true, maxMinOk = true, orderOk = true, stepsOk = true, rangeOk = true;
        std::vector<std::set<int>> levels (kNumSteps);
        int minima = 0, cycles = 0; double prevFold = -1, prevDiff = 0, prevPhase = e.lanePhase (0);
        for (int t = 0; t < (int) (200.0 * 48000.0 / Engine::kTickSamples); ++t)
        {
            e.render (Engine::kTickSamples, nullptr, 0, n);
            for (int k = 0; k < kNumFold; ++k)
                foldOk = foldOk && e.exValue (k) == std::fabs (e.laneValue (kFoldLfo[k]));
            const double mx23 = e.exValue (4), mn23 = e.exValue (5), mx67 = e.exValue (6), mn67 = e.exValue (7);
            maxMinOk = maxMinOk && mx23 == std::max (e.laneValue (1), e.laneValue (2)) && mn23 == std::min (e.laneValue (1), e.laneValue (2))
                                && mx67 == std::max (e.laneValue (5), e.laneValue (6)) && mn67 == std::min (e.laneValue (5), e.laneValue (6));
            orderOk = orderOk && mx23 >= mn23 && mx67 >= mn67;
            for (int k = 0; k < kNumSteps; ++k)
            {
                int level = 0;
                for (int b = 0; b < 4; ++b) level = level * 2 + (e.lanePhase (kStepsBits[k][b]) < 0.5 ? 1 : 0);
                const double v = e.exValue (kNumFold + kNumMaxMin + k);
                stepsOk = stepsOk && v == level / 15.0;
                levels[(size_t) k].insert ((int) std::lround (v * 15.0));
            }
            for (int i = 0; i < kNumExSmooth; ++i)
            {
                const double v = e.exValue (i);
                rangeOk = rangeOk && (kExBipolar[i] ? (v >= -1.0 && v <= 1.0) : (v >= 0.0 && v <= 1.0));
            }
            // Fold 1 runs at twice LFO 1's speed: count its low points against LFO 1's cycles.
            const double f = e.exValue (0), diff = f - prevFold;
            if (prevFold >= 0 && prevDiff < 0 && diff > 0) ++minima;
            if (prevFold >= 0) prevDiff = diff;
            prevFold = f;
            if (e.lanePhase (0) < prevPhase) ++cycles;
            prevPhase = e.lanePhase (0);
        }
        expect (foldOk, "fold x2 = LFO 1, 3, 5, 7 folded up");
        expect (std::abs (minima - 2 * cycles) <= 2, "fold x2 runs at twice the speed (" + std::to_string (minima) + " low points, " + std::to_string (cycles) + " LFO 1 cycles)");
        expect (maxMinOk, "max / min = the higher / lower of LFO 2 and 3, and of 6 and 7");
        expect (orderOk, "max is never below min");
        expect (stepsOk, "steps = four LFOs as bits (on while rising), weighted 8 4 2 1");
        expect (rangeOk, "fold and steps stay 0..1, max / min -1..+1");
        for (int k = 0; k < kNumSteps; ++k)
            expect (levels[(size_t) k].size() >= 14, "steps output " + std::to_string (k + 1) + " visits many of the 16 levels (" + std::to_string (levels[(size_t) k].size()) + ")");

        // The CC stream: every Ex output arrives as a 14-bit pair; after Hold the receiver has exactly the engine's value.
        Engine c; c.prepare (48000.0);
        Settings cs = plain (0.5); cs.drift = 1; c.setSettings (cs);
        Collector col;
        run (c, col, 30.0, 48000.0, 0, 9);
        cs.hold = true; c.setSettings (cs);
        run (c, col, 0.05, 48000.0, 512);
        std::set<int> used;
        std::vector<int> msb (128, -1), got (128, -1);
        bool pairOk = true;
        for (size_t k = 0; k < col.events.size(); ++k)
        {
            const auto& ev = col.events[k];
            used.insert (ev.cc);
            if ((ev.cc >= 12 && ev.cc <= 31) && (k + 1 >= col.events.size() || col.events[k + 1].cc != ev.cc + 32 || col.events[k + 1].sample != ev.sample))
                pairOk = false;
            if (ev.cc >= 12 && ev.cc <= 31) msb[(size_t) ev.cc] = ev.value;
            if (ev.cc >= 44 && ev.cc <= 63) got[(size_t) ev.cc] = msb[(size_t) (ev.cc - 32)] * 128 + ev.value;
        }
        std::set<int> expected;
        for (int i = 12; i <= 31; ++i) { expected.insert (i); expected.insert (i + 32); }
        for (int k = 0; k < kNumTrigs; ++k) expected.insert (kTrigCc[k]);
        expect (used == expected, "Ex on: CC 12-31, 44-63 and 102-105 used, nothing else");
        expect (pairOk, "Ex outputs: coarse part always straight before its fine part");
        bool exact = true;
        for (int i = 0; i < kNumExSmooth; ++i)
            exact = exact && got[(size_t) kExLsbCc[i]] == toFourteenBit (mapped (i, c.exValue (i)));
        expect (exact, "receiver's view of all twelve Ex CC outputs equals the engine's values");
        bool stepsExact = true;
        for (int k = 0; k < kNumSteps; ++k)
        {
            const double v = got[(size_t) kExLsbCc[kNumFold + kNumMaxMin + k]] / 16383.0;
            stepsExact = stepsExact && std::fabs (v * 15.0 - std::round (v * 15.0)) < 0.001;
        }
        expect (stepsExact, "steps arrive as clean sixteenths");

        // Ex off: only the eight LFOs are sent.
        Engine o; o.prepare (48000.0);
        Settings os = plain (0.5); os.exOn = false; os.trigMode = (int) TrigMode::both; o.setSettings (os);
        Collector oc;
        run (o, oc, 20.0, 48000.0, 512);
        bool onlyLfo = ! oc.events.empty() && oc.notes.empty();
        for (auto& ev : oc.events) onlyLfo = onlyLfo && isLfoCc (ev.cc);
        expect (onlyLfo, "Ex off: only the eight LFOs' CC, no triggers, no notes");

        // Message rate, LFOs alone and with Ex.
        auto rate = [] (bool ex)
        {
            Engine r; r.prepare (48000.0);
            Settings rs; rs.rate = 0.5; rs.drift = 1; rs.modSource = 8; rs.modAmount = 0.0; rs.exOn = ex; r.setSettings (rs);
            Collector rc;
            run (r, rc, 60.0, 48000.0, 512);
            return (double) (rc.events.size() + rc.notes.size()) / 60.0;
        };
        const double without = rate (false), with = rate (true);
        std::printf ("MIDI: %.0f messages a second (LFOs alone), %.0f with Ex\n", without, with);
        expect (with < 2.0 * without + 200.0, "Ex adds no more than its share of messages");
    }

    // Runs tick by tick, recording which LFOs passed their bottom at each tick (seen from outside the engine).
    struct TickLog { std::vector<std::array<bool, kNumLanes>> wraps; std::vector<std::array<uint64_t, kNumTrigs>> arrivals; };

    TickLog runTicks (Engine& e, Collector& c, double seconds)
    {
        TickLog log;
        std::array<double, kNumLanes> last {};
        for (int i = 0; i < kNumLanes; ++i) last[(size_t) i] = e.lanePhase (i);
        const int ticks = (int) (seconds * 48000.0 / Engine::kTickSamples);
        for (int t = 0; t < ticks; ++t)
        {
            e.render (Engine::kTickSamples, nullptr, 0, c);
            c.blockStart += Engine::kTickSamples;
            std::array<bool, kNumLanes> w {};
            for (int i = 0; i < kNumLanes; ++i) { w[(size_t) i] = e.lanePhase (i) < last[(size_t) i]; last[(size_t) i] = e.lanePhase (i); }
            log.wraps.push_back (w);
            std::array<uint64_t, kNumTrigs> a {};
            for (int k = 0; k < kNumTrigs; ++k) a[(size_t) k] = e.trigArrivals (k);
            log.arrivals.push_back (a);
        }
        return log;
    }

    void testTriggers()
    {
        const int pulseSamples = 960;          // 20 ms at 48 kHz (30 ticks of 32 samples)

        // All four in use, CC pulse.
        {
            Engine e; e.prepare (48000.0);
            Settings s = plain (0.5); s.drift = 1; e.setSettings (s);
            Collector c;
            auto log = runTicks (e, c, 300.0);
            std::array<int, kNumTrigs> wraps {};
            for (auto& w : log.wraps) for (int k = 0; k < kNumTrigs; ++k) wraps[(size_t) k] += w[(size_t) kTrigLfo[k]] ? 1 : 0;
            for (int k = 0; k < kNumTrigs; ++k)
            {
                const std::string out = "trig " + std::to_string (kTrigLfo[k] + 1);
                expect (wraps[(size_t) k] > 3, out + ": its LFO passed the bottom (" + std::to_string (wraps[(size_t) k]) + " times)");
                expect ((int) e.trigArrivals (k) == wraps[(size_t) k], out + ": one trigger each time its LFO starts rising");
                expect (e.trigPulses (k) == e.trigArrivals (k), out + ": every trigger sent (slow enough that none merge)");
                int ons = 0, offs = 0; long long lastOn = -1; bool alternate = true, length = true;
                for (auto& ev : c.events)
                {
                    if (ev.cc != kTrigCc[k]) continue;
                    if (ev.value == 127) { alternate = alternate && lastOn < 0; lastOn = ev.sample; ++ons; }
                    else if (ev.value == 0) { alternate = alternate && lastOn >= 0; length = length && ev.sample - lastOn == pulseSamples; lastOn = -1; ++offs; }
                    else alternate = false;
                }
                expect (ons == wraps[(size_t) k] && offs >= ons - 1, out + ": CC " + std::to_string (kTrigCc[k]) + " pulses 127 then 0");
                expect (alternate, out + ": on and off alternate");
                expect (length, out + ": each pulse lasts exactly 20 ms");
            }
            expect (c.notes.empty(), "CC pulse mode: no notes");
        }

        // The pulse starts at the bottom of the triangle.
        {
            Engine e; e.prepare (48000.0);
            e.setSettings (plain (0.5));
            NullSink n;
            bool atBottom = true; uint64_t seen = 0;
            for (int t = 0; t < (int) (30.0 * 48000.0 / Engine::kTickSamples); ++t)
            {
                e.render (Engine::kTickSamples, nullptr, 0, n);
                if (e.trigPulses (0) != seen)
                {
                    seen = e.trigPulses (0);
                    atBottom = atBottom && e.laneValue (1) < -1.0 + 4.0 * kCentreHz[1] * Engine::kTickSamples / 48000.0 + 1e-9 && e.lanePhase (1) < 0.5;
                }
            }
            expect (seen > 10 && atBottom, "a trigger fires where its LFO turns upward at the bottom");
        }

        // The chain: outputs switched off pass their triggers on.
        for (int pattern = 0; pattern < 16; ++pattern)
        {
            Engine e; e.prepare (48000.0);
            Settings s = plain (0.62); s.drift = 2;
            for (int k = 0; k < kNumTrigs; ++k) s.trigUse[k] = (pattern >> k) & 1;
            e.setSettings (s);
            Collector c;
            auto log = runTicks (e, c, 120.0);
            std::array<uint64_t, kNumTrigs> expected {};
            for (auto& w : log.wraps)
            {
                bool carried = false;
                for (int k = 0; k < kNumTrigs; ++k)
                {
                    const bool arrive = w[(size_t) kTrigLfo[k]] || carried;
                    if (arrive) ++expected[(size_t) k];
                    carried = arrive && ! s.trigUse[k];
                }
            }
            bool arrivalsOk = true, silentOk = true, sentOk = true;
            for (int k = 0; k < kNumTrigs; ++k)
            {
                arrivalsOk = arrivalsOk && e.trigArrivals (k) == expected[(size_t) k];
                int ons = 0;
                for (auto& ev : c.events) if (ev.cc == kTrigCc[k] && ev.value == 127) ++ons;
                if (! s.trigUse[k]) silentOk = silentOk && ons == 0 && e.trigPulses (k) == 0;
                else sentOk = sentOk && ons == (int) e.trigPulses (k) && ons > 0 && e.trigPulses (k) <= e.trigArrivals (k);
            }
            const std::string name = "in use pattern " + std::to_string (pattern & 1) + std::to_string ((pattern >> 1) & 1)
                                   + std::to_string ((pattern >> 2) & 1) + std::to_string ((pattern >> 3) & 1);
            expect (arrivalsOk, name + ": triggers reach the right outputs");
            expect (silentOk, name + ": outputs switched off send nothing");
            expect (sentOk, name + ": outputs in use send what reaches them");
        }
        // All into trig 8: it carries the triggers of all four LFOs.
        {
            Engine e; e.prepare (48000.0);
            Settings s = plain (0.5); s.trigUse[0] = s.trigUse[1] = s.trigUse[2] = false; e.setSettings (s);
            Collector c;
            auto log = runTicks (e, c, 60.0);
            int any = 0;
            for (auto& w : log.wraps) any += (w[1] || w[3] || w[5] || w[7]) ? 1 : 0;
            expect ((int) e.trigArrivals (3) == any && any > 150, "trig 8 alone gathers all four LFOs' triggers (" + std::to_string (any) + ")");
        }

        // Fast: triggers closer than 20 ms merge into one longer pulse; it is never stuck on.
        {
            Engine e; e.prepare (48000.0);
            Settings s = plain (1.0); e.setSettings (s);       // LFO 2 at about 85 Hz
            Collector c;
            runTicks (e, c, 2.0);
            expect (e.trigPulses (0) < e.trigArrivals (0), "fast triggers merge");
            s.hold = true; e.setSettings (s);
            runTicks (e, c, 0.1);
            expect (! e.trigOn (0) && c.events.back().cc != 127, "after stopping, the merged pulse ends");
        }

        // Notes and both.
        for (int mode : { (int) TrigMode::note, (int) TrigMode::both })
        {
            Engine e; e.prepare (48000.0);
            Settings s = plain (0.5); s.trigMode = mode; e.setSettings (s);
            Collector c;
            runTicks (e, c, 60.0);
            s.hold = true; e.setSettings (s);
            runTicks (e, c, 0.1);
            const std::string m = mode == 1 ? "Note mode" : "Both mode";
            int ccPulses = 0;
            for (auto& ev : c.events) if (ev.cc >= 102 && ev.cc <= 105 && ev.value == 127) ++ccPulses;
            std::array<long long, 128> onAt {}; onAt.fill (-1);
            bool pairs = true, numbers = true, vel = true; int ons = 0;
            for (auto& nv : c.notes)
            {
                numbers = numbers && nv.cc >= 36 && nv.cc <= 39;
                if (nv.value > 0) { pairs = pairs && onAt[(size_t) nv.cc] < 0; onAt[(size_t) nv.cc] = nv.sample; vel = vel && nv.value == 100; ++ons; }
                else { pairs = pairs && onAt[(size_t) nv.cc] >= 0 && nv.sample - onAt[(size_t) nv.cc] == pulseSamples; onAt[(size_t) nv.cc] = -1; }
            }
            bool noneHanging = true;
            for (auto x : onAt) noneHanging = noneHanging && x < 0;
            uint64_t total = 0; for (int k = 0; k < kNumTrigs; ++k) total += e.trigPulses (k);
            expect (ons == (int) total && ons > 50, m + ": one note per trigger (" + std::to_string (ons) + ")");
            expect (numbers && vel, m + ": notes 36-39 (C1 to D#1), velocity 100");
            expect (pairs, m + ": each note ends after exactly 20 ms");
            expect (noneHanging, m + ": no note left hanging");
            expect (mode == 1 ? ccPulses == 0 : ccPulses == ons, m + (mode == 1 ? ": no CC pulses" : ": a CC pulse with every note"));
        }

        // Changing the mode, switching off, Ex off, bypass or a reset during a pulse: the pulse always ends cleanly.
        auto startPulse = [] (Engine& e, Collector& c, Settings s)
        {
            e.prepare (48000.0); e.setSettings (s);
            for (int t = 0; t < 400000 && ! e.trigOn (0); ++t) { e.render (Engine::kTickSamples, nullptr, 0, c); c.blockStart += Engine::kTickSamples; }
            return e.trigOn (0);
        };
        auto ended = [] (const Collector& c, size_t fromCc, size_t fromNote, bool wantCc, bool wantNote)
        {
            bool cc0 = false, n0 = false;
            for (size_t i = fromCc; i < c.events.size(); ++i) if (c.events[i].cc == 102 && c.events[i].value == 0) cc0 = true;
            for (size_t i = fromNote; i < c.notes.size(); ++i) if (c.notes[i].cc == 36 && c.notes[i].value == 0) n0 = true;
            return cc0 == wantCc && n0 == wantNote;
        };
        {
            Engine e; Collector c; Settings s = plain (0.5); s.trigMode = 2;
            expect (startPulse (e, c, s), "a pulse starts");
            const size_t a = c.events.size(), b = c.notes.size();
            s.trigMode = 0; e.setSettings (s);
            runTicks (e, c, 0.05);
            expect (ended (c, a, b, true, true), "mode changed during a pulse: both the CC and the note still end");
        }
        {
            Engine e; Collector c; Settings s = plain (0.5); s.trigMode = 2;
            startPulse (e, c, s);
            const size_t a = c.events.size(), b = c.notes.size();
            s.trigUse[0] = false; s.exOn = false; e.setSettings (s);
            runTicks (e, c, 0.05);
            expect (ended (c, a, b, true, true), "switched off or Ex off during a pulse: it still ends");
        }
        {
            Engine e; Collector c; Settings s = plain (0.5); s.trigMode = 1;
            startPulse (e, c, s);
            const size_t a = c.events.size(), b = c.notes.size();
            e.closeTriggers (0, c);
            expect (ended (c, a, b, false, true) && ! e.trigOn (0), "bypass: the note ends at once");
        }
        {
            Engine e; Collector c; Settings s = plain (0.5); s.trigMode = 1;
            startPulse (e, c, s);
            const size_t a = c.events.size(), b = c.notes.size();
            e.reset();
            e.render (Engine::kTickSamples, nullptr, 0, c);
            expect (ended (c, a, b, false, true), "reset (e.g. new sample rate) during a note: it ends at the next tick");
        }

        // Hold: no triggers while held.
        {
            Engine e; e.prepare (48000.0);
            Settings s = plain (0.5); s.hold = true; e.setSettings (s);
            Collector c;
            runTicks (e, c, 20.0);
            uint64_t total = 0; for (int k = 0; k < kNumTrigs; ++k) total += e.trigArrivals (k);
            bool noPulse = true;
            for (auto& ev : c.events) noPulse = noPulse && (ev.cc < 102 || ev.cc > 105);
            expect (total == 0 && noPulse && c.notes.empty(), "held: no triggers");
        }

        // Pulse length at 44.1 kHz.
        {
            Engine e; e.prepare (44100.0); e.setSettings (plain (0.5));
            Collector c;
            run (e, c, 5.0, 44100.0, 512);
            long long on = -1; double ms = 0;
            for (auto& ev : c.events)
                if (ev.cc == 102) { if (ev.value == 127) on = ev.sample; else if (on >= 0) { ms = (ev.sample - on) / 44.1; break; } }
            expect (ms > 19.5 && ms < 21.0, "pulse about 20 ms at 44.1 kHz (" + std::to_string (ms) + " ms)");
        }
    }

    void testCpu()
    {
        Engine e; e.prepare (96000.0);
        Settings s; s.rate = 1.0; s.modSource = 1; s.modAmount = 0.7; s.drift = 2; e.setSettings (s);
        NullSink n;
        const double seconds = 120.0;
        const auto t0 = std::chrono::steady_clock::now();
        for (long b = 0; b < (long) (seconds * 96000.0 / 64.0); ++b) e.render (64, nullptr, 0, n);
        const double used = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double percent = used / seconds * 100.0;
        std::printf ("CPU: %.4f%% of one core at 96 kHz, 64-sample blocks, worst-case settings\n", percent);
        expect (percent < 1.0, "CPU under 1% of one core (measured " + std::to_string (percent) + "%)");
    }
}

int main()
{
    testRateLaw();
    testTriangle();
    testHold();
    testModulation();
    testDrift();
    testSampleRates();
    testBlockSizes();
    testCcStream();
    testExValues();
    testTriggers();
    testCpu();
    if (failures == 0) std::printf ("engine tests: %d checks passed\n", checks);
    else std::printf ("engine tests: %d of %d checks FAILED\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
