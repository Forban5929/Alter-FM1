// SPDX-License-Identifier: GPL-3.0-only
// v0.74: the power-on animation (alt_splash_draw, ~1.5 s, its LEDs) and the screen asleep (the seal, test op 35):
//   node tools/alter/alter_boot_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`boot: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  const lit = () => { const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240); let n = 0; for (const v of fb) if (((v >> 8) | (v << 8)) & 0xE79C) n++; return n; };
  const hash = () => { const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240); let h = 0; for (const v of fb) h = (h * 31 + v) >>> 0; return h; };
  return { ex, render, lit, hash };
}
{
  const m = await boot();
  const seen = [];
  for (const at of [250, 600, 1000, 1700]) { m.render(at - (seen.length ? seen[seen.length - 1].at : 0)); seen.push({ at, lit: m.lit(), h: m.hash() }); }
  check(`splash: the picture grows while the hand turns (${seen.map((s) => s.lit).join(" < ")} lit px)`,
        seen[0].lit > 200 && seen[1].lit > seen[0].lit && seen[3].lit > seen[1].lit);
  check("splash: a new picture at each moment", new Set(seen.map((s) => s.h)).size === 4);
  m.render(1000);
  check("after ~2 s: the PLAY view (the splash gone)", m.hash() !== seen[3].h && m.lit() > seen[3].lit);
}
{
  const m = await boot();
  m.render(400); const h0 = m.hash();
  m.ex.web_buttons(1 << 6); m.render(60); m.ex.web_buttons(0); m.render(300);
  check("a button during the splash skips it (the PLAY view at once)", m.hash() !== h0 && m.lit() > 20000);
}
{
  const m = await boot();
  m.render(2500); const home = m.hash(), homeLit = m.lit();
  m.ex.web_alter(35, 0, 0); m.render(1500); const a = m.hash(), aLit = m.lit(); m.render(3000); const b = m.hash();
  check(`asleep: the seal, dark (${aLit} lit px against ${homeLit}) and breathing (moves)`, aLit < homeLit / 2 && a !== b);
  m.ex.web_buttons(1 << 6); m.render(100); m.ex.web_buttons(0); m.render(1500);
  check("a button wakes it: the PLAY view whole again, the press swallowed (no page change)", m.hash() === home);
}
console.log(fails ? `boot: ${fails} FAIL` : "boot: all ok");
process.exit(fails ? 1 : 0);
