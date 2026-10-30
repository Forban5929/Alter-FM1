// SPDX-License-Identifier: GPL-3.0-only
// Screenshots of the browser build in Node: node tools/alter/alter_shot.mjs build/emu/felucca.wasm OUTDIR "script"
// The script: space-separated actions, run in order after the boot:
//   b:NAME (tap a button)  B:NAME+ / B:NAME- (hold / let go)  k:N / K:N+ / K:N- (key N: tap / hold / let go)
//   e:ROLE:N (turn encoder ROLE 0..6 by N)  w:MS (render)  s:NAME (save OUTDIR/NAME.ppm)
import fs from "node:fs";
const [wasmPath, out, script] = process.argv.slice(2);
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
let btn = 0, keys = 0;
render(2500);
for (const a of (script || "").split(/\s+/).filter(Boolean)) {
  const [op, x, y] = a.split(":");
  if (op === "b") { ex.web_buttons(btn | 1 << B[x]); render(60); ex.web_buttons(btn); render(60); }
  else if (op === "B") { const n = B[x.slice(0, -1)]; btn = x.endsWith("+") ? btn | 1 << n : btn & ~(1 << n); ex.web_buttons(btn); render(60); }
  else if (op === "k") { ex.web_keys(keys | 1 << +x); render(60); ex.web_keys(keys); render(60); }
  else if (op === "K") { const n = parseInt(x); keys = x.endsWith("+") ? keys | 1 << n : keys & ~(1 << n); ex.web_keys(keys); render(60); }
  else if (op === "e") { const n = +y; for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(+x, Math.sign(n)); render(30); } }
  else if (op === "w") render(+x);
  else if (op === "s") {
    const fb = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240), px = Buffer.alloc(240 * 240 * 3);
    for (let i = 0; i < 240 * 240; i++) { const v = fb[i], p = ((v >> 8) | (v << 8)) & 0xFFFF; px[i * 3] = (p >> 11 & 31) * 255 / 31; px[i * 3 + 1] = (p >> 5 & 63) * 255 / 63; px[i * 3 + 2] = (p & 31) * 255 / 31; }
    fs.mkdirSync(out, { recursive: true });
    fs.writeFileSync(`${out}/${x}.ppm`, Buffer.concat([Buffer.from(`P6 240 240 255\n`), px]));
  }
}
