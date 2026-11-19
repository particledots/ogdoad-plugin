// Ogdoad Ex: the sixteen expander outputs, worked out from the eight LFOs.
//
//   fold x2    LFO 1, 3, 5, 7 folded up (the part below the centre turned upside down): 0..1 at twice the speed
//   max / min  the higher (max) and the lower (min) of LFO 2 and 3, and of LFO 6 and 7: -1..+1
//   steps      four LFOs as on/off bits (on while rising) weighted 8, 4, 2, 1: sixteen levels, 0..1
//                1-4: LFO 1 (8) .. LFO 4 (1)   5-8: LFO 5 .. LFO 8   odd: 1, 3, 5, 7   even: 2, 4, 6, 8
//   trig chain a trigger each time LFO 2, 4, 6 or 8 starts rising (the bottom of its triangle). An output switched
//              off ("not in use") sends nothing and passes its triggers on to the next: 2 -> 4 -> 6 -> 8.
//
// The smooth and stepped outputs go out as 14-bit CC pairs like the LFOs; the triggers as a 7-bit CC pulse
// (127, then 0 after 20 ms) and/or a short note. All numbers here were chosen once (milestone 2) and never change.

#pragma once

namespace ogdoad
{
    inline constexpr int kNumFold = 4, kNumMaxMin = 4, kNumSteps = 4, kNumTrigs = 4;
    inline constexpr int kNumExSmooth = kNumFold + kNumMaxMin + kNumSteps;     // 12 outputs sent as 14-bit CC

    // Ex output index 0..11 (fold 1,3,5,7; max 2.3, min 2.3, max 6.7, min 6.7; steps 1-4, 5-8, odd, even)
    inline constexpr int kExMsbCc[kNumExSmooth] = { 12, 13, 14, 15, 16, 17, 18, 19, 28, 29, 30, 31 };
    inline constexpr int kExLsbCc[kNumExSmooth] = { 44, 45, 46, 47, 48, 49, 50, 51, 60, 61, 62, 63 };
    inline constexpr bool kExBipolar[kNumExSmooth] = { false, false, false, false, true, true, true, true, false, false, false, false };

    // Which LFO (0-based) each fold output uses, and each max/min pair.
    inline constexpr int kFoldLfo[kNumFold] = { 0, 2, 4, 6 };
    inline constexpr int kMaxMinA[kNumMaxMin] = { 1, 1, 5, 5 };
    inline constexpr int kMaxMinB[kNumMaxMin] = { 2, 2, 6, 6 };
    inline constexpr bool kMaxMinIsMax[kNumMaxMin] = { true, false, true, false };

    // Steps: the four LFOs (0-based), most significant bit first.
    inline constexpr int kStepsBits[kNumSteps][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 0, 2, 4, 6 }, { 1, 3, 5, 7 } };

    // Trig chain: output k is fed by LFO kTrigLfo[k]; CC pulse number and note number.
    inline constexpr int kTrigLfo[kNumTrigs]  = { 1, 3, 5, 7 };
    inline constexpr int kTrigCc[kNumTrigs]   = { 102, 103, 104, 105 };
    inline constexpr int kTrigNote[kNumTrigs] = { 36, 37, 38, 39 };       // C1, C#1, D1, D#1 in Logic's naming
    inline constexpr int kTrigVelocity = 100;
    inline constexpr double kTrigSeconds = 0.020;

    enum class TrigMode : int { ccPulse = 0, note = 1, both = 2 };
}
