// SPDX-License-Identifier: GPL-3.0-only
// One part setting swept on circle 1: level, bass (<100 Hz), brightness. node tools/alter/alter_param_probe.mjs wasm SOUND "id=v,..." ID v1,v2,...
// node p2.mjs wasm sound "fixed: id=v,id=v" sweepId v1,v2,...
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2]), FS = 44100, STEP = Math.floor(FS * 60 / 120 / 4);
const snd = Number(process.argv[3]);
const fixed = process.argv[4] ? process.argv[4].split(",").filter(Boolean).map((s) => s.split("=").map(Number)) : [];
const sw = Number(process.argv[5]), vals = process.argv[6].split(",").map(Number);
async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports; ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n, keep) => { const got = []; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
    if (keep) { const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) got.push(l[i]); } } return got; };
  render(FS * 2); return { ex, render };
}
function metrics(a) {
  let s = 0, d = 0, lp = 0, lo = 0;
  for (let i = 0; i < a.length; i++) { s += a[i] ** 2; if (i) d += (a[i] - a[i - 1]) ** 2; lp += (a[i] - lp) * 0.0142; lo += lp * lp; }
  const db = (x) => (10 * Math.log10(x / a.length + 1e-12)).toFixed(1);
  return `${db(s).padStart(6)}dB bas ${db(lo).padStart(6)}dB ${(Math.sqrt(d / (s + 1e-12)) * FS / 6.2832).toFixed(0).padStart(5)}Hz`;
}
const out = [];
for (const v of vals) {
  const { ex, render } = await boot(); const W = (op, a, b) => ex.web_alter(op, a, b);
  W(4, 120, 0); W(0, 0, snd); W(2, 1, 1); W(2, 2, 1); W(5, 0, 0); W(18, 1, 0);
  for (const [id, x] of fixed) W(17, id << 8, x);
  if (sw >= 0) W(17, sw << 8, v);
  render(2000); ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
  render(STEP); out.push(`${String(v).padStart(4)}: ${metrics(render(STEP * 16, true))}`);
}
console.log(out.join("\n"));
