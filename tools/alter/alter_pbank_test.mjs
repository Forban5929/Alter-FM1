// SPDX-License-Identifier: GPL-3.0-only
// v1.2.16 PERFORM: EDIT LFO GLO are the player's own (held 1 s = choose KNOB 1-3 a circle setting, KNOB 4 a GLOBAL; let
// go = kept, the machine's, kept at power-off).   node tools/alter/alter_pbank_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`pbank: ${what.padEnd(86)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const PLAY = 10, EDIT = 4, LFO = 3, K1 = 3, K4 = 6;
const pk = () => { const v = ex.web_alter_peek(0, 35) >>> 0; return { ed: v & 15, bank: (v >> 4) & 15, t: (v >> 8) & 255, g: (v >> 16) & 255 }; };
const knob = (i) => (ex.web_alter_peek(i, 29) >>> 16) & 255;
const turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); render(25); } };
render(2500);
ex.web_buttons(1 << PLAY); render(3300); ex.web_buttons(0); render(300);
check(`PERFORM open (${ex.web_alter_peek(0, 29) & 255})`, (ex.web_alter_peek(0, 29) & 255) === 17);
ex.web_buttons(1 << EDIT); render(100); ex.web_buttons(0); render(200);
const e0 = pk();
check(`EDIT tapped: bank EDIT (${e0.bank}), its factory targets TONE / FILTER (${e0.t} ${e0.g}), not choosing`, e0.bank === 5 && e0.t === 0 && e0.g === 3 && e0.ed === 0);
ex.web_buttons(1 << EDIT); render(1200);
check(`EDIT held 1 s: choosing (${pk().ed}), still the bank EDIT (${pk().bank})`, pk().ed === 5 && pk().bank === 5);
const before = ex.web_alter_peek(0, 133) >>> 0;
turn(K1, 12); turn(K4, 2); render(100);
const e1 = pk();
check(`KNOB 1 +12: circle setting 12 = DRIVE (${e1.t}); KNOB 4 +2: FILT DRIVE (${e1.g}); circle I's DRIVE not moved (${before} ${ex.web_alter_peek(0, 133) >>> 0})`, e1.t === 12 && e1.g === 5 && e1.ed === 5 && (ex.web_alter_peek(0, 133) >>> 0) === before);
ex.web_buttons(0); render(300);
check(`let go: kept (${pk().t} ${pk().g}), not choosing (${pk().ed})`, pk().t === 12 && pk().g === 5 && pk().ed === 0);
const d0 = ex.web_alter_peek(1, 100 + 33) >>> 0;   // circle II's DIST (P 33)
turn(4, 10); render(1200);
check(`KNOB 2 now moves circle II's DRIVE (${d0} -> ${ex.web_alter_peek(1, 133) >>> 0})`, (ex.web_alter_peek(1, 133) >>> 0) > d0);
ex.web_buttons(1 << LFO); render(100); ex.web_buttons(0); render(200);
check(`LFO tapped: its own factory targets still (${pk().bank} ${pk().t} ${pk().g})`, pk().bank === 6 && pk().t === 12 && pk().g === 4);
// kept at power-off: the settings to the flash (once stopped), then a cold boot on the same flash
ex.web_buttons(1 << PLAY); render(100); ex.web_buttons(0); render(3000);
ex.web_boot(); render(3000);
ex.web_buttons(1 << PLAY); render(3300); ex.web_buttons(0); render(300);
ex.web_buttons(1 << EDIT); render(100); ex.web_buttons(0); render(300);
check(`after a restart: EDIT still DRIVE / FILT DRIVE (${pk().t} ${pk().g})`, pk().bank === 5 && pk().t === 12 && pk().g === 5);
console.log(fails ? `pbank: ${fails} FAIL` : "pbank: all ok");
process.exit(fails ? 1 : 0);
