// SPDX-License-Identifier: GPL-3.0-only
// v1.2.14 REC + PLAY (Romain 26/11): together = from the top, 4 beats counted in (nothing plays), then REC on and the
// click on the first turn of the longest circle only; REC tapped = off as usual. node tools/alter/alter_recplay_test.mjs wasm
import fs from "node:fs";
const [wasmPath] = process.argv.slice(2);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const ms = (t) => render(Math.round(t * 44.1));
let fails = 0;
const check = (name, ok) => { console.log(`recplay: ${name}`.padEnd(90) + (ok ? "ok" : "FAIL")); if (!ok) fails++; };
const PLAY = 10, REC = 11, BEAT = 500;                          // (120 BPM)
const playing = () => (ex.web_alter_peek(0, 12) >>> 24) & 1;
const recOn = () => (ex.web_alter_peek(0, 12) >>> 25) & 1;
const p33 = () => { const v = ex.web_alter_peek(0, 33) >>> 0; return { cin: v & 255, left: (v >> 8) & 255, n: v >>> 16 }; };
ms(2500);
const turn = Math.max(...[0, 1, 2].map((c) => ex.web_alter_peek(c, 26) >>> 0 >> 8 & 255));
// REC down first, then PLAY
ex.web_buttons(1 << REC); ms(60); ex.web_buttons((1 << REC) | (1 << PLAY)); ms(60); ex.web_buttons(0); ms(40);
const c0 = p33();
check(`REC then PLAY: counting in (${c0.cin} beats left), not playing yet (${playing()}), REC lit (${recOn()})`, c0.cin >= 3 && playing() === 0 && recOn() === 1);
ms(4 * BEAT - 160 + 60);
const c1 = p33();
check(`after 4 beats: playing (${playing()}), the click on the first turn (${c1.left} steps left of ${turn})`, playing() === 1 && c1.left > 0);
ms(turn * BEAT / 4 + 100);
const c2 = p33();
check(`after the longest turn: click off (${c2.left} left), still playing and REC (${playing()} ${recOn()})`, c2.left === 0 && playing() === 1 && recOn() === 1);
const n2 = c2.n; ms(4 * BEAT);
check(`then no more clicks (${n2} -> ${p33().n})`, p33().n === n2);
ex.web_buttons(1 << REC); ms(60); ex.web_buttons(0); ms(60);
check(`REC tapped: off (${recOn()}), still playing`, recOn() === 0 && playing() === 1);
// while playing, PLAY down first then REC: from the top again with the count-in
ex.web_buttons(1 << PLAY); ms(100); ex.web_buttons((1 << PLAY) | (1 << REC)); ms(60); ex.web_buttons(0); ms(40);
check(`PLAY then REC (while playing): counted in again (${p33().cin}), REC lit (${recOn()})`, p33().cin >= 3 && recOn() === 1);
ms(4 * BEAT);
check(`and plays after (${playing()})`, playing() === 1);
// PLAY alone still stops; REC alone still toggles
ex.web_buttons(1 << PLAY); ms(60); ex.web_buttons(0); ms(60);
check(`PLAY alone: stops (${playing()}), the click stopped too (${p33().left})`, playing() === 0 && p33().left === 0);
ex.web_buttons(1 << PLAY); ms(60); ex.web_buttons(0); ms(60);
check(`PLAY alone again: plays at once, no count-in (${playing()} ${p33().cin})`, playing() === 1 && p33().cin === 0);
console.log(fails ? `recplay: ${fails} FAIL` : "recplay: all ok");
process.exit(fails ? 1 : 0);
