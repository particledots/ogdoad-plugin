// Ogdoad Mapper, for Logic's Scripter (MIDI FX). Version 0.1.0.
//
// Ogdoad sends its eight LFOs as smooth 14-bit MIDI CC (LFO 1 to 8 = CC 20 to 27 coarse, CC 52 to 59 fine, on MIDI
// channel 16 unless you changed it). This script turns them into movements of knobs on any plugin BELOW it on the
// same channel strip (the instrument, or an audio effect).
//
// Setup, once per Mac:
//   Add Scripter in the MIDI FX slot just BELOW Ogdoad, click "Open Script in Editor", delete everything,
//   paste this whole file, click "Run Script", close the editor, and in Scripter's preset menu choose
//   "Save As..." and call it "Ogdoad Mapper". From then on it is in Scripter's preset menu.
// Per track:
//   Ogdoad in MIDI FX slot 1, Scripter with the "Ogdoad Mapper" preset in slot 2.
//   For each lane you want: pick a Source (LFO 1 to 8), open its Target menu, choose "Learn plug-in parameter"
//   and move the knob you want to modulate. Min and Max set the range the knob will sweep (Max below Min turns it
//   upside down). Need more than 8? Add a second Scripter with the same preset below the first.
//
// Ogdoad's CC messages stop here, so they never reach the instrument. Everything else passes through unchanged.

var PluginParameters = [
    { name: "Ogdoad MIDI channel", type: "lin", minValue: 1, maxValue: 16, numberOfSteps: 15, defaultValue: 16 }
];

var LANES = 8;
var SOURCES = ["Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8"];

for (var lane = 1; lane <= LANES; lane++) {
    PluginParameters.push({ name: "Lane " + lane + " source", type: "menu", valueStrings: SOURCES, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " target", type: "target" });
    PluginParameters.push({ name: "Lane " + lane + " min %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 0 });
    PluginParameters.push({ name: "Lane " + lane + " max %", type: "lin", minValue: 0, maxValue: 100, numberOfSteps: 1000, defaultValue: 100 });
}

var MSB_FIRST = 20, LSB_FIRST = 52, OUTPUTS = 8;

var channel = 16;
var laneSource = [], laneMin = [], laneMax = [], laneTarget = [], laneLast = [];
var coarse = [];
var ready = false;

for (var i = 0; i < LANES; i++) {
    laneSource.push(0); laneMin.push(0); laneMax.push(1); laneLast.push(-1);
    laneTarget.push("Lane " + (i + 1) + " target");
}
for (var j = 0; j < OUTPUTS; j++) coarse.push(0);

function readAll() {
    channel = Math.round(GetParameter("Ogdoad MIDI channel"));
    for (var i = 0; i < LANES; i++) {
        laneSource[i] = Math.round(GetParameter("Lane " + (i + 1) + " source"));
        laneMin[i] = GetParameter("Lane " + (i + 1) + " min %") / 100;
        laneMax[i] = GetParameter("Lane " + (i + 1) + " max %") / 100;
        laneLast[i] = -1;
    }
    ready = true;
}

function ParameterChanged(index, value) {
    if (index == 0) { channel = Math.round(value); return; }
    var lane = Math.floor((index - 1) / 4), which = (index - 1) % 4;
    if (lane < 0 || lane >= LANES) return;
    if (which == 0) laneSource[lane] = Math.round(value);
    else if (which == 2) laneMin[lane] = value / 100;
    else if (which == 3) laneMax[lane] = value / 100;
    laneLast[lane] = -1;
}

function HandleMIDI(event) {
    if (!ready) readAll();
    if (event instanceof ControlChange && event.channel == channel) {
        var n = event.number;
        if (n >= MSB_FIRST && n < MSB_FIRST + OUTPUTS) {
            coarse[n - MSB_FIRST] = event.value;        // wait for the fine part
            return;
        }
        if (n >= LSB_FIRST && n < LSB_FIRST + OUTPUTS) {
            var out = n - LSB_FIRST;
            var v = (coarse[out] * 128 + event.value) / 16383;
            for (var lane = 0; lane < LANES; lane++) {
                if (laneSource[lane] != out + 1) continue;
                var x = laneMin[lane] + (laneMax[lane] - laneMin[lane]) * v;
                if (x == laneLast[lane]) continue;
                laneLast[lane] = x;
                var t = new TargetEvent();
                t.target = laneTarget[lane];
                t.value = x;
                t.send();
            }
            return;
        }
    }
    event.send();
}
