// SPDX-License-Identifier: GPL-3.0-only
// v1.06: INSERT OFF with AMNT 50, FX page, KNOB 3 one click at a time: level, brightness, TYPE, A. node tools/alter/alter_insert_click.mjs wasm SOUND
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2]), FS = 44100, STEP = Math.floor(FS * 60 / 120 / 4);
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports; ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n, keep) => { const got = []; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
  if (keep) { const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) got.push(l[i]); } } return got; };
const db = (a) => { let s = 0, d = 0; for (let i = 0; i < a.length; i++) { s += a[i] ** 2; if (i) d += (a[i] - a[i - 1]) ** 2; } return `${(10 * Math.log10(s / a.length)).toFixed(1)}dB ${(Math.sqrt(d / s) * FS / 6.2832).toFixed(0)}Hz`; };
render(FS * 2);
const W = (op, a, b) => ex.web_alter(op, a, b);
W(4, 120, 0); W(0, 0, +process.argv[3]); W(2, 1, 1); W(2, 2, 1); W(5, 0, 0); W(18, 1, 0);
W(17, 96 << 8, 0); W(17, 97 << 8, 50);
render(2000); ex.web_buttons(1 << 10); render(128); ex.web_buttons(0); render(STEP);
console.log("avant      :", db(render(STEP * 16, true)), " A =", ex.web_alter_peek(0, 100 + 97));
ex.web_buttons(1 << 0); render(Math.round(FS * 0.7)); ex.web_buttons(0); render(FS * 0.1 | 0);   // FX held: the FX page
for (let k = 1; k <= 4; k++) {
  ex.web_enc(5, 1); render(STEP);
  console.log(`cran ${k}     :`, db(render(STEP * 16, true)), " TYPE =", ex.web_alter_peek(0, 100 + 96), " A =", ex.web_alter_peek(0, 100 + 97));
}
