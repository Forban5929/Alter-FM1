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
btn(HOME);
check(`HOME: PERFORM left (page ${pk().mn}), still playing`, pk().mn === 0 && playing() === 1);
ex.web_buttons(1 << PLAY); ms(100);
const still = playing();
ex.web_buttons(0); ms(100);
check(`PLAY pressed while playing: plays on while held (${still}), stops at its release (${playing()})`, still === 1 && playing() === 0);
console.log(`perform: ${fails} FAIL`);
process.exit(fails ? 1 : 0);
