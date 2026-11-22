// SPDX-License-Identifier: GPL-3.0-only
// v1.1 knob audit (Romain 22/11: "pas de sauts brutaux, des plages qui changent vraiment le son"): every factory sound on
// circle 1 alone, each of its engine's 8 settings (E1..E8) swept 0..127 by 4 (or its own range), one bar at each:
// level (dB RMS), brightness (~centroid), length (time over 10 % of the peak). Then: JUMPS (one step of 4 that moves
// the level >= 3 dB, the brightness x1.4 or the length x1.5), DEAD zones (>= 40 % of the travel that changes nothing
// heard) and settings that change nothing at all. node tools/alter/alter_knob_audit.mjs build/emu/felucca.wasm OUT.json
import fs from "node:fs";
import { req, parse, splitFrames, F_ENUM } from "../../web/alter/link.js";
const wasm = fs.readFileSync(process.argv[2]), OUTF = process.argv[3] || "/tmp/knob_audit.json";
const only = process.argv[4] ? process.argv[4].split(",").map(Number) : null;
const FS = 44100, BAR = FS * 2;
async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n, keep) => { const got = keep ? new Float32Array(n) : null; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
    if (keep) got.set(new Float32Array(mem.buffer, ex.web_out_l(), m), k); } return got; };
  render(FS * 2);
  const out = () => { const n = ex.web_link_out_n(), b = Array.from(new Uint8Array(mem.buffer, ex.web_link_out(), n)); ex.web_link_out_clear(); return splitFrames(b).map(parse).filter(Boolean); };
  const one = (bytes) => { new Uint8Array(mem.buffer, ex.web_link_in_buf(), bytes.length).set(bytes); ex.web_link_in(bytes.length); return out()[0]; };
  return { ex, render, one };
}
function metrics(a) {
  let s = 0, d = 0, pk = 0;
  for (let i = 0; i < a.length; i++) { s += a[i] * a[i]; if (i) d += (a[i] - a[i - 1]) ** 2; pk = Math.max(pk, Math.abs(a[i])); }
  const W = 256; let over = 0;
  for (let i = 0; i + W <= a.length; i += W) { let e = 0; for (let j = 0; j < W; j++) e = Math.max(e, Math.abs(a[i + j])); if (e > pk * 0.1) over++; }
  return { db: 10 * Math.log10(s / a.length + 1e-12), hz: Math.sqrt(d / (s + 1e-12)) * FS / (2 * Math.PI), len: over * W / FS };
}
const B0 = await boot();
const info = B0.one(req.info());
const sounds = info.sounds;
const res = [];
for (let si = 0; si < sounds.length; si++) {
  if (only && !only.includes(si)) continue;
  const snd = sounds[si], eng = snd.eng ?? snd.engine;
  const descs = info.ids.slice(0, 8).map((id) => B0.one(req.desc(2, id, eng)));
  for (let k = 0; k < 8; k++) {
    const d = descs[k];
    if (!d || !d.label || d.max <= d.min) continue;
    const vals = [];
    for (let v = d.min; v < d.max; v += d.fmt === F_ENUM ? 1 : 4) vals.push(v);
    vals.push(d.max);
    const { ex, render } = await boot();
    const W = (op, a, b) => ex.web_alter(op, a, b);
    W(4, 120, 0); W(0, 0, si); W(2, 1, 1); W(2, 2, 1); W(5, 0, 0); W(18, 1, 0);
    render(2000); ex.web_buttons(1 << 10); render(128); ex.web_buttons(0); render(BAR);
    const pts = [];
    for (const v of vals) {
      W(14, 0, (k << 8) | (v & 255));
      render(FS / 2);
      const m = metrics(render(BAR, true));
      pts.push({ v, ...m });
    }
    res.push({ si, sound: snd.name, eng: info.engines[eng], k, label: d.label, enumerated: d.fmt === F_ENUM, min: d.min, max: d.max, def: d.def, pts });
  }
  process.stderr.write(`${si} ${snd.name} ${info.engines[eng]}\n`);
}
fs.writeFileSync(OUTF, JSON.stringify(res));
