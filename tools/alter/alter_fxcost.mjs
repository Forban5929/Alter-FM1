// SPDX-License-Identifier: GPL-3.0-only
// What each effect costs (Romain 10/11: "cuts with FILTER, MASTER COMP, DIST, DELAY, REVERB"): the browser's time to
// render 8 s of a factory scene, effects one by one and together (the browser's CPU: an order of size, not the FM-1's)
//   node tools/alter/alter_fxcost.mjs build/emu/felucca.wasm [scene]
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
const scene = Number(process.argv[3] || 1);
const P = { DIST: 33, DLY: 35, REV: 36 };
async function run(setup) {
  const { instance } = await WebAssembly.instantiate(wasm, {}); const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  render(44100);
  ex.web_alter(1, scene, 0); render(4410);
  ex.web_buttons(1 << 10); render(2048); ex.web_buttons(0); render(44100);
  for (let c = 0; c < 3; c++) { ex.web_alter(17, c | P.DIST << 8, 0); ex.web_alter(17, c | P.DLY << 8, 0); ex.web_alter(17, c | P.REV << 8, 0); }
  ex.web_alter(13, 0, 0); ex.web_alter(12, 22, 0);       // (the MASTER FILTER off)
  ex.web_alter(8, 3, 0);                                  // (MASTER COMP off)
  setup(ex);
  render(44100);
  let best = 1e9;
  for (let r = 0; r < 7; r++) { const t0 = performance.now(); render(44100 * 4); best = Math.min(best, performance.now() - t0); }
  return best / 4;                                        // ms of CPU a second of audio
}
const all3 = (ex, p, v) => { for (let c = 0; c < 3; c++) ex.web_alter(17, c | p << 8, v); };
const cfg = {
  "rien": () => {},
  "MASTER FILTER (ENV WAH)": (ex) => { ex.web_alter(13, 0, 7); },
  "MASTER FILTER (PEAK)": (ex) => { ex.web_alter(13, 0, 8); },
  "MASTER COMP 100": (ex) => { ex.web_alter(8, 3, 100); },
  "DIST 80 x3": (ex) => all3(ex, P.DIST, 80),
  "DELAY envoi 60 x3": (ex) => all3(ex, P.DLY, 60),
  "REVERB envoi 60 x3": (ex) => all3(ex, P.REV, 60),
  "CONSOLE COMP+EQ x3": (ex) => { for (let c = 0; c < 3; c++) { ex.web_alter(20, c | 0 << 8, 90); ex.web_alter(20, c | 3 << 8, 80); } },
  "TOUT": (ex) => { ex.web_alter(13, 0, 7); ex.web_alter(8, 3, 100); all3(ex, P.DIST, 80); all3(ex, P.DLY, 60); all3(ex, P.REV, 60); },
};
const base = await run(cfg["rien"]);
console.log(`fxcost: scène ${scene}, sans effet : ${base.toFixed(1)} ms de calcul par seconde de son`);
for (const [k, f] of Object.entries(cfg)) {
  if (k === "rien") continue;
  const v = await run(f);
  console.log(`fxcost: ${k.padEnd(26)} ${v.toFixed(1)} ms/s  (+${(v - base).toFixed(1)}, +${(100 * (v - base) / base).toFixed(0)} %)`);
}
