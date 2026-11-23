// SPDX-License-Identifier: GPL-3.0-only
// v1.20: each factory scene circle's loudness, alone (RUMBLE off): run it on an old build and on today's, the differences x 2
// = ALT_SCENE fix[] (alter_sounds.c). node tools/alter/alter_scene_fix.mjs build/emu/felucca.wasm
import fs from "node:fs";
const [,, wasmf] = process.argv;
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmf), {});
const ex = instance.exports, mem = ex.memory; ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const FS = 44100;
const render = (n, keep) => { let s = 0, k2 = 0; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
  if (keep) { const l = new Float32Array(mem.buffer, ex.web_out_l(), m), r = new Float32Array(mem.buffer, ex.web_out_r(), m); for (let i = 0; i < m; i++) { s += l[i] * l[i] + r[i] * r[i]; k2 += 2; } } } return 10 * Math.log10(s / k2 + 1e-20); };
render(FS * 2); ex.web_alter(18, 1, 0);
const res = [];
for (let s = 0; s < 4; s++) {
  const row = [];
  for (let c = 0; c < 3; c++) {
    ex.web_alter(1, s, 0); render(FS / 2);
    for (let o = 0; o < 3; o++) ex.web_alter(2, o, o === c ? 0 : 1);
    ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
    render(FS); row.push(+render(FS * 8, true).toFixed(2));
    ex.web_buttons(1 << 10); render(128); ex.web_buttons(0); render(FS);
  }
  res.push(row);
}
console.log(JSON.stringify(res));
