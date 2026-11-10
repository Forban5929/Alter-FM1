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
ex.web_alter_smcap(0);                                  // (every frame drawn: compared with a whole one)
for (const on of [1, 0]) {
  ex.web_alter_smooth(on);
  ex.web_draw_bench(1, 0);
  let bad = 0, frames = 0, moved = 0, prev = fb();
  for (let k = 0; k < 300; k++) {                       // 300 frames of ~15 ms: ~4.5 s of play (the alidade's turns)
    render(662);
    ex.web_draw_bench(1, 3);                            // the loop's own drawing at this instant (bands when it can)
    const a = fb();
    if (a.some((v, i) => v !== prev[i])) moved++;
    ex.web_draw_bench(1, 0);                            // a whole redraw of the same instant
    const b = fb();
    if (a.some((v, i) => v !== b[i])) bad++;
    prev = b; frames++;
  }
  check(`SMOOTH ${on ? "ON " : "OFF"} : ${frames} images, ${moved} ont bougé, ${bad} différentes d'un redessin complet`, bad === 0 && moved > (on ? 250 : 5));
}
// cost: the loop's own frames (partial) vs whole frames, ~15 ms of audio between each
const cost = (on, whole) => {
  ex.web_alter_smcap(0);                                // (each frame drawn: the cost of one)
  ex.web_alter_smooth(on); ex.web_draw_bench(1, 0);
  let t = 0;
  for (let k = 0; k < 200; k++) { render(662); const t0 = performance.now(); ex.web_draw_bench(1, whole ? 0 : 3); t += performance.now() - t0; }
  return t / 200;
};
const px = (on) => {                                     // pixels sent to the screen a second of play, the loop's own
  ex.web_alter_smooth(on); ex.web_draw_bench(1, 0); render(4410);
  const p0 = ex.web_screen_px(); render(44100); return ex.web_screen_px() - p0;
};
ex.web_alter_smcap(28);                                 // (as on the FM-1: ~30 frames a second)
const pOn = px(1), pOff = px(0);
console.log(`smooth: pixels envoyés à l'écran par seconde de jeu : SMOOTH ON ${pOn} (${(pOn / 57600).toFixed(1)} écrans), OFF ${pOff} (${(pOff / 57600).toFixed(1)} écrans)`);
check(`SMOOTH ON envoie moins de 12 écrans entiers par seconde (${(pOn / 57600).toFixed(1)})`, pOn < 12 * 57600);
// the alidade's speed, SWING 60: each ~15 ms frame it should turn by about as much (the POS it used before: waits, jumps)
for (const old of [1, 0]) {
  ex.web_alter(4, 120, 0);                                   // (tempo 120)
  ex.web_alter_set_swing?.(60);
  const d = [];
  let prev = ex.web_alter_disp(0, old);
  for (let k = 0; k < 120; k++) { render(662); const v = ex.web_alter_disp(0, old); d.push((v - prev + 32768) % 32768); prev = v; }
  const s = d.slice(5).sort((a, b) => a - b), lo = s[Math.floor(s.length * 0.05)], hi = s[Math.floor(s.length * 0.95)];
  console.log(`smooth: pas de l'aiguille par image, SWING 60, ${old ? "avant (POS)" : "maintenant"} : de ${lo} à ${hi} (rapport ${(hi / Math.max(1, lo)).toFixed(2)})`);
  if (!old) check(`aiguille régulière avec du SWING (écart des pas par image ${(hi / Math.max(1, lo)).toFixed(2)} ≤ 1,3)`, lo > 0 && hi / lo <= 1.3);
}
const full = cost(1, 1), sOn = cost(1, 0), sOff = cost(0, 0);
console.log(`smooth: coût moyen d'une image (navigateur) : complète ${full.toFixed(2)} ms, SMOOTH ON ${sOn.toFixed(2)} ms (${(100 * sOn / full).toFixed(0)} %), OFF ${sOff.toFixed(2)} ms (${(100 * sOff / full).toFixed(0)} %)`);
// the LFO menu (Romain 10/11 "LFO: low frame rate"): its wave scrolling = its band alone, the same pixels; no whole screens
{
  const fb = () => Uint16Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240));
  ex.web_alter(27, 0, 0 | (4 << 3) | (2 << 8) | (40 << 16));   // (circle 1's LFO 1: SIN 1/8 on SHAPE)
  ex.web_buttons(1 << 3); render(Math.round(44.1 * 800)); ex.web_buttons(0); render(2646);   // LFO held: its menu
  let bad = 0;
  for (let k = 0; k < 40; k++) { render(331); ex.web_draw_bench(1, 0); ex.web_alter_matband(); const a = fb(); ex.web_draw_bench(1, 0);
    const b = fb(); if (a.some((v, i) => v !== b[i])) bad++; }
  check(`menu LFO : la bande de l'onde = un redessin complet (${bad} différentes sur 40)`, bad === 0);
  ex.web_alter_smcap(28); ex.web_draw_bench(1, 0); render(4410);
  const f0 = ex.web_alter_full(), p0 = ex.web_screen_px(); render(44100);
  const full = ex.web_alter_full() - f0, scr = (ex.web_screen_px() - p0) / 57600;
  check(`menu LFO : presque plus d'écran entier (${full}/s), ${scr.toFixed(1)} écrans de pixels/s (avant : ~67)`, full <= 3 && scr < 15);
  ex.web_buttons(1 << 3); render(2646); ex.web_buttons(0); render(2646);   // (closed)
  ex.web_alter(27, 0, 0);
}
ex.web_alter_smcap(28);
console.log(fails ? `smooth: ${fails} ÉCHEC(S)` : "smooth: tout ok");
process.exit(fails ? 1 : 0);
