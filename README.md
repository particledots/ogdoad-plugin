# Ogdoad

An original AU / VST3 MIDI effect for Logic Pro: eight free-running triangle LFOs, fastest at the top and slowest at
the bottom, plus sixteen more outputs worked out from them (Ogdoad Ex), all sent out as smooth MIDI CC. Inspired by the
feel of an eight-LFO Eurorack module and its expander. All code is our own; it is not a port or copy of anything, and
it is not affiliated with any module maker.

Status: **version 0.2.2 (resizable window, with the brighter lights and Lights | Waves button of 0.2.1) tested in Logic by Bryn on
2026-11-19 and working.** Version 0.2.0 (milestone 2: Ogdoad Ex) was tested in Logic on both Macs on 2026-11-19 and works.
Versions 0.2.1 and 0.2.2 change only the window, not one MIDI message.

- **Rate** moves all eight together. At the centre LFO 1 runs at about 4.9 Hz and LFO 8 at one cycle in 19 s; each LFO
  is about 1.9 times slower than the one above, in uneven steps so they never lock together. Fully clockwise LFO 1 runs
  at 160 Hz; fully anticlockwise LFO 8 takes 25 minutes for one cycle.
- **Mod** (−/+) and **Mod source**: what bends the speed. *LFO 1* to *LFO 8* is the self-patch (an LFO feeding the
  rate, which bends the shapes of all eight); *Mod wheel* uses the incoming mod wheel (CC 1). Turned far enough
  negative, the source stalls all eight (track and hold). *Off* = no mod.
- **Hold** freezes all eight; **Pedal** makes the sustain pedal (CC 64) hold them too, while it is down.
- **Drift** (Off / Low / More): a slow random wander of each LFO's speed, so the eight never repeat exactly.
  Low = up to 0.05 octave, More = up to 0.18 octave.
- **Lights | Waves** (under the eight lanes): *Lights* (the default) shows each LFO as a bright round light plus a light strip
  of the same colour and brightness; *Waves* shows the last two seconds of each LFO as a moving trace instead. The choice is
  saved with the project. It is not an audio parameter and changes nothing that is sent.
- **Resizing**: drag the corner at the bottom right. The window scales from 80% to 200% of its normal size (624 x 376 to
  1560 x 940) with its proportions locked, and everything grows together. The size is saved with the project; a project
  with no size saved opens at the normal 780 x 470.
- **MIDI out channel** (bottom left, default 16). Your own notes and every other message pass through untouched.

## Ogdoad Ex

Sixteen more outputs, all following the eight LFOs (so Rate, Mod, Hold and Drift move them too). Each has a light and
its CC number on the panel.

- **fold ×2** (LFO 1, 3, 5, 7): the LFO with its lower half turned upside down: always positive, twice as fast.
- **max ∨ / min ∧**: the higher (max) and the lower (min) of LFO 2 and 3, and of LFO 6 and 7.
- **steps** (1–4, 5–8, odd, even): four LFOs read as on/off (on while rising) and added up with weights 8, 4, 2, 1,
  giving sixteen levels that change in an irregular, random-sounding pattern ("slow noise"). The first LFO named is
  the biggest step. 1–4 = LFO 1 to 4, 5–8 = LFO 5 to 8, odd = 1, 3, 5, 7, even = 2, 4, 6, 8.
- **trig chain** (2, 4, 6, 8): a trigger each time LFO 2, 4, 6 or 8 starts rising (the bottom of its triangle).
  Under each output, **in use**: switched off, that output is silent and its triggers pass on to the next one,
  clockwise 2 → 4 → 6 → 8. Switch off 2, 4 and 6 and output 8 carries the triggers of all four LFOs.
- **Trig out**: *CC pulse* (the trig CC goes to 127 for 20 ms, then back to 0), *Note* (a 20 ms note: C1, C♯1, D1, D♯1
  = notes 36 to 39, velocity 100, on the MIDI out channel; it passes the mapper and plays the track's instrument,
  for example Drum Machine Designer's kick, rim, snare and clap), or *Both*. Triggers closer together than 20 ms
  merge into one longer pulse.
- **On** (top right of the section): sends the sixteen Ex outputs. Off: only the eight LFOs are sent, exactly as in
  0.1.0, which halves the MIDI on the track.

## How the LFOs reach a knob in Logic

Logic has no CV. Each LFO goes out as a standard 14-bit CC pair (16,384 steps instead of 128):

| LFO | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| coarse CC | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 |
| fine CC | 52 | 53 | 54 | 55 | 56 | 57 | 58 | 59 |

Ogdoad Ex, same channel:

| Output | fold 1 | fold 3 | fold 5 | fold 7 | max 2∨3 | min 2∧3 | max 6∨7 | min 6∧7 | steps 1–4 | steps 5–8 | steps odd | steps even |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| coarse CC | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 28 | 29 | 30 | 31 |
| fine CC | 44 | 45 | 46 | 47 | 48 | 49 | 50 | 51 | 60 | 61 | 62 | 63 |

| Trigger | trig 2 | trig 4 | trig 6 | trig 8 |
|---|---|---|---|---|
| CC pulse (127 for 20 ms, then 0) | 102 | 103 | 104 | 105 |
| Note (velocity 100, 20 ms) | 36 (C1) | 37 (C♯1) | 38 (D1) | 39 (D♯1) |

The **Ogdoad Mapper** (a Scripter script, in the download as `Ogdoad Mapper (paste into Scripter).txt`, source in
`tools/mapper/Ogdoad-Mapper.js`) sits in the MIDI FX slot below Ogdoad. It has 8 lanes; each lane has a Source
(LFO 1 to 8, Fold 1/3/5/7, Max 2+3, Min 2+3, Max 6+7, Min 6+7, Trig 2/4/6/8, Steps 1-4/5-8/odd/even), a Target
(Logic's *Learn plug-in parameter*), and Min % / Max %. It moves the learnt knob on any plugin below it on the same
channel strip, and stops Ogdoad's CC from reaching the instrument; notes (including trigger notes) pass through.
A Trig source jumps the knob from Min to Max for 20 ms. Set up once per Mac (saved as a Scripter preset), then chosen
from Scripter's preset menu on each track. `INSTALL.txt` in the download has the steps. Version 0.2.0 of the mapper
keeps the 0.1.0 sources in the same places in the menu, so after updating, a saved lane still points at the same LFO.

Smoothness and CPU: each output is sent only when its value changes, at most once every 6 ms (about one Logic audio
block), coarse part only when it changes. Messages are placed at their exact sample inside each block. Above roughly
30 to 50 Hz an LFO is faster than MIDI CC can follow and turns into a slower, gritty wobble; the knob still goes there.
The engine uses well under 0.1% of one core. At the centre of Rate Ogdoad sends about 2,200 MIDI messages a second
with Ex off and about 4,600 with Ex on; the Ex **On** switch is there for tracks that do not use Ex.

## Folders

| Folder | What it is |
|---|---|
| `engine/` | The LFOs, the rate law, drift, hold, the Ogdoad Ex outputs (`ExOutputs.h`) and the CC stream (`CcStream`). Plain C++, no plugin code; reusable for the later modulation plugins. |
| `plugin/` | The plugin wrapper: what Logic sees, the list of controls, and the panel (`PluginEditor`). `plugin/preview/` draws the window to pictures and checks the plugin without Logic. |
| `tools/mapper/` | The Scripter mapper script. |
| `tools/install/` | `Install-Unofficial-Plugin.command`, the same installer as the other particledots plugins (Audio Unit only). |
| `.github/workflows/` | The automatic build. |

## Getting the plugin

1. Open the **Actions** tab of this repo and click the latest **Build Ogdoad** run.
2. At the bottom, under **Artifacts**, download **Ogdoad-macOS** and unzip it (twice: the download is a zip containing `Ogdoad-macOS.zip`).
3. Inside the `Ogdoad-macOS` folder are `Ogdoad.component`, `Ogdoad.vst3`, `INSTALL.txt`, `auval.log`,
   `pluginval-AU.log`, `pluginval-VST3.log`, `Ogdoad Mapper (paste into Scripter).txt` and `Install-Unofficial-Plugin.command`.
4. In Terminal, once, clear the download block: `xattr -dr com.apple.quarantine ~/Downloads/Ogdoad-macOS/*`
   (adjust the path if you unzipped elsewhere).
5. **Easy install (Logic, Audio Unit):** double-click `Install-Unofficial-Plugin.command`. It asks for your Mac
   password once, installs the AU for all users and restarts the AU registrar.
   Or by hand: copy `Ogdoad.component` to `~/Library/Audio/Plug-Ins/Components/` and `Ogdoad.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
6. In Logic it is a MIDI effect: MIDI FX slot of a Software Instrument track, **Audio Units > particledots > Ogdoad**.

## What the build checks

- **Engine self-test** (Linux, before the Mac build): speeds at the three Rate positions, the triangle shape, Hold and
  the pedal, the mod wheel stall, every self-patch at every amount staying stable, Drift, the same movement at
  44.1 / 48 / 96 kHz, an identical MIDI stream at any block size, the CC rules (slowest LFO never jumps more than one
  step, the 6 ms cap, coarse before fine, nothing sent when nothing changes) and the CPU use.
  Ogdoad Ex: each output against its definition at every tick (fold, max / min, steps), fold at twice the speed,
  the receiver ending up with exactly the engine's values, one trigger per LFO cycle at the bottom of the triangle,
  all 16 combinations of the in-use switches, merging at high speed, CC pulses and notes exactly 20 ms long, no
  note ever left hanging (mode change, switching off, Ex off, bypass, reset), Ex off sending exactly the 0.1.0 stream.
- **Mapper test**: the real mapper script run in a stand-in for Scripter, fed by the engine's real MIDI stream: values,
  Min/Max, upside-down lanes, 14-bit detail, every kind of Ex source (fold, min, trig, steps), trigger notes passing
  in Note and Both modes, notes passing, all of Ogdoad's CC stopped, the channel setting, the 0.1.0 menu order kept.
- **Mac build**: universal binary (Apple Silicon + Intel), ad-hoc signing, Apple's `auval` (as a MIDI effect,
  `aumi Ogd1 Pdot`), pluginval at strictness 5 on the AU and the VST3, the package contents.
- **Window picture and plugin checks**: the real window drawn to pictures (artifact **Ogdoad-window-pictures**: default,
  running, held, Waves view, small / medium / large size, Ex with trig 4 off in Note mode, Ex off), every control moving only its own parameter, save and
  reload (including the Lights | Waves choice), a 0.1.0 project loading with Ex at its defaults, a 0.2.0 project opening on Lights at the normal size, the window size limits and locked proportions, every control inside the window at 80%, 150% and 200%, the saved size coming back, MIDI passing through, CC on the chosen channel, mod wheel,
  pedal, bypass, block sizes, and Ogdoad Ex in the real plugin (the CC numbers used, pulses, notes, both, the chain,
  Ex off, bypass during a trigger note).

## Fixed names

Plugin code `Ogd1`, manufacturer `Pdot`, bundle id `com.particledots.ogdoad`, target `Ogdoad`, engine library
`ogdoad_engine`. Parameter ids: `rate`, `modAmount`, `modSource`, `hold`, `holdPedal`, `drift`, `outChannel` (0.1.0);
`exOn`, `trigUse1` to `trigUse4` (trig 2, 4, 6, 8), `trigMode` (CC pulse, Note, Both) (0.2.0). The CC and note
numbers in the tables above and the mapper's Source menu order are fixed too.
These never change, so saved projects keep finding the plugin and its settings.
