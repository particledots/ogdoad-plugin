// Engine self-test. Runs on the GitHub build (Linux) before the Mac build, and prints one summary line
// "engine tests: N checks passed" or the failures.

#include "ogdoad/Engine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
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
        std::vector<Event> events;
        void cc (int offset, int number, int v) override { events.push_back ({ blockStart + offset, number, v }); }
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
        run (e, c, 0.01, 48000.0, 256);            // one tick to settle the last value
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
            e.setSettings (s);
            Collector c;
            run (e, c, 20.0, 48000.0, block, seed);
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
        Settings s; s.rate = 0.5; s.drift = 1; s.modSource = 0; e.setSettings (s);
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
    testCpu();
    if (failures == 0) std::printf ("engine tests: %d checks passed\n", checks);
    else std::printf ("engine tests: %d of %d checks FAILED\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
