# Changelog

## 0.2.2 (resizable window; tested in Logic by Bryn on 2026-11-19: working)

- The window is resizable: drag the corner at the bottom right. It scales from 80% to 200% of the normal size
  (624 x 376 to 1560 x 940) with its proportions locked; knobs, lights, strips, traces and text all grow together.
- The window width is saved with the project (not an audio parameter); a project with no size saved, such as one from
  0.2.1 or earlier, opens at the normal 780 x 470.
- Nothing else changes: the look at the normal size, the engine, every parameter id, CC and note number, the mapper and
  its Source menu order are as in 0.2.1.

## 0.2.1 (window update; tested in Logic by Bryn on 2026-11-19: working)

- Brighter, bigger lights: the eight LFO lights and the sixteen Ogdoad Ex lights are larger, have a soft halo, and shine
  clearly already at 30% of their range. The four trig lights now fade out over about a quarter of a second instead of cutting off.
- Lights | Waves button under the eight lanes. *Lights* (default) shows a light strip beside each light; *Waves* shows the
  moving traces as in 0.2.0. The choice is saved with the project (a project saved with 0.2.0 opens on Lights).
- Nothing else changes: the engine, every parameter id, CC and note number, the mapper and its Source menu order are as in 0.2.0.

## 0.2.0 (milestone 2: Ogdoad Ex; tested in Logic on both Macs on 2026-11-19: working)

- Ogdoad Ex comes alive in the same window: sixteen outputs, each with a light and its CC number.
  - fold ×2 (LFO 1, 3, 5, 7) on CC 12-15 / 44-47; max and min of LFO 2 and 3 and of 6 and 7 on CC 16-19 / 48-51;
    steps (1-4, 5-8, odd, even) on CC 28-31 / 60-63; all 14-bit like the LFOs.
  - trig chain: a trigger each time LFO 2, 4, 6 or 8 starts rising; an "in use" switch under each (ids
    `trigUse1`-`trigUse4`): off = silent, triggers pass on to the next (2 → 4 → 6 → 8).
  - Trig out (id `trigMode`): CC pulse (CC 102-105, 127 for 20 ms), Note (36-39, C1 to D♯1, velocity 100, 20 ms) or Both.
  - On switch (id `exOn`): Ex off sends exactly what 0.1.0 sent.
- A trigger pulse or note always ends, also when the mode, the channel or Ex On is changed during it, on bypass and on reset.
- Ogdoad Mapper 0.2.0: the Source menu adds Fold 1/3/5/7, Max/Min 2+3 and 6+7, Trig 2/4/6/8 and Steps 1-4/5-8/odd/even
  after the 0.1.0 entries; a Trig lane jumps from Min to Max for 20 ms; trigger notes pass through.
- README, CHANGELOG and INSTALL.txt finished for 0.2.0. Projects saved with 0.1.0 load with their settings and Ex at its defaults.

## 0.1.0 (milestone 1, tested in Logic on 2026-11-19: working)

- First build: AU + VST3 MIDI effect, universal (Apple Silicon + Intel), macOS 11 or newer, ad-hoc signed.
- Eight free-running triangle LFOs sent as 14-bit CC (CC 20-27 coarse, 52-59 fine) on channel 16 (changeable).
- Rate, Mod, Mod source (Off, LFO 1-8, Mod wheel), Hold, Pedal (sustain pedal holds), Drift (Off, Low, More).
- The panel as approved in the draft; the Ogdoad Ex section shown dimmed until milestone 2.
- The Ogdoad Mapper Scripter script (8 lanes: source, Learn target, min, max).
- Package: the plugin, `Install-Unofficial-Plugin.command` (unchanged, Audio Unit only), `INSTALL.txt`, the mapper, auval and pluginval logs.
