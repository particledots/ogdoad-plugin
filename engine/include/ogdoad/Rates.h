// How fast each of the eight LFOs runs.
//
// At the centre of the Rate knob the eight speeds are fixed, about 1.9 times slower each step down, with uneven
// steps so they never lock together: LFO 1 about 4.9 Hz (vibrato rate) down to LFO 8 about one cycle in 19 seconds.
// Turning Rate moves all eight together, in octaves: fully clockwise puts LFO 1 at 160 Hz, fully anticlockwise puts
// LFO 8 at one cycle in 25 minutes. The knob is linear in octaves on each side of the centre.
//
// The control value can go below 0 or above 1 when Mod is used: below 0 every LFO stalls (track and hold),
// above 1 they keep speeding up (capped at 1.25).

#pragma once

#include <cmath>

namespace ogdoad
{
    inline constexpr int kNumLanes = 8;

    inline constexpr double kCentreHz[kNumLanes] = {
        4.9, 2.6, 1.4, 0.72, 1.0 / 2.6, 1.0 / 5.1, 1.0 / 9.8, 1.0 / 19.0
    };

    inline constexpr double kTopHz       = 160.0;        // LFO 1, Rate fully clockwise
    inline constexpr double kBottomCycle = 25.0 * 60.0;  // LFO 8 period in seconds, Rate fully anticlockwise
    inline constexpr double kMaxControl  = 1.25;

    // Octaves to shift all eight from their centre speeds, for a control value (0..1 is the knob's travel).
    inline double controlToOctaves (double c)
    {
        static const double up   = std::log2 (kTopHz / kCentreHz[0]);
        static const double down = std::log2 (kBottomCycle * kCentreHz[kNumLanes - 1]);
        if (c > kMaxControl) c = kMaxControl;
        return c >= 0.5 ? (c - 0.5) * 2.0 * up : (c - 0.5) * 2.0 * down;
    }

    // Frequency of lane i (0..7) for a control value, before drift. Zero means stalled.
    inline double laneFrequency (int lane, double control, double driftOct = 0.0)
    {
        if (control < 0.0) return 0.0;
        return kCentreHz[lane] * std::exp2 (controlToOctaves (control) + driftOct);
    }
}
