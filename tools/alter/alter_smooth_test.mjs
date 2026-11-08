// SPDX-License-Identifier: GPL-3.0-only
// SMOOTH (08/11): the PLAY view drawn in bands while the circles turn = the same pixels as a whole redraw; the bands'
// cost, SMOOTH ON / OFF, against whole frames (the browser's CPU: an order of size, not the FM-1's).
// node tools/alter/alter_smooth_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports, mem = ex.memory;
let fails = 0;
const check = (what, ok) => { console.log(`smooth: ${what.padEnd(80)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
render(44100);
ex.web_buttons(1 << 10); render(2048); ex.web_buttons(0); render(4410);   // PLAY
const fb = () => Uint16Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240));
for (const on of [1, 0]) {
  ex.web_alter_smooth(on);
  ex.web_draw_bench(1, 0);
  let bad = 0, frames = 0, moved = 0, prev = fb();
  for (let k = 0; k < 120; k++) {                       // 120 frames of ~15 ms: ~1.8 s of play
    render(662);
    ex.web_draw_bench(1, 3);                            // the loop's own drawing at this instant (bands when it can)
    const a = fb();
    if (a.some((v, i) => v !== prev[i])) moved++;
    ex.web_draw_bench(1, 0);                            // a whole redraw of the same instant
    const b = fb();
    if (a.some((v, i) => v !== b[i])) bad++;
    prev = b; frames++;
  }
  check(`SMOOTH ${on ? "ON " : "OFF"} : ${frames} images, ${moved} ont bougé, ${bad} différentes d'un redessin complet`, bad === 0 && moved > (on ? 90 : 5));
}
// cost: the loop's own frames (partial) vs whole frames, ~15 ms of audio between each
const cost = (on, whole) => {
  ex.web_alter_smooth(on); ex.web_draw_bench(1, 0);
  let t = 0;
  for (let k = 0; k < 200; k++) { render(662); const t0 = performance.now(); ex.web_draw_bench(1, whole ? 0 : 3); t += performance.now() - t0; }
  return t / 200;
};
const full = cost(1, 1), sOn = cost(1, 0), sOff = cost(0, 0);
console.log(`smooth: coût moyen d'une image (navigateur) : complète ${full.toFixed(2)} ms, SMOOTH ON ${sOn.toFixed(2)} ms (${(100 * sOn / full).toFixed(0)} %), OFF ${sOff.toFixed(2)} ms (${(100 * sOff / full).toFixed(0)} %)`);
console.log(fails ? `smooth: ${fails} ÉCHEC(S)` : "smooth: tout ok");
process.exit(fails ? 1 : 0);
