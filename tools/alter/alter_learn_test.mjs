// SPDX-License-Identifier: GPL-3.0-only
// ALTER's MIDI LEARN (firmware/src/alter_learn.c, midi_control.c alm_midi) against the browser build:
//   node tools/alter/alter_learn_test.mjs build/emu/felucca.wasm
// SYSTEM (HOME held) > LEARN, as Romain would: GROUP with KNOB 1, SELECT the row, KNOB 1 > then a CC / a note
import fs from "node:fs";

const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`learn: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const B = { HOME: 6, PLAY: 10 };
const E = { SELECT: 0, ALGO: 1, K1: 3 };

async function boot(nor) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase();
  if (nor) new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()).set(nor);
  ex.web_boot();
  const t = {
    ex,
    w: (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); },
    enc: (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); t.w(30); } },
    tap: (b) => { ex.web_buttons(1 << b); t.w(60); ex.web_buttons(0); t.w(60); },
    peek: (c, w) => ex.web_alter_peek(c, w),
    midi: (st, d1, d2) => ex.web_midi(st, d1, d2),
    system: () => { ex.web_buttons(1 << B.HOME); t.w(700); ex.web_buttons(0); t.w(100); },
    // LEARN: group g (0..4), row r (1..), a control sent while it waits
    learn: (g, r, st, d1, d2) => {
      t.enc(E.SELECT, -60); t.enc(E.K1, -6); t.enc(E.K1, g);
      t.enc(E.SELECT, r); t.enc(E.K1, 1); t.midi(st, d1, d2); t.w(100);
    },
  };
  t.w(2500);
  return t;
}
const entry = (t, i) => { const v = t.peek(0, 400 + i) >>> 0; return { n: v & 255, ch: (v >> 8) & 255, code: (v >> 16) & 255 }; };
const links = (t) => Array.from({ length: 42 }, (_, i) => entry(t, i)).filter((e) => e.ch & 0x80);

let t = await boot();
const P_LEVEL = 0;
check(`à la 1re fois : aucun lien (${links(t).length})`, links(t).length === 0);
t.ex.web_alter(4, 120, 0);                                       // (TEMPO 120)
t.midi(0xB0, 7, 50); t.w(100);
check(`carte fixe : CC7 = 50 sur le canal 1 = LEVEL du cercle 1 (${t.peek(0, 100 + P_LEVEL)})`, t.peek(0, 100 + P_LEVEL) === 50);

t.system(); t.enc(E.ALGO, 1);                                   // SYSTEM > LEARN
// CIRCLE I, LEVEL (row 20): CC 21 on channel 1
t.learn(0, 20, 0xB0, 21, 100);
let L = links(t);
check(`CIRCLE I > LEVEL appris : CC21 CANAL 1 (${JSON.stringify(L)})`, L.length === 1 && L[0].n === 21 && (L[0].ch & 15) === 0 && L[0].code === 19);
t.midi(0xB0, 21, 127); t.w(100);
const lv127 = t.peek(0, 100 + P_LEVEL);
t.midi(0xB0, 21, 0); t.w(100);
check(`CC21 127 puis 0 : LEVEL du cercle 1 = ${lv127} puis ${t.peek(0, 100 + P_LEVEL)}`, lv127 === 127 && t.peek(0, 100 + P_LEVEL) === 0);
t.midi(0xB1, 21, 127); t.w(100);
check(`CC21 sur un autre canal (2) : pas ce lien (LEVEL reste ${t.peek(0, 100 + P_LEVEL)})`, t.peek(0, 100 + P_LEVEL) === 0);

// CIRCLE II, COMP (row 17): learn CC7 (the standard map's LEVEL) -> the map no longer sees CC7 on ch 1
t.learn(1, 17, 0xB0, 7, 10);
t.midi(0xB0, 7, 90); t.w(100);
check(`CC7 appris sur COMP du cercle 2 : COMP = ${(t.peek(1, 11) >>> 24) & 255}, LEVEL du cercle 1 inchangé (${t.peek(0, 100 + P_LEVEL)})`,
  ((t.peek(1, 11) >>> 24) & 255) === 90 && t.peek(0, 100 + P_LEVEL) === 0);

// one control, one thing: CC21 learned again on CIRCLE III HIGH (row 16) -> LEVEL of circle 1 lost its link
t.learn(2, 16, 0xB0, 21, 64);
L = links(t);
check(`un CC appris deux fois : un seul lien (CIRCLE III HIGH), ${L.length} liens`, L.length === 2 && L.some((e) => e.n === 21 && e.code === 2 * 32 + 15) && !L.some((e) => e.code === 19));

// a setting refuses a note
t.learn(0, 9, 0x90, 40, 100); t.midi(0x80, 40, 0); t.w(60);
check(`une note sur un réglage (DECAY) : refusée (${links(t).length} liens)`, links(t).length === 2);

// GLOBAL: FILTER MORPH (row 8), MASTER (row 1)
t.learn(3, 8, 0xB0, 30, 0);
t.midi(0xB0, 30, 99); t.w(100);
// ACTIONS: MUTE 2 (row 2) on a pad (note 36, channel 10); DICE 1 (row 4) on CC 64; PLAY (row 26) on CC 80
t.learn(4, 2, 0x99, 36, 100); t.midi(0x89, 36, 0); t.w(60);
t.learn(4, 4, 0xB0, 64, 127); t.midi(0xB0, 64, 0); t.w(60);
t.learn(4, 26, 0xB0, 80, 127); t.midi(0xB0, 80, 0); t.w(60);
L = links(t);
check(`ACTIONS appris : MUTE 2 = note 36 canal 10, DICE 1 = CC64, PLAY = CC80 (${L.length} liens)`, L.length === 6 &&
  L.some((e) => e.n === (0x80 | 36) && (e.ch & 15) === 9 && e.code === 128 + 1) && L.some((e) => e.n === 64 && e.code === 128 + 3));
t.tap(B.HOME); t.w(200);                                         // back to PLAY

const mute = () => t.peek(1, 12) & 1;
const m0 = mute();
t.midi(0x99, 36, 110); t.w(80); t.midi(0x89, 36, 0); t.w(80);
const m1 = mute();
t.midi(0x99, 36, 110); t.midi(0x89, 36, 0); t.w(80); t.w(80);   // (on and off in one pass: a tap)
check(`pad (note 36) = MUTE 2 : ${m0} -> ${m1} -> ${mute()} (même un coup très court)`, m1 === 1 - m0 && mute() === m0);

const p0 = t.ex.web_alter_pat(0);
let changed = 0;
for (let k = 0; k < 4; k++) { t.midi(0xB0, 64, 127); t.w(80); t.midi(0xB0, 64, 0); t.w(150); if (t.ex.web_alter_pat(0) !== p0) changed++; }
check(`CC64 = DICE 1 : le motif du cercle 1 change (${changed}/4 tirages)`, changed >= 2);

const pl0 = t.ex.web_playing();
t.midi(0xB0, 80, 127); t.w(80); t.midi(0xB0, 80, 0); t.w(300);
const pl1 = t.ex.web_playing();
t.midi(0xB0, 80, 127); t.w(80); t.midi(0xB0, 80, 0); t.w(300);
check(`CC80 = PLAY / STOP : ${pl0} -> ${pl1} -> ${t.ex.web_playing()}`, pl1 !== pl0 && t.ex.web_playing() === pl0);
check(`GLOBAL > FILTER MORPH = CC30 (99) ; CC 120..127 jamais appris`, links(t).some((e) => e.n === 30 && e.code === 96 + 7));

// the links are kept in the flash: after a power-off, CC21 moves circle 3's HIGH again
t.w(3000);
const nor = new Uint8Array(t.ex.memory.buffer, t.ex.web_nor(), t.ex.web_nor_size()).slice();
const t2 = await boot(nor);
t2.midi(0xB0, 21, 0); t2.w(100);
check(`après extinction : ${links(t2).length} liens gardés, CC21 = HIGH du cercle 3 (KILL : ${(t2.peek(2, 11) >>> 16) & 255})`,
  links(t2).length === 6 && ((t2.peek(2, 11) >>> 16) & 255) === 0);

// CLEAR (KNOB 1 <) and CLEAR ALL (GLOBAL's last row)
t2.system(); t2.enc(E.ALGO, 1);
t2.enc(E.SELECT, -60); t2.enc(E.K1, -6); t2.enc(E.K1, 4); t2.enc(E.SELECT, 2); t2.enc(E.K1, -1);
check(`KNOB 1 < sur MUTE 2 : ce lien effacé (${links(t2).length} liens)`, links(t2).length === 5);
t2.enc(E.SELECT, -60); t2.enc(E.K1, -6); t2.enc(E.K1, 3); t2.enc(E.SELECT, 13); t2.enc(E.K1, -1);
check(`CLEAR ALL : plus aucun lien (${links(t2).length})`, links(t2).length === 0);
t2.tap(B.HOME); t2.w(100);
t2.midi(0xB0, 7, 33); t2.w(100);
check(`après CLEAR ALL : CC7 retourne à la carte fixe (LEVEL du cercle 1 = ${t2.peek(0, 100 + P_LEVEL)})`, t2.peek(0, 100 + P_LEVEL) === 33);

console.log(fails ? `learn: ${fails} FAIL` : "learn: all ok");
process.exit(fails ? 1 : 0);
