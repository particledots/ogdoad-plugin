# Ogdoad

An original AU / VST3 MIDI effect for Logic Pro: eight free-running triangle LFOs, fastest at the top and slowest at
the bottom, sent out as smooth MIDI CC. Inspired by the feel of an eight-LFO Eurorack module and its expander.
All code is our own; it is not a port or copy of anything, and it is not affiliated with any module maker.

Status: **milestone 1 (version 0.1.0, waiting for a test in Logic).**

- **Rate** moves all eight together. At the centre LFO 1 runs at about 4.9 Hz and LFO 8 at one cycle in 19 s; each LFO
  is about 1.9 times slower than the one above, in uneven steps so they never lock together. Fully clockwise LFO 1 runs
  at 160 Hz; fully anticlockwise LFO 8 takes 25 minutes for one cycle.
- **Mod** (−/+) and **Mod source**: what bends the speed. *LFO 1* to *LFO 8* is the self-patch (an LFO feeding the
  rate, which bends the shapes of all eight); *Mod wheel* uses the incoming mod wheel (CC 1). Turned far enough
  negative, the source stalls all eight (track and hold). *Off* = no mod.
- **Hold** freezes all eight; **Pedal** makes the sustain pedal (CC 64) hold them too, while it is down.
- **Drift** (Off / Low / More): a slow random wander of each LFO's speed, so the eight never repeat exactly.
  Low = up to 0.05 octave, More = up to 0.18 octave.
- **MIDI out channel** (bottom left, default 16). Your own notes and every other message pass through untouched.
- The **Ogdoad Ex** section is shown dimmed; it arrives in milestone 2.

## How the LFOs reach a knob in Logic

Logic has no CV. Each LFO goes out as a standard 14-bit CC pair (16,384 steps instead of 128):

| LFO | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| coarse CC | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 |
| fine CC | 52 | 53 | 54 | 55 | 56 | 57 | 58 | 59 |

The **Ogdoad Mapper** (a Scripter script, in the download as `Ogdoad Mapper (paste into Scripter).txt`, source in
`tools/mapper/Ogdoad-Mapper.js`) sits in the MIDI FX slot below Ogdoad. It has 8 lanes; each lane has a Source
(LFO 1 to 8), a Target (Logic's *Learn plug-in parameter*), and Min % / Max %. It moves the learnt knob on any plugin
below it on the same channel strip, and stops Ogdoad's CC from reaching the instrument. Set up once per Mac (saved as
a Scripter preset), then chosen from Scripter's preset menu on each track. `INSTALL.txt` in the download has the steps.

Smoothness and CPU: each LFO is sent only when its value changes, at most once every 6 ms (about one Logic audio
block), coarse part only when it changes. Messages are placed at their exact sample inside each block. Above roughly
30 to 50 Hz an LFO is faster than MIDI CC can follow and turns into a slower, gritty wobble; the knob still goes there.
The engine uses well under 0.1% of one core.

## Folders

| Folder | What it is |
|---|---|
| `engine/` | The LFOs, the rate law, drift, hold and the CC stream (`CcStream`). Plain C++, no plugin code; reusable for the later modulation plugins. |
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
  44.1 / 48 / 96 kHz, an identical CC stream at any block size, the CC rules (slowest LFO never jumps more than one
  step, the 6 ms cap, coarse before fine, nothing sent when nothing changes) and the CPU use.
- **Mapper test**: the real mapper script run in a stand-in for Scripter, fed by the engine's real CC stream: values,
  Min/Max, upside-down lanes, 14-bit detail, notes passing, Ogdoad's CC stopped, the channel setting.
- **Mac build**: universal binary (Apple Silicon + Intel), ad-hoc signing, Apple's `auval` (as a MIDI effect,
  `aumi Ogd1 Pdot`), pluginval at strictness 5 on the AU and the VST3, the package contents.
- **Window picture and plugin checks**: the real window drawn to pictures (artifact **Ogdoad-window-pictures**), every
  control moving only its own parameter, save and reload, MIDI passing through, CC on the chosen channel, mod wheel,
  pedal, bypass, block sizes.

## Fixed names

Plugin code `Ogd1`, manufacturer `Pdot`, bundle id `com.particledots.ogdoad`, target `Ogdoad`, engine library
`ogdoad_engine`. Parameter ids: `rate`, `modAmount`, `modSource`, `hold`, `holdPedal`, `drift`, `outChannel`.
These never change, so saved projects keep finding the plugin and its settings.
