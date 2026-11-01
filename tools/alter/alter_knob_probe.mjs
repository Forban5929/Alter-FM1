// SPDX-License-Identifier: GPL-3.0-only
// Does each main knob change the sound? node tools/alter/alter_knob_probe.mjs build/emu/felucca.wasm 23,24 (ALT_PRE indices)
// Each sound alone on circle 1, then KNOB 1..4 (E0..E3) at 0 32 64 96 127: level (dB RMS), brightness (~centroid, Hz),
// length (time over 10 % of the peak in the 2 s). Uses web_alter op 14 (alter_ui.c).
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2]), FS = 44100, STEP = Math.floor(FS * 60 / 120 / 4);
const which = process.argv[3] ? process.argv[3].split(",").map(Number) : [23, 24, 25, 26, 27, 28];
async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports; ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n, keep) => { const got = []; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
    if (keep) { const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) got.push(l[i]); } } return got; };
  render(FS * 2); return { ex, render };
}
function metrics(a) {
  let s = 0, pk = 0; for (const x of a) { s += x * x; pk = Math.max(pk, Math.abs(x)); }
  const rms = Math.sqrt(s / a.length);
  // centroid via zero crossings + first difference energy ratio
  let d = 0; for (let i = 1; i < a.length; i++) d += (a[i] - a[i - 1]) ** 2;
  const bright = Math.sqrt(d / (s + 1e-12)) * FS / (2 * Math.PI);    // ~ spectral centroid (Hz)
  // length: time the envelope stays above -30 dB of its peak after each hit (first 0.5 s of the bar)
  const W = 256; let env = []; for (let i = 0; i + W <= a.length; i += W) { let e = 0; for (let j = 0; j < W; j++) e = Math.max(e, Math.abs(a[i + j])); env.push(e); }
  const top = Math.max(...env); let last = 0; env.forEach((e) => { if (e > top * 0.1) last++; }); last = last / env.length * a.length / W;
  return { db: (20 * Math.log10(rms + 1e-9)).toFixed(1), pk: pk.toFixed(3), hz: bright.toFixed(0), len: (last * W / FS).toFixed(2) };
}
const P = ["E0", "E1", "E2", "E3"];
for (const snd of which) {
  const run = async (k, v) => {
    const { ex, render } = await boot(); const W = (op, a, b) => ex.web_alter(op, a, b);
    W(4, 120, 0); W(0, 0, snd); W(2, 1, 1); W(2, 2, 1); W(5, 0, 0);
    if (k >= 0) W(14, 0, (k << 8) | v);
    render(2000); ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
    render(STEP); return metrics(render(STEP * 16, true));
  };
  const base = await run(-1, 0);
  console.log(`sound ${snd}: ${JSON.stringify(base)}`);
  for (let k = 0; k < 4; k++) {
    const row = [];
    for (const v of [0, 32, 64, 96, 127]) { const m = await run(k, v); row.push(`${v}:${m.db}dB ${m.hz}Hz ${m.len}s`); }
    console.log(`  ${P[k]}  ${row.join(" | ")}`);
  }
}
