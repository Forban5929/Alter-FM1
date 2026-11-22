// SPDX-License-Identifier: GPL-3.0-only
// Listening test (Romain 22/11): what RING MOD and WAVE FOLDING would do to each engine, before any of it is coded.
//   node tools/alter/alter_rmfold_wav.mjs build/emu/felucca.wasm OUTDIR
// Each chosen factory sound alone on circle 1 (4/16, 120 BPM, 2 bars, delay / reverb / chorus / DIST / INSERT off),
// then the same audio through, offline: a ring modulator whose carrier follows the note played (x 0.5, x 1.5,
// x 2.71) or stays at 700 Hz, and a sine wave folder (light, medium, hard). OUTDIR/NAME__VARIANT.wav + list.json.
// A sketch of what the FM-1 could do (an output stage per voice), not the firmware itself: levels matched by RMS.
import fs from "node:fs";

const [wasmPath, out] = process.argv.slice(2);
const FS = 44100, wasm = fs.readFileSync(wasmPath);
const ENG = ["ALTER", "PHYS", "NOISE", "RESO", "PLATE", "CORDE", "RING"];
const PICK = [[0, "THUD"], [2, "STAB"], [22, "KICK"], [7, "GONG"], [10, "BUZZ"], [13, "SNARE"], [14, "METAL"],
  [23, "ZAP"], [28, "GROWL"], [29, "GONG"], [30, "TOLE"], [33, "WIRE"], [34, "SITAR"], [37, "RADIO"], [38, "RING BELL"]];
const P_DIST = 33, P_CHOR = 34, P_DLY = 35, P_REV = 36, P_ITYPE = 96;
fs.mkdirSync(out, { recursive: true });

async function dry(si) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const L = [], R = [], N = [];
  const render = (n, keep) => {
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      if (!keep) continue;
      const a = new Float32Array(ex.memory.buffer, ex.web_out_l(), m), b = new Float32Array(ex.memory.buffer, ex.web_out_r(), m);
      const note = ex.web_alter_peek(0, 15);
      for (let i = 0; i < m; i++) { L.push(a[i]); R.push(b[i]); N.push(note); }
    }
  };
  render(FS * 2);
  ex.web_alter(18, 1, 0);                                       // (RUMBLE off)
  ex.web_alter(4, 120, 0);
  ex.web_alter(0, 0, si);
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);                 // (circles 2 and 3 muted)
  ex.web_alter(5, 0, 0);
  for (const p of [P_DIST, P_CHOR, P_DLY, P_REV, P_ITYPE]) ex.web_alter(17, 0 | p << 8, 0);
  render(2000);
  ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
  render(FS * 4 + FS / 2, true);
  return { L, R, N };
}
const hz = (n) => 440 * Math.pow(2, (n - 69) / 12);
const rms = (a) => Math.sqrt(a.reduce((p, x) => p + x * x, 0) / a.length);
function ring(d, ratio, fixed) {
  let ph = 0;
  const L = [], R = [];
  for (let i = 0; i < d.L.length; i++) {
    const f = fixed || hz(d.N[i] || 36) * ratio;
    ph += 2 * Math.PI * f / FS;
    const c = Math.cos(ph) * Math.SQRT2;
    L.push(d.L[i] * c); R.push(d.R[i] * c);
  }
  return { L, R };
}
function fold(d, g) {
  let pk = 0;
  for (const x of d.L) pk = Math.max(pk, Math.abs(x));
  pk = pk || 1;
  const f = (x) => Math.sin(Math.PI / 2 * g * x / pk) * pk;
  return { L: d.L.map(f), R: d.R.map(f) };
}
function match(v, d) {                                          // the same RMS as the dry sound
  const k = rms(d.L) / (rms(v.L) || 1);
  return { L: v.L.map((x) => x * k), R: v.R.map((x) => x * k) };
}
function wav(path, { L, R }) {
  const n = L.length, b = Buffer.alloc(44 + n * 4);
  b.write("RIFF", 0); b.writeUInt32LE(36 + n * 4, 4); b.write("WAVEfmt ", 8); b.writeUInt32LE(16, 16);
  b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22); b.writeUInt32LE(FS, 24); b.writeUInt32LE(FS * 4, 28);
  b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write("data", 36); b.writeUInt32LE(n * 4, 40);
  const q = (x) => Math.max(-32767, Math.min(32767, Math.round(x * 32767)));
  for (let i = 0; i < n; i++) { b.writeInt16LE(q(L[i]), 44 + i * 4); b.writeInt16LE(q(R[i]), 46 + i * 4); }
  fs.writeFileSync(path, b);
}
const VAR = [
  ["DRY", (d) => d],
  ["RING x0.5", (d) => match(ring(d, 0.5), d)],
  ["RING x1.5", (d) => match(ring(d, 1.5), d)],
  ["RING x2.71", (d) => match(ring(d, 2.71), d)],
  ["RING 700Hz", (d) => match(ring(d, 0, 700), d)],
  ["FOLD light", (d) => match(fold(d, 1.6), d)],
  ["FOLD medium", (d) => match(fold(d, 3.2), d)],
  ["FOLD hard", (d) => match(fold(d, 7), d)],
];
const list = [];
for (const [si, name] of PICK) {
  const d = await dry(si);
  const eng = ENG[[0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 0, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6][si]];
  const row = { eng, name, files: [] };
  for (const [vn, fn] of VAR) {
    const file = `${eng}_${name}__${vn}`.replace(/[ .]/g, "_");
    wav(`${out}/${file}.wav`, fn(d));
    row.files.push({ v: vn, file });
  }
  list.push(row);
  console.log(`${eng} ${name}: notes ${[...new Set(d.N)].join(",")}`);
}
fs.writeFileSync(`${out}/list.json`, JSON.stringify(list, null, 1));
