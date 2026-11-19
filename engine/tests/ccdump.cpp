// Prints the engine's real CC stream (one line per message: "sample cc value"), for the mapper test.
// Usage: ogdoad_ccdump [seconds]

#include "ogdoad/Engine.h"

#include <cstdio>
#include <cstdlib>

using namespace ogdoad;

struct Printer : CcSink
{
    long long blockStart = 0;
    void cc (int offset, int number, int v) override { std::printf ("%lld %d %d\n", blockStart + offset, number, v); }
};

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 30.0;
    Engine e; e.prepare (48000.0);
    Settings s; s.rate = 0.55; s.modSource = 8; s.modAmount = 0.3; s.drift = 1;
    e.setSettings (s);
    Printer p;
    long long total = (long long) (seconds * 48000.0);
    while (total > 0)
    {
        const int n = total > 512 ? 512 : (int) total;
        e.render (n, nullptr, 0, p);
        p.blockStart += n;
        total -= n;
    }
    // Final state, for the test to compare with: "final lane value14".
    for (int i = 0; i < kNumLanes; ++i) std::printf ("final %d %d\n", i, e.ccStream().lastSent (i));
    return 0;
}
