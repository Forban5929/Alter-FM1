// SPDX-License-Identifier: GPL-3.0-only
// v1.2.19 (Romain 27/11, "comme le clic knob d'Elektron"): OCT- held alone + a knob turned = the setting straight to its
// bottom (left) / top (right), everywhere; alone, a knob moves as before.   node tools/alter/alter_jump_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const ms = (t) => { for (let k = 0, n = Math.round(t * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
let fails = 0;
const check = (w, ok) => { console.log(`jump: ${w.padEnd(84)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const FX = 0, SEQ = 9, OCTDN = 12, OCTUP = 13, K1 = 3, K2 = 4, P_DIST = 33;
const peek = (c, w) => ex.web_alter_peek(c, w) >>> 0;
const hits = () => (peek(0, 12) >> 8) & 255;
const turn = (role, d) => { ex.web_enc(role, d); ms(60); };
ms(2500);
ex.web_alter(24, 0, 2); ms(500);                                  // NEW scene
ex.web_buttons(1 << OCTDN); ms(700); const h1 = peek(0, 36); turn(K2, 1); ms(50); const h2 = peek(0, 36);
ex.web_buttons(0); ms(100);
check(`OCT- held: its hint (${h1}), gone once a knob turns (${h2})`, h1 === 1 && h2 === 0);
const kp = () => ex.web_alter_knobp(0, 0);
const e0 = kp(); turn(K1, 1); const e1 = kp();
ex.web_buttons(1 << OCTDN); ms(100); turn(K1, 1); const emax = kp(); turn(K1, -1); const emin = kp(); ex.web_buttons(0); ms(100);
check(`PLAY view, the sound's KNOB 1: alone ${e0} -> ${e1}, OCT- right ${emax}, left ${emin}`, e1 > e0 && emax === 127 && emin === 0);
ex.web_buttons(1 << SEQ); ms(300);                                // RHYTHM (SEQ held): KNOB 2 = HITS
const h0 = hits(); turn(K2, 1); const ha = hits();
ex.web_buttons(1 << SEQ | 1 << OCTDN); ms(100); turn(K2, 1); const hmax = hits(); turn(K2, -1); const hmin = hits();
ex.web_buttons(1 << SEQ | 1 << OCTDN | 1 << OCTUP); ms(100); turn(K2, 1); const hboth = hits();
ex.web_buttons(0); ms(300);
check(`SEQ held, HITS: alone ${h0} -> ${ha}, OCT- right ${hmax}, left ${hmin}, OCT- + OCT+ ${hboth}`,
      ha === h0 + 1 && hmax === 16 && hmin === 0 && hboth === 1);
ex.web_buttons(1 << FX); ms(300);                                 // the FX page (DIST tab), FX held
const d0 = peek(0, 100 + P_DIST); turn(K1, 1); const d1 = peek(0, 100 + P_DIST);
ex.web_buttons(1 << FX | 1 << OCTDN); ms(100); turn(K1, 1); const dmax = peek(0, 100 + P_DIST);
turn(K1, -1); const dmin = peek(0, 100 + P_DIST);
ex.web_buttons(0); ms(200);
check(`FX page DRIVE: alone ${d0} -> ${d1}, OCT- right ${dmax}, left ${dmin}`, d1 === d0 + 2 && dmax === 127 && dmin === 0);
console.log(fails ? `jump: ${fails} FAIL` : "jump: all ok");
process.exit(fails ? 1 : 0);
