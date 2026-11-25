// SPDX-License-Identifier: GPL-3.0-only
// v1.2.10 PERFORM (Romain 25/11): PLAY held 3 s opens it (the music goes on), its banks (FX SEL ENV SEQ EDIT LFO GLO,
// again = MACRO), the knobs = I II III MASTER through MIDI LEARN's setters, HOME leaves; PLAY tapped while playing stops at
// its release. node tools/alter/alter_perform_test.mjs wasm
import fs from "node:fs";
const [wasmPath] = process.argv.slice(2);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const ms = (t) => render(Math.round(t * 44.1));
let fails = 0;
const check = (name, ok) => { console.log(`perform: ${name}`.padEnd(84) + (ok ? "ok" : "FAIL")); if (!ok) fails++; };
const PLAY = 10, FX = 0, SCL = 1, ENV = 2, HOME = 6, SEQ = 9;
const btn = (b, t = 60) => { ex.web_buttons(1 << b); ms(t); ex.web_buttons(0); ms(60); };
const pk = () => { const v = ex.web_alter_peek(0, 29) >>> 0; return { mn: v & 255, bank: (v >> 8) & 255 }; };
const playing = () => (ex.web_alter_peek(0, 12) >>> 24) & 1;
const P_REV = 36, P_DEC = 2;

ms(2500);
btn(PLAY);
check(`PLAY tapped: playing (${playing()})`, playing() === 1);
ex.web_buttons(1 << PLAY); ms(1500);
check(`PLAY held 1.5 s: still playing, no page yet (page ${pk().mn})`, playing() === 1 && pk().mn !== 17);
ms(1700); ex.web_buttons(0); ms(100);
check(`PLAY held 3 s: PERFORM (page ${pk().mn}, bank ${pk().bank}), still playing`, pk().mn === 17 && pk().bank === 0 && playing() === 1);
btn(FX);
check(`FX tapped: the REVERB bank (${pk().bank}), no FX page`, pk().bank === 1 && pk().mn === 17);
const r0 = ex.web_alter_peek(0, 100 + P_REV), r1 = ex.web_alter_peek(1, 100 + P_REV);
for (let i = 0; i < 10; i++) { ex.web_enc(3, 1); ms(30); }
ms(300);
const r0b = ex.web_alter_peek(0, 100 + P_REV), r1b = ex.web_alter_peek(1, 100 + P_REV);
check(`KNOB 1: circle I's REVERB send up (${r0} -> ${r0b}), circle II's not (${r1} -> ${r1b})`, r0b > r0 && r1b === r1);
btn(ENV);
const kv = (c) => (ex.web_alter_peek(c, 29) >>> 16) & 255;
const d1 = kv(1);
for (let i = 0; i < 10; i++) { ex.web_enc(4, -1); ms(30); }
ms(1500);                                                       // (after a second the knobs read the machine again)
check(`ENV, KNOB 2: circle II's DECAY down and kept (${d1} -> ${kv(1)})`, pk().bank === 3 && kv(1) === d1 - 20);
btn(ENV);
check(`ENV again: back to MACRO (${pk().bank})`, pk().bank === 0);
btn(SEQ);
check(`SEQ: the DENSITY bank, no SEQ page (${pk().bank}, page ${pk().mn})`, pk().bank === 4 && pk().mn === 17);
// v1.2.12: the keyboard
const key = (k, t = 60) => { ex.web_keys(1 << k); ms(t); ex.web_keys(0); ms(40); };
const pp = () => { const v = ex.web_alter_peek(0, 30) >>> 0; return { sel: v & 7, rate: (v >> 3) & 3, fill: (v >> 5) & 3, up: (v >> 7) & 1, drop: (v >> 8) & 1, sq: v >>> 16 }; };
const p31 = () => { const v = ex.web_alter_peek(0, 31) >>> 0; return { morph: v & 255, hold: (v >> 8) & 0xffff, row: v >>> 24 }; };
const muted = (c) => ex.web_alter_peek(c, 12) & 1;
const BAR = 2100;                                               // (a bar at the default tempo, with room)
key(0);
check(`MUTE I (white key 0, as the normal view): muted at once (${muted(0)})`, muted(0) === 1);
key(2); key(0);
check(`MUTE II on, MUTE I off again (${muted(0)} ${muted(1)})`, muted(0) === 0 && muted(1) === 1);
key(2);
key(3);
check(`CIRCLE II (black key 3): the selection is II (${pp().sel})`, pp().sel === 2);
key(1);
check(`CIRCLE I too: I + II (${pp().sel})`, pp().sel === 3);
key(1); key(3);
check(`both again: ALL (${pp().sel})`, pp().sel === 0);
key(10); ms(600);
check(`FILTER B: the MASTER FILTER's MORPH glides to B and stays (${p31().morph})`, p31().morph === 127);
key(8); ms(80);
const mid = p31().morph;
ms(600);
check(`FILTER A: on its way (${mid}), then at A (${p31().morph})`, mid > 0 && mid < 127 && p31().morph === 0);
ex.web_keys(1 << 8); ms(40); ex.web_keys((1 << 8) | (1 << 10)); ms(600); ex.web_keys(0); ms(40);
check(`FILTER A + B: the middle (${p31().morph})`, p31().morph === 64);
const P_DLY = 35, dl = [0, 1, 2].map((c) => ex.web_alter_peek(c, 100 + P_DLY));
ex.web_keys(1 << 13); ms(300);
const dh = [0, 1, 2].map((c) => ex.web_alter_peek(c, 100 + P_DLY));
ex.web_keys(0); ms(300);
check(`DUB DELAY held, ALL: the three circles' DELAY sends up (${dl} -> ${dh})`, dh.every((v, c) => v > dl[c]));
key(5);
const dl2 = [0, 1, 2].map((c) => ex.web_alter_peek(c, 100 + P_DLY));
ex.web_keys(1 << 13); ms(300);
const dh2 = [0, 1, 2].map((c) => ex.web_alter_peek(c, 100 + P_DLY));
ex.web_keys(0); ms(300);
check(`CIRCLE III, DUB DELAY: only circle III's send up (${dl2} -> ${dh2})`, dh2[2] > dl2[2] && dh2[0] === dl2[0] && dh2[1] === dl2[1]);
key(5);
key(19);
check(`RATE +: 1/32 (${pp().rate})`, pp().rate === 3);
key(18); key(18);
check(`RATE - twice: 1/8 (${pp().rate})`, pp().rate === 1);
key(3);
ex.web_keys(1 << 21); ms(200);
check(`CIRCLE II + FILL held: filled (${pp().fill}), a roll held (${(p31().hold >> 1) & 1})`, pp().fill === 1 && ((p31().hold >> 1) & 1) === 1);
ex.web_keys(0); ms(100);
check(`.. let go: no fill (${pp().fill}, ${p31().hold})`, pp().fill === 0 && p31().hold === 0);
const row0 = p31().row;
key(22);
check(`NEXT SC: a scene waits (${pp().sq - 1}), not loaded yet (${p31().row})`, pp().sq > 0 && p31().row === row0);
ms(BAR);
check(`.. on the 1: loaded (row ${row0} -> ${p31().row}), nothing waiting`, p31().row === row0 + 1 && pp().sq === 0 && playing() === 1);
const fr0 = ex.web_alter_peek(0, 32) & 255, rv0 = (ex.web_alter_peek(1, 32) >>> 8) & 255, rI0 = (ex.web_alter_peek(0, 32) >>> 8) & 255;
ex.web_keys(1 << 26); ms(2 * BAR);
const fr1 = ex.web_alter_peek(0, 32) & 255, rv1 = (ex.web_alter_peek(1, 32) >>> 8) & 255, rI1 = (ex.web_alter_peek(0, 32) >>> 8) & 255;
check(`CIRCLE II, MONTEE held 2 bars: FREQ ${fr0} -> ${fr1}, circle II's REVERB ${rv0} -> ${rv1}, circle I's stays (${rI0} -> ${rI1})`, pp().up === 1 && fr1 > 40 && fr1 < 127 && rv1 > rv0 && rI1 === rI0);
ex.web_keys(0); ms(60);
check(`.. let go: waits for the drop (${pp().drop}), still up (${ex.web_alter_peek(0, 32) & 255})`, pp().drop === 1 && (ex.web_alter_peek(1, 32) >>> 8 & 255) >= rv1);
ms(BAR);
const fr2 = ex.web_alter_peek(0, 32) & 255, rv2 = (ex.web_alter_peek(1, 32) >>> 8) & 255;
check(`.. the drop on the 1: all back (FREQ ${fr2}, REVERB ${rv2})`, pp().drop === 0 && fr2 === fr0 && rv2 === rv0);
key(3);
btn(FX);
const t0 = kv(0);
ex.web_keys(1 << 25); ms(60);
for (let i = 0; i < 10; i++) { ex.web_enc(3, 1); ms(30); }
const t1 = kv(0);
ex.web_keys(0); ms(200);
check(`TEMP held + KNOB 1: ${t0} -> ${t1}; let go: back (${kv(0)})`, t1 > t0 && kv(0) === t0);
btn(HOME);
check(`HOME: PERFORM left (page ${pk().mn}), still playing`, pk().mn === 0 && playing() === 1);
ex.web_buttons(1 << PLAY); ms(100);
const still = playing();
ex.web_buttons(0); ms(100);
check(`PLAY pressed while playing: plays on while held (${still}), stops at its release (${playing()})`, still === 1 && playing() === 0);
console.log(`perform: ${fails} FAIL`);
process.exit(fails ? 1 : 0);
