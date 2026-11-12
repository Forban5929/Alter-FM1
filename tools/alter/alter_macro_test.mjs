// SPDX-License-Identifier: GPL-3.0-only
// v0.77: the MACRO (alter_macro.c): OCT+ held + KNOB 1 moves its recipe's targets from where they are, back exactly at 0;
// a target turned by hand keeps its new place; KNOB 2 the recipe; kept with a USER scene; CHAOS puts it at 0.
//   node tools/alter/alter_macro_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`macro: ${what.padEnd(78)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const OCTP = 13, K1 = 3, K2 = 4, K3 = 5, K4 = 6, SEL = 0;
const P = { DIST: 33, DLY: 35, REV: 36 };
const peek = (c, w) => ex.web_alter_peek(c, w) >>> 0;
const mac = () => peek(0, 19);
const vals = () => [0, 1, 2].flatMap((c) => [peek(c, 100 + P.DIST), peek(c, 100 + P.DLY), peek(c, 100 + P.REV)]);
const turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); render(25); } };
render(2500);
ex.web_alter(24, 0, 2); render(500);                       // NEW (no REC moves: only the MACRO moves the settings here)
for (let c = 0; c < 3; c++) {                              // settings to move from (alter_ui.c op 17: a circle's P)
  ex.web_alter(17, c | (P.DIST << 8), 20 + c * 10); ex.web_alter(17, c | (P.DLY << 8), 10 + c * 5); ex.web_alter(17, c | (P.REV << 8), 30);
}
ex.web_buttons(1 << 10); render(100); ex.web_buttons(0); render(500);   // PLAY
const v0 = vals();
ex.web_buttons(1 << OCTP); render(400);
check(`OCT+ held 0.4 s: the MACRO page shown, TENSION at 0 (${(mac() >> 8) & 127}, ${mac() & 255})`, (mac() >> 16) === 1 && ((mac() >> 8) & 127) === 0 && (mac() & 255) === 0);
turn(K1, 40); render(200);
const v1 = vals();
check(`KNOB 1 +40: MACRO 80, DRIVE / DELAY / REVERB of the 3 circles up (${v0.join(",")} -> ${v1.join(",")})`,
      (mac() & 255) === 80 && v1.every((v, i) => v > v0[i] || v === 127 || v >= 120));
turn(K1, -40); render(200);
const v2 = vals();
check(`KNOB 1 back to 0: every target exactly where it was (${v2.join(",")})`, v2.every((v, i) => v === v0[i]));
// by hand while the MACRO is up: circle 1's DRIVE set to 10; back to 0: 10 minus the MACRO's share, kept
turn(K1, 30); render(200);
ex.web_buttons(0); render(300);
check("OCT+ let go: the page gone, the MACRO stays at 60", (mac() >> 16) === 0 && (mac() & 255) === 60);
ex.web_alter(17, 0 | (P.DIST << 8), 100); render(300);
ex.web_buttons(1 << OCTP); render(100); turn(K1, -30); render(200);
const d = peek(0, 100 + P.DIST);
check(`a target turned by hand keeps its place: DRIVE 100 by hand, MACRO back to 0 -> ${d} (below 100, others as before)`,
      d < 100 && d > 40 && peek(1, 100 + P.DIST) === v0[3] && peek(2, 100 + P.REV) === v0[8]);
// KNOB 2: SPACE; a row edited: KNOB 4 on row 1 -> '*'
turn(K2, 1); render(100);
check(`KNOB 2: recipe SPACE (${(mac() >> 8) & 127})`, ((mac() >> 8) & 127) === 1 && (mac() & 32768) === 0);
turn(SEL, 1); turn(K4, -5); render(100);
const r1 = peek(0, 21);
check(`SELECT row 2, KNOB 4 -5: its amount ${(r1 >>> 16) - 64}, the recipe marked edited`, (r1 >>> 16) - 64 === 39 && (mac() & 32768) !== 0);
turn(K1, 50); render(200);
ex.web_buttons(0); render(300);
const mv = mac();
ex.web_alter(23, 1, 0); render(400);                      // SAVE in a USER slot
ex.web_alter(24, 0, 2); render(500);                      // NEW
check(`NEW: the MACRO at 0 (${mac() & 255})`, (mac() & 255) === 0);
ex.web_alter(24, 1, 0); render(600);                      // A01 back
check(`the USER slot back: its MACRO (${mac() & 255} SPACE*, row 2 ${(peek(0, 21) >>> 16) - 64})`,
      (mac() & 0xFFFF) === (mv & 0xFFFF) && (peek(0, 21) >>> 16) - 64 === 39);
ex.web_alter(22, 0, 0); render(500);                      // CHAOS TOTAL
check(`CHAOS TOTAL: the MACRO at 0 (${mac() & 255})`, (mac() & 255) === 0);
console.log(fails ? `macro: ${fails} FAIL` : "macro: all ok");
process.exit(fails ? 1 : 0);
