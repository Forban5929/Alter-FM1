// SPDX-License-Identifier: GPL-3.0-only
// ALTER's level alignment (alter_fx.c): each sound's measured level, alone on circle 1 (its advised mode), so
// AC_TARGET can sit at their median; then CHAOS several times: each circle's output alone, after the alignment.
//   node tools/alter/alter_level.mjs build/emu/felucca.wasm
import fs from "node:fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports, mem = ex.memory, FS = 44100;
const render = (n, keep) => { const out = []; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m); if (keep) { const a = new Float32Array(mem.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) out.push(a[i]); } } return out; };
ex._initialize();
const loud = (a) => { const w = []; for (let i = 0; i + 1024 <= a.length; i += 1024) { let e = 0; for (let j = i; j < i + 1024; j++) e += a[j] * a[j]; w.push(Math.sqrt(e / 1024)); } w.sort((x, y) => y - x); return w.slice(0, 4).reduce((s, x) => s + x, 0) / 4; };
const db = (x) => (20 * Math.log10(x + 1e-9)).toFixed(1);
const boot = () => { ex.web_nor_erase(); ex.web_boot(); render(FS * 2); ex.web_alter(4, 120, 0); };
const play = () => { ex.web_buttons(1 << 10); render(128); ex.web_buttons(0); };
const L = [];
boot(); play();                                                  // (one instance, playing: web_boot boots once, PLAY toggles)
ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
for (let s = 0; s < 23; s++) {
  ex.web_alter(0, 0, s); ex.web_alter(5, 0, 0);
  render(FS / 2); ex.web_alter(11, 0, 0); render(FS * 9);
  L.push(ex.web_alter_cal(0, 0));
  console.log(`sound ${String(s).padStart(2)}  level ${String(L[s]).padStart(6)}`);
}
const med = [...L].sort((a, b) => a - b)[11];
console.log(`median ${med} (min ${Math.min(...L)}, max ${Math.max(...L)})`);
if (process.argv[3] === "chaos") {
  const K = (m) => ex.web_keys(m);
  [0, 1, 2].forEach((o) => ex.web_alter(2, o, 0));
  for (let r = 0; r < 10; r++) {
    render(FS);
    for (let i = 0; i < 1; i++) { K(1 << 7); render(400); K(1 << 7 | 1 << 9); render(400); K(1 << 7 | 1 << 9 | 1 << 11); render(2000); K(0); render(FS / 4); }
    render(FS * 9);                                              // (the alignment done)
    const lv = [0, 1, 2].map((c) => { [0, 1, 2].forEach((o) => ex.web_alter(2, o, o !== c)); render(FS); return loud(render(FS * 8, true)); });
    [0, 1, 2].forEach((o) => ex.web_alter(2, o, 0));
    const g = [0, 1, 2].map((c) => db(ex.web_alter_cal(c, 1) / 32768));
    const pre = lv.map((x, c) => 20 * Math.log10(x) - 20 * Math.log10(ex.web_alter_cal(c, 1) / 32768));
    const spr = (a) => (Math.max(...a) - Math.min(...a)).toFixed(1);
    console.log(`  without the alignment: ${pre.map((x) => x.toFixed(1)).join(" / ")} dB (spread ${spr(pre)} dB)`);
    console.log(`CHAOS ${r + 1}: output alone ${lv.map(db).join(" / ")} dB (spread ${(Math.max(...lv.map((x) => 20 * Math.log10(x))) - Math.min(...lv.map((x) => 20 * Math.log10(x)))).toFixed(1)} dB), gains ${g.join(" / ")} dB`);
  }
}
