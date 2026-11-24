// SPDX-License-Identifier: GPL-3.0-only
// MIDI OUT over USB (v1.29): SYSTEM > SETUP > MIDI OUT OFF / CLOCK / NOTES / ALL, against the browser build.
//   node tools/alter/alter_midiout_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`midiout: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const w = (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const take = () => { const out = []; let p; while ((p = ex.web_mo_take() >>> 0)) out.push(p); return out; };
const st = (p) => (p >> 8) & 255;
w(2500);
ex.web_mo_host(1);
ex.web_alter(4, 120, 0);                          // TEMPO 120
const tap = (b) => { ex.web_buttons(1 << b); w(40); ex.web_buttons(0); w(40); };
take();
// OFF: 1 s, nothing
w(1000); take();
ex.web_alter(36, 0, 0); w(1000);
check("OFF : rien n'est envoyé", take().length === 0);
// CLOCK: about 48 pulses in 1 s at 120 BPM (24 a beat), no notes
ex.web_alter(36, 0, 1); take(); w(1000);
let o = take();
const f8 = o.filter((p) => st(p) === 0xF8).length, notes = o.filter((p) => (st(p) & 0xF0) === 0x90).length;
check(`CLOCK : ${f8} tops en 1 s à 120 BPM (≈ 48), ${notes} note`, f8 >= 46 && f8 <= 50 && notes === 0);
// PLAY / STOP: START then STOP
tap(10); w(500); o = take();
const a1 = o.map(st);
tap(10); w(200); const a2 = take().map(st);
check(`PLAY → START (${a1.includes(0xFA) ? "FA" : "-"}) ; STOP → STOP (${a2.includes(0xFC) ? "FC" : "-"}) `,
      (a1.includes(0xFA) && a2.includes(0xFC)) || (a1.includes(0xFC) && a2.includes(0xFA)));
// NOTES: playing, note-ons on channels 1..3 and as many note-offs, no clock
const play = () => { tap(10); w(200); };
const pl = () => take();
ex.web_alter(36, 0, 2);
// make sure it plays: if the last tap stopped it, start again
play(); let first = pl();
if (!first.some((p) => (st(p) & 0xF0) === 0x90)) { play(); }
take(); w(2000); o = take();
const on = o.filter((p) => (st(p) & 0xF0) === 0x90 && (p >>> 24) > 0), off = o.filter((p) => (st(p) & 0xF0) === 0x80);
const chans = [...new Set(on.map((p) => st(p) & 15))].sort();
check(`NOTES : ${on.length} notes en 2 s, canaux ${chans.map((c) => c + 1).join(" ")}, ${off.length} fins, pas de clock`,
      on.length > 8 && chans.every((c) => c <= 2) && Math.abs(on.length - off.length) <= 3 && !o.some((p) => st(p) === 0xF8));
// ALL: both
ex.web_alter(36, 0, 3); take(); w(1000); o = take();
check("ALL : notes + clock", o.some((p) => st(p) === 0xF8) && o.some((p) => (st(p) & 0xF0) === 0x90));
// no host: nothing queued
ex.web_mo_host(0); w(500); ex.web_mo_host(1);
check("sans ordinateur branché : rien en attente", take().length === 0);
console.log(fails ? `midiout: ${fails} FAIL` : "midiout: tout ok");
process.exit(fails ? 1 : 0);
