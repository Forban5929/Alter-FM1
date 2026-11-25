// SPDX-License-Identifier: GPL-3.0-only
// v1.36 SONG (Romain 25/11): A B C = HITS ROT LEN of the three circles, in the scene. node tools/alter/alter_song_test.mjs wasm
// An empty slot's key captures; a full one plays from the next bar; held alone 1 s, it is replaced; A held + B C = a chain;
// SAVE into a USER scene keeps slots and chain; NEW clears them; the USER scene brings them back.
import fs from "node:fs";
const [wasmPath] = process.argv.slice(2);
const FS = 44100, BPM = 120, BAR = Math.round(FS * 240 / BPM);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const ms = (t) => render(Math.round(t * 44.1));
let fails = 0;
const check = (name, ok) => { console.log(`song: ${name}`.padEnd(84) + (ok ? "ok" : "FAIL")); if (!ok) fails++; };
const K = { A: 19, B: 21, C: 23 };
let keys = 0;
const down = (k) => { keys |= 1 << K[k]; ex.web_keys(keys); ms(40); };
const up = (k) => { keys &= ~(1 << K[k]); ex.web_keys(keys); ms(40); };
const tap = (k) => { down(k); up(k); };
const c0 = () => { const v = ex.web_alter_peek(0, 26); return { hits: v & 255, len: (v >> 8) & 255, rot: (v >> 16) & 255, play: (v >> 24) & 255 }; };
const sg = () => { const v = ex.web_alter_peek(0, 27); return { used: v & 7, n: (v >> 4) & 31, on: (v >> 9) & 1 }; };

ms(2500);
ex.web_alter(4, BPM, 0);
ex.web_alter(24, 0, 2);                                         // NEW: no slot
ms(200);
check(`NEW: no slot, no chain (used ${sg().used}, links ${sg().n})`, sg().used === 0 && sg().n === 0);
ex.web_alter(15, 0, 16 | 4 << 8); ms(200);                      // circle 1: LEN 16 HITS 4
ex.web_buttons(1 << 10); ms(50); ex.web_buttons(0);            // PLAY
ms(300);
tap("A");
check(`A tapped, empty: captured (used ${sg().used})`, sg().used === 1);
ex.web_alter(15, 0, 12 | 7 << 8); ms(200);                      // circle 1: LEN 12 HITS 7
tap("B");
check(`B tapped, empty: captured (used ${sg().used})`, sg().used === 3);
tap("A");
ms(2 * BAR * 1000 / FS);
let s = c0();
check(`A tapped, full: at the bar, circle 1 back to LEN 16 HITS 4 (${s.len} ${s.hits}, slot ${s.play})`, s.len === 16 && s.hits === 4 && s.play === 1);
tap("B");
ms(2 * BAR * 1000 / FS);
s = c0();
check(`B: LEN 12 HITS 7 (${s.len} ${s.hits})`, s.len === 12 && s.hits === 7);
ex.web_alter(15, 0, 12 | 3 << 8); ms(200);                      // circle 1 HITS 3 while B plays: kept in B
tap("A"); ms(2 * BAR * 1000 / FS);
tap("B"); ms(2 * BAR * 1000 / FS);
check(`a slot is a snapshot: an edit while B played did not go into it (HITS ${c0().hits})`, c0().hits === 7);
ex.web_alter(15, 0, 16 | 9 << 8); ms(200);                      // HITS 9, then A held alone 1.2 s: A replaced
down("A"); ms(1200); up("A");
tap("B"); ms(2 * BAR * 1000 / FS);
tap("A"); ms(2 * BAR * 1000 / FS);
check(`A held 1 s: replaced (A plays HITS ${c0().hits}, LEN ${c0().len})`, c0().hits === 9 && c0().len === 16);
down("A"); tap("B"); tap("B"); up("A");                          // a chain A B B
ms(100);
check(`A held + B, B: a chain of 3, on (${sg().n}, ${sg().on})`, sg().n === 3 && sg().on === 1);
ex.web_alter(23, 0, 0); ms(300);                                // SAVE into USER 1
ex.web_alter(24, 0, 2); ms(300);                                // NEW
check(`NEW after: slots and chain gone (${sg().used}, ${sg().n})`, sg().used === 0 && sg().n === 0);
ex.web_alter(24, 0, 0); ms(300);                                // USER 1
check(`USER 1 again: A B and the chain of 3 back (${sg().used}, ${sg().n})`, sg().used === 3 && sg().n === 3);
console.log(`song: ${fails} FAIL`);
process.exit(fails ? 1 : 0);
