// SPDX-License-Identifier: GPL-3.0-only
// ALTER v1.34 (Romain 25/11: "un max par cercle, puis les 3 cercles en même temps : le MASTER ne doit pas saturer trop vite").
//   node tools/alter/alter_headroom.mjs build/emu/felucca.wasm [hits]
// 1. each factory sound alone on circle 1, its own LEVEL then LEVEL 127 (+7.5 dB, the top), HITS/16 at 120 BPM:
//    the loudest sample into the MASTER limiter (peek 25; LIM_T 18000 = the limiter starts, 32767 = the soft clip).
// 2. the three circles together: the loudest sounds of each family, at their LEVEL then all at 127.
// dB are against LIM_T (0 = the limiter starts working; above 0 it pulls the mix down).
import fs from "node:fs";

const [wasmPath, hitsArg] = process.argv.slice(2);
const HITS = +(hitsArg || 4), FS = 44100, wasm = fs.readFileSync(wasmPath);
const src = fs.readFileSync(new URL("../../firmware/src/alter_sounds.c", import.meta.url), "utf8");
const sounds = [...src.matchAll(/^\s*\{"([A-Z ]+)", ([0-9]), \{[^}]*\}, \{[^}]*\}, \{[^}]*\}, (\d+)/gm)]
  .map((m) => ({ name: m[1], eng: +m[2], lvl: +m[3] }));
const ENG = ["ALTER", "PHYS", "NOISE", "RESO", "PLATE", "CORDE", "RING", "WAVE"];
const LIM_T = 18000, P_LEVEL = 0;
const db = (a) => (a > 0 ? 20 * Math.log10(a / LIM_T) : -99);

async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  render(FS * 2);
  return { ex, render };
}
const play = (ex, render) => { ex.web_buttons(1 << 10); render(128); ex.web_buttons(0); };

// circles: [[sound index, level or null (its own)], ...] up to 3; returns the peak into the limiter (4 bars)
async function measure(circles, hits = HITS) {
  const { ex, render } = await boot();
  ex.web_alter(4, 120, 0);
  for (let c = 0; c < 3; c++) {
    if (c < circles.length) {
      const [i, lvl] = circles[c];
      ex.web_alter(0, c, i);
      ex.web_alter(5, c, 0);
      ex.web_alter(15, c, 16 | hits << 8);
      ex.web_alter(2, c, 0);
      if (lvl != null) ex.web_alter(17, c | P_LEVEL << 8, lvl);
    } else ex.web_alter(2, c, 1);
  }
  render(2000);
  play(ex, render);
  render(FS); ex.web_alter_peek(0, 25);                         // (the attack of the first bar, then measured)
  render(FS * 8);
  return ex.web_alter_peek(0, 25);
}

const rows = [];
for (let i = 0; i < sounds.length; i++) {
  const s = sounds[i];
  const own = await measure([[i, null]]), top = await measure([[i, 127]]);
  rows.push({ i, n: `${ENG[s.eng]} ${s.name}`, lvl: s.lvl, own: db(own), top: db(top) });
}
rows.sort((a, b) => b.top - a.top);
console.log(`ONE CIRCLE, ${HITS}/16 (dB vs the limiter's threshold; > 0 = the limiter works)`);
console.log("sound               LEVEL  own   at 127");
for (const r of rows)
  console.log(`${r.n.padEnd(20)} ${String(r.lvl).padStart(3)}  ${r.own.toFixed(1).padStart(5)}  ${r.top.toFixed(1).padStart(5)}${r.top > 0 ? "  LIMIT" : ""}`);

const idx = (name) => sounds.findIndex((s) => s.name === name);
const combos = [["KICK", "WARM", "CRUSH"], ["KICK", "GROWL", "SNARE"], ["THUD", "STAB", "HAT"], ["KICK", "SHREDDER", "LARSEN"],
  ["TOMS", "BROKEN", "ZAP"]];
console.log(`\nTHREE CIRCLES TOGETHER, ${HITS}/16`);
for (const c of combos) {
  const a = await measure(c.map((n) => [idx(n), null])), b = await measure(c.map((n) => [idx(n), 127]));
  console.log(`${c.join(" + ").padEnd(26)} own ${db(a).toFixed(1).padStart(5)}   all 127 ${db(b).toFixed(1).padStart(5)}`);
}
