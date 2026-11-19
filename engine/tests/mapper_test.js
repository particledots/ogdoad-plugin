// Runs the real Scripter mapper script with a stand-in for Logic's Scripter, fed by the engine's real CC stream
// (from ogdoad_ccdump). Usage: OGDOAD_CCDUMP=<path to ogdoad_ccdump> node mapper_test.js <Ogdoad-Mapper.js>

"use strict";
const fs = require("fs");
const vm = require("vm");
const { execFileSync } = require("child_process");

let checks = 0, failures = 0;
function expect(ok, what) { checks++; if (!ok) { failures++; console.log("FAIL: " + what); } }

const scriptText = fs.readFileSync(process.argv[2], "utf8");
const dumpExe = process.env.OGDOAD_CCDUMP;
const dump = execFileSync(dumpExe, ["30"], { maxBuffer: 1 << 28 }).toString().trim().split("\n");
const events = [], finals = [];
for (const line of dump) {
    const p = line.split(" ");
    if (p[0] === "final") finals[+p[1]] = +p[2];
    else events.push({ sample: +p[0], cc: +p[1], value: +p[2] });
}
expect(events.length > 5000, "the engine produced a CC stream (" + events.length + " messages)");

// ---- A stand-in for Scripter ---------------------------------------------------------------------------------
function makeScripter(settings, callParameterChanged) {
    const out = { passed: [], targets: [] };
    class Event { send() { out.passed.push(this); } trace() {} }
    class ControlChange extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, number: 0, value: 0 }, o); } }
    class NoteOn extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, pitch: 60, velocity: 100 }, o); } }
    class NoteOff extends Event { constructor(o) { super(); Object.assign(this, { channel: 1, pitch: 60, velocity: 0 }, o); } }
    class TargetEvent { constructor() { this.target = ""; this.value = 0; } send() { out.targets.push({ target: this.target, value: this.value }); } }
    const values = {};
    const sandbox = {
        Event, ControlChange, NoteOn, NoteOff, TargetEvent, Math,
        GetParameter: (name) => { if (!(name in values)) throw new Error("GetParameter unknown name " + name); return values[name]; },
        Trace: () => {},
    };
    vm.createContext(sandbox);
    vm.runInContext(scriptText, sandbox);
    const params = vm.runInContext("PluginParameters", sandbox);
    params.forEach(p => { if (p.type !== "target") values[p.name] = p.defaultValue !== undefined ? p.defaultValue : 0; });
    for (const [name, v] of Object.entries(settings)) {
        values[name] = v;
        if (callParameterChanged) {
            const index = params.findIndex(p => p.name === name);
            sandbox.ParameterChanged(index, v);
        }
    }
    return { sandbox, params, out, values };
}

// ---- 1. The script itself -----------------------------------------------------------------------------------------
{
    const { params } = makeScripter({}, false);
    const names = params.map(p => p.name);
    expect(params.length === 33, "33 settings (channel + 8 lanes x 4)");
    expect(new Set(names).size === names.length, "setting names are unique");
    expect(params.filter(p => p.type === "target").length === 8, "8 Learn targets");
    expect(params.every(p => ["lin", "menu", "target"].includes(p.type)), "only setting types Scripter knows");
    expect(!/\.trace\(\)/.test(scriptText), "no trace printing in the mapper");
}

// ---- 2. Nothing set up: Ogdoad's CC stops, everything else passes -----------------------------------------------
{
    const s = makeScripter({}, false);
    const { ControlChange, NoteOn, NoteOff } = s.sandbox;
    for (const e of events.slice(0, 2000)) s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: e.cc, value: e.value }));
    s.sandbox.HandleMIDI(new NoteOn({ pitch: 64 }));
    s.sandbox.HandleMIDI(new NoteOff({ pitch: 64 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 1, number: 1, value: 90 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: 7, value: 100 }));
    s.sandbox.HandleMIDI(new ControlChange({ channel: 3, number: 20, value: 5 }));
    expect(s.out.targets.length === 0, "lanes Off: no knob moves");
    expect(s.out.passed.length === 5, "notes and other CC pass through (" + s.out.passed.length + ")");
    expect(s.out.passed[0].pitch === 64 && s.out.passed[0] instanceof NoteOn, "notes pass unchanged");
    expect(s.out.passed.every(e => !(e.channel === 16 && ((e.number >= 20 && e.number <= 27) || (e.number >= 52 && e.number <= 59)))),
        "Ogdoad's own CC never reaches the instrument");
}

// ---- 3. Four lanes set up, real stream ----------------------------------------------------------------------------
for (const viaParameterChanged of [true, false]) {
    const how = viaParameterChanged ? " (settings changed in the panel)" : " (settings restored with a project)";
    const settings = {
        "Lane 1 source": 1,
        "Lane 2 source": 8, "Lane 2 min %": 20, "Lane 2 max %": 80,
        "Lane 3 source": 8, "Lane 3 min %": 100, "Lane 3 max %": 0,
        "Lane 4 source": 3,
    };
    const s = makeScripter(settings, viaParameterChanged);
    const { ControlChange, NoteOn } = s.sandbox;
    const coarse = [];
    const expectedLast = {};
    let notesIn = 0;
    const t0 = process.hrtime.bigint();
    events.forEach((e, k) => {
        s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: e.cc, value: e.value }));
        if (e.cc >= 20 && e.cc < 28) coarse[e.cc - 20] = e.value;
        if (e.cc >= 52 && e.cc < 60) expectedLast[e.cc - 52] = (coarse[e.cc - 52] * 128 + e.value) / 16383;
        if (k % 500 === 0) { s.sandbox.HandleMIDI(new NoteOn({ pitch: 40 + (k % 30) })); notesIn++; }
    });
    const usPerEvent = Number(process.hrtime.bigint() - t0) / 1000 / events.length;

    const byTarget = name => s.out.targets.filter(t => t.target === name).map(t => t.value);
    const l1 = byTarget("Lane 1 target"), l2 = byTarget("Lane 2 target"), l3 = byTarget("Lane 3 target"), l4 = byTarget("Lane 4 target");
    expect(l1.length > 100 && l2.length > 100 && l3.length > 100 && l4.length > 100, "all four lanes move their knobs" + how);
    expect(byTarget("Lane 5 target").length === 0, "lanes left Off stay still" + how);
    const near = (a, b) => Math.abs(a - b) < 1e-9;
    expect(near(l1[l1.length - 1], finals[0] / 16383), "lane 1 ends exactly where LFO 1 ended" + how);
    expect(near(l4[l4.length - 1], finals[2] / 16383), "lane 4 ends exactly where LFO 3 ended" + how);
    expect(near(l2[l2.length - 1], 0.2 + 0.6 * finals[7] / 16383), "lane 2: LFO 8 scaled to 20..80%" + how);
    expect(near(l3[l3.length - 1], 1 - finals[7] / 16383), "lane 3: LFO 8 upside down" + how);
    expect(l2.every(v => v >= 0.2 - 1e-12 && v <= 0.8 + 1e-12), "lane 2 never leaves 20..80%" + how);
    expect(l1.every(v => v >= 0 && v <= 1), "targets always 0..1" + how);
    expect(Math.max(...l1) > 0.99 && Math.min(...l1) < 0.01, "LFO 1 sweeps the whole range" + how);
    // Fine resolution reaches the target: many distinct values between two coarse steps.
    const distinct = new Set(l1.map(v => Math.round(v * 16383))).size;
    expect(distinct > 4000, "14-bit detail reaches the target (" + distinct + " distinct values)" + how);
    const whole = l1.every(v => Math.abs(v * 16383 - Math.round(v * 16383)) < 1e-6);
    const notCoarse = l1.filter(v => Math.round(v * 16383) % 128 !== 0).length > l1.length / 2;
    expect(whole && notCoarse, "values arrive in 14-bit steps, not 128 coarse steps" + how);
    expect(s.out.passed.length === notesIn, "notes still pass with lanes in use" + how);
    console.log("mapper: " + usPerEvent.toFixed(2) + " microseconds per CC message in Node" + how);
}

// ---- 4. A different channel ------------------------------------------------------------------------------------------
{
    const s = makeScripter({ "Ogdoad MIDI channel": 15, "Lane 1 source": 1 }, true);
    const { ControlChange } = s.sandbox;
    for (const e of events.slice(0, 400)) s.sandbox.HandleMIDI(new ControlChange({ channel: 16, number: e.cc, value: e.value }));
    expect(s.out.targets.length === 0 && s.out.passed.length === 400, "channel 15 set: channel 16 CC is left alone");
    for (const e of events.slice(0, 400)) s.sandbox.HandleMIDI(new ControlChange({ channel: 15, number: e.cc, value: e.value }));
    expect(s.out.targets.length > 10, "channel 15 set: channel 15 CC is mapped");
}

if (failures === 0) console.log("mapper tests: " + checks + " checks passed");
else console.log("mapper tests: " + failures + " of " + checks + " checks FAILED");
process.exit(failures === 0 ? 0 : 1);
