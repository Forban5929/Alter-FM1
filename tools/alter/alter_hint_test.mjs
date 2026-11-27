// SPDX-License-Identifier: GPL-3.0-only
// v1.2.18 HINTS: a key with combos held 0.4 s on the PLAY view shows them; let go: gone; SYSTEM > SETUP > HINTS OFF: none.
import fs from "node:fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const ms = (t) => { for (let k = 0, n = Math.round(t * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
let fails = 0;
const check = (w, ok) => { console.log(`hint: ${w.padEnd(80)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const id = () => ex.web_alter_peek(0, 36);
const OCTDN = 12, REC = 11;
ms(2500);
ex.web_buttons(1 << OCTDN); ms(200); const early = id(); ms(400);
check(`OCT- held: nothing at 0.2 s (${early}), its hint at 0.6 s (${id()})`, early === 0 && id() === 1);
ex.web_buttons(0); ms(100);
check(`let go: gone (${id()})`, id() === 0);
ex.web_buttons(1 << REC); ms(600); const r = id(); ex.web_buttons(0); ms(100);
ex.web_keys(1 << 19); ms(600); const a = id(); ex.web_keys(0); ms(100);
ex.web_keys(1 << 7); ms(600); const d = id(); ex.web_keys(0); ms(300);
check(`REC ${r}, SONG A ${a}, DICE ${d}`, r === 3 && a === 4 && d === 7);
ex.web_alter(37, 1, 0); ms(100);                                // (HINTS OFF)
ex.web_buttons(1 << OCTDN); ms(700); const off = id(); ex.web_buttons(0); ms(100);
check(`HINTS OFF: none (${off})`, off === 0);
console.log(fails ? `hint: ${fails} FAIL` : "hint: all ok");
process.exit(fails ? 1 : 0);
