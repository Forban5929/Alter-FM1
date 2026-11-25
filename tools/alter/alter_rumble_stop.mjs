// SPDX-License-Identifier: GPL-3.0-only
// v1.2.12 (Romain 25/11: "le rumble ... reste quand je mets pause"): KICK + RUMBLE four to the floor, then STOP: the level
// after the stop, each 0.5 s (dB). It should be gone within ~2 s. node tools/alter/alter_rumble_stop.mjs wasm [RUMBLE 80]
import fs from "node:fs";
const [wasmPath, amtArg] = process.argv.slice(2);
const AMT = +(amtArg || 80), FS = 44100;
const src = fs.readFileSync(new URL("../../firmware/src/alter_sounds.c", import.meta.url), "utf8");
const KICK = [...src.matchAll(/^\s*\{"([A-Z ]+)", ([0-9]), \{/gm)].findIndex((m) => m[1] === "KICK");
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const out = [];
const render = (n, keep) => { for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
  if (keep) { const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) out.push(l[i]); } } };
render(FS * 2);
ex.web_alter(4, 120, 0);
ex.web_alter(0, 0, KICK); ex.web_alter(5, 0, 0);
ex.web_alter(15, 0, 16 | 4 << 8);
ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
ex.web_alter(17, 0 | 255 << 8, AMT);
render(2000);
ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
render(FS * 4);
ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);        // STOP
render(FS * 6, true);
const db = [];
for (let w = 0; w < 12; w++) { let e = 0; for (let i = 0; i < FS / 2; i++) { const v = out[w * FS / 2 + i]; e += v * v; } db.push((10 * Math.log10(e / (FS / 2) + 1e-20)).toFixed(0)); }
console.log(`RUMBLE ${AMT} after STOP, dB per 0.5 s: ${db.join(" ")}`);
const gone = db.findIndex((x) => +x < -80);
console.log(`rumble_stop: gone (under -80 dB) after ${gone < 0 ? "more than 6" : gone * 0.5} s ${gone >= 0 && gone <= 5 ? "ok" : "FAIL"}`);
process.exit(gone >= 0 && gone <= 5 ? 0 : 1);
