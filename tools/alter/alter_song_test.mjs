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
// v1.37: ARP held + REC writes the chain by playing: A 3 bars, B 2 bars, A, then ARP tapped: A B A, it plays from A
{
  const ARP = 8, REC = 11, barMs = 2 * 1000;                    // (120 BPM: a bar = 2 s)
  const btn = (b, on) => { ex.web_buttons(on ? 1 << b : 0); ms(40); };
  tap("A"); ms(barMs * 1.5);                                    // A plays
  ex.web_buttons(1 << ARP); ms(40); ex.web_buttons((1 << ARP) | (1 << REC)); ms(40); ex.web_buttons(1 << ARP); ms(40); btn(ARP, 0);
  const armed = ex.web_alter_song(5);
  ms(barMs * 3);
  tap("B"); ms(barMs * 2);
  tap("A"); ms(barMs * 1);
  btn(ARP, 1); btn(ARP, 0);                                     // ARP tapped: the end at the next bar
  ms(barMs * 1.5);
  const n = ex.web_alter_song(2), L = [...Array(n)].map((_, k) => ex.web_alter_song(16 + k));
  const txt = L.map((l) => `${"ABC"[l & 3]}${(l >> 2) + 1}`).join(" ");
  check(`ARP held + REC: armed (${armed}); A B A written by playing (${txt}), the chain on (${ex.web_alter_song(3)}), done (${ex.web_alter_song(5)})`,
        armed === 1 && n === 3 && (L[0] & 3) === 0 && (L[1] & 3) === 1 && (L[2] & 3) === 0 &&
        Math.abs((L[0] >> 2) + 1 - 3) <= 1 && Math.abs((L[1] >> 2) + 1 - 2) <= 1 && ex.web_alter_song(3) === 1 && ex.web_alter_song(5) === 0);
}
// v1.38: the SONG page (ARP held): A B C tapped add a block; KNOB 3 moves the chosen one; OCT- held + SELECT removes it
{
  const ARP = 8, OCTDN = 12;
  ex.web_buttons(1 << ARP); ms(800); ex.web_buttons(0); ms(100);   // the page
  const n0 = ex.web_alter_song(2);
  tap("B"); tap("A");
  const n1 = ex.web_alter_song(2), last = ex.web_alter_song(16 + n1 - 1);
  check(`SONG page: B then A tapped, two blocks more (${n0} -> ${n1}), the last A 4 bars (${"ABC"[last & 3]}${(last >> 2) + 1})`,
        n1 === n0 + 2 && (last & 3) === 0 && (last >> 2) + 1 === 4);
  for (let i = 0; i < 3; i++) { ex.web_enc(5, -1); ms(40); }    // KNOB 3 to the left: the last block (A) one place earlier
  const moved = ex.web_alter_song(16 + n1 - 2), after = ex.web_alter_song(16 + n1 - 1);
  check(`KNOB 3: the chosen block moved left (${"ABC"[moved & 3]} before ${"ABC"[after & 3]})`, (moved & 3) === 0 && (after & 3) === 1);
  ex.web_buttons(1 << OCTDN); ms(40); ex.web_enc(0, 1); ms(40); ex.web_buttons(0); ms(40);   // OCT- held + SELECT: removed
  check(`OCT- + SELECT: a block removed (${n1} -> ${ex.web_alter_song(2)})`, ex.web_alter_song(2) === n1 - 1);
}
console.log(`song: ${fails} FAIL`);
process.exit(fails ? 1 : 0);
