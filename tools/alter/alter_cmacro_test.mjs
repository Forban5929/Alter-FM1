// SPDX-License-Identifier: GPL-3.0-only
// v1.2.15: a MACRO per circle (alter_macro.c amc_m[1..3]): the MACRO page's tab (PRESET) SCENE / I / II / III; a circle's
// recipe moves that circle alone; PERFORM's KNOB 1-3 turn them; kept with a USER scene (recipe + MACRO); an edited one's
// rows are the machine's; CHAOS / NEW put them at 0.   node tools/alter/alter_cmacro_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`cmacro: ${what.padEnd(84)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const OCTP = 13, PLAY = 10, HOME = 6, SEL = 0, PRESET = 2, K1 = 3, K2 = 4, K3 = 5, K4 = 6;
const P = { DIST: 33, DLY: 35, REV: 36 };
const peek = (c, w) => ex.web_alter_peek(c, w) >>> 0;
const mac = () => peek(0, 19), tab = () => (mac() >> 20) & 3;
const cm = (c) => { const v = peek(c, 34); return { val: v & 255, rcp: (v >> 8) & 127, ed: (v >> 15) & 1 }; };
const vals = () => [0, 1, 2].flatMap((c) => [peek(c, 100 + P.DIST), peek(c, 100 + P.DLY), peek(c, 100 + P.REV)]);
const turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); render(25); } };
render(2500);
ex.web_alter(24, 0, 2); render(500);                       // NEW
for (let c = 0; c < 3; c++) { ex.web_alter(17, c | (P.DIST << 8), 20); ex.web_alter(17, c | (P.DLY << 8), 10); ex.web_alter(17, c | (P.REV << 8), 30); }
ex.web_buttons(1 << PLAY); render(100); ex.web_buttons(0); render(500);
const v0 = vals();
ex.web_buttons(1 << OCTP); render(400);
turn(PRESET, 1); render(100);
check(`OCT+ held, PRESET +1: the tab I (${tab()}), TENSION at 0 (${cm(0).rcp} ${cm(0).val})`, tab() === 1 && cm(0).rcp === 0 && cm(0).val === 0);
turn(K1, 40); render(300);
const v1 = vals();
check(`KNOB 1 +40: circle I's MACRO 80 (${cm(0).val}), II and III at 0`, cm(0).val === 80 && cm(1).val === 0 && cm(2).val === 0);
check(`  circle I's DRIVE DELAY REVERB up, II and III as they were (${v0.join(",")} -> ${v1.join(",")})`,
      v1.slice(0, 3).every((v, i) => v > v0[i]) && v1.slice(3).every((v, i) => v === v0[3 + i]));
turn(K1, -40); render(300);
check(`KNOB 1 back to 0: circle I exactly as it was (${vals().slice(0, 3).join(",")})`, vals().every((v, i) => v === v0[i]));
turn(PRESET, 1); render(100);
turn(K2, 1); render(100);
check(`tab II (${tab()}), KNOB 2: recipe SPACE (${cm(1).rcp}), circle I's still TENSION (${cm(0).rcp})`, tab() === 2 && cm(1).rcp === 1 && cm(0).rcp === 0);
turn(SEL, 1); turn(K4, -5); render(100);
check(`SELECT row 2, KNOB 4 -5: circle II's MACRO edited (${cm(1).ed})`, cm(1).ed === 1);
turn(PRESET, 2); render(100);
check(`PRESET +2: back to SCENE (${tab()}), the scene's MACRO untouched (${mac() & 255})`, tab() === 0 && (mac() & 255) === 0);
ex.web_buttons(0); render(300);
// PERFORM: PLAY held 3 s, KNOB 1-3 = the circles' MACROs
ex.web_buttons(1 << PLAY); render(3300); ex.web_buttons(0); render(200);
check(`PERFORM open (${peek(0, 29) & 255}), bank MACRO (${(peek(0, 29) >> 8) & 255})`, (peek(0, 29) & 255) === 17 && ((peek(0, 29) >> 8) & 255) === 0);
turn(K1, 10); turn(K3, 20); render(1500);
check(`PERFORM KNOB 1 +10, KNOB 3 +20: circle I's MACRO ${cm(0).val}, III's ${cm(2).val}, II's ${cm(1).val}`, cm(0).val === 20 && cm(2).val === 40 && cm(1).val === 0);
const dI = peek(0, 100 + P.DIST), dII = peek(1, 100 + P.DIST);
check(`  circle I's DRIVE moved (${v0[0]} -> ${dI}), II's not (${dII})`, dI > v0[0] && dII === v0[3]);
ex.web_buttons(1 << HOME); render(100); ex.web_buttons(0); render(300);
ex.web_alter(23, 1, 0); render(400);                      // SAVE in a USER slot
ex.web_alter(24, 0, 2); render(500);                      // NEW
check(`NEW: every circle MACRO at 0 (${cm(0).val} ${cm(1).val} ${cm(2).val}), recipes kept (${cm(1).rcp})`, cm(0).val + cm(1).val + cm(2).val === 0 && cm(1).rcp === 1);
ex.web_alter(24, 1, 0); render(600);                      // A01 back
check(`the USER slot back: I ${cm(0).val} TENSION, II SPACE* (${cm(1).rcp} ${cm(1).ed}), III ${cm(2).val}`,
      cm(0).val === 20 && cm(0).rcp === 0 && cm(1).rcp === 1 && cm(1).ed === 1 && cm(2).val === 40);
const r2 = (() => { ex.web_buttons(1 << OCTP); render(400); turn(PRESET, 2); render(100); turn(SEL, 1); render(50); const v = peek(0, 21); turn(PRESET, 2); ex.web_buttons(0); render(300); return v; })();
check(`  circle II's row 2 amount as edited (the machine's rows): ${(r2 >>> 16) - 64}`, (r2 >>> 16) - 64 === 39);
ex.web_alter(22, 0, 0); render(500);                      // CHAOS TOTAL
check(`CHAOS TOTAL: every circle MACRO at 0 (${cm(0).val} ${cm(1).val} ${cm(2).val})`, cm(0).val + cm(1).val + cm(2).val === 0);
console.log(fails ? `cmacro: ${fails} FAIL` : "cmacro: all ok");
process.exit(fails ? 1 : 0);
