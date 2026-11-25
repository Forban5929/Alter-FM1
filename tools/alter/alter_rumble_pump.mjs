// SPDX-License-Identifier: GPL-3.0-only
// v1.35 (Romain 25/11: "la pompe pas assez creusée"): the RUMBLE alone (KICK with RUMBLE minus the same KICK without, the
// build is deterministic), its level through the beat in 10 ms windows, averaged over 8 beats: the dip (dB under the
// beat's loudest window) and where it comes back. node tools/alter/alter_rumble_pump.mjs build/emu/felucca.wasm [RUMBLE 80]
import fs from "node:fs";
const [wasmPath, amtArg] = process.argv.slice(2);
const AMT = +(amtArg || 80), FS = 44100, BPM = 120, wasm = fs.readFileSync(wasmPath);
const src = fs.readFileSync(new URL("../../firmware/src/alter_sounds.c", import.meta.url), "utf8");
const KICK = [...src.matchAll(/^\s*\{"([A-Z ]+)", ([0-9]), \{/gm)].findIndex((m) => m[1] === "KICK");
async function take(rmb) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const out = [];
  const render = (n, keep) => { for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
    if (keep) { const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) out.push(l[i]); } } };
  render(FS * 2);
  ex.web_alter(4, BPM, 0);
  ex.web_alter(0, 0, KICK); ex.web_alter(5, 0, 0);
  ex.web_alter(15, 0, 16 | 4 << 8);                            // four to the floor
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
  ex.web_alter(17, 0 | 255 << 8, rmb);
  render(2000);
  ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
  render(FS * 4);                                               // (the room fills)
  render(FS * 4, true);                                         // 8 beats
  return out;
}
const a = await take(AMT), b = await take(0);
const beat = Math.round(FS * 60 / BPM), win = Math.round(FS / (+process.env.WINHZ || 100)), nw = Math.floor(beat / win);
const e = new Array(nw).fill(0);
for (let k = 0; k < 8; k++)
  for (let w = 0; w < nw; w++)
    for (let i = 0; i < win; i++) { const j = k * beat + w * win + i; const d = a[j] - b[j]; e[w] += d * d; }
const db = e.map((x) => 10 * Math.log10(x / (8 * win) + 1e-20));
const top = Math.max(...db), low = Math.min(...db.slice(0, Math.floor(nw / 2)));
const back = db.findIndex((x, w) => w > db.indexOf(low) && x > top - 3);
console.log(`RUMBLE ${AMT}: dip ${(top - low).toFixed(1)} dB (lowest at ${db.indexOf(low) * 10} ms), back within 3 dB at ${back * 10} ms of ${nw * 10}`);
console.log(db.map((x) => (x - top).toFixed(0)).join(" "));
