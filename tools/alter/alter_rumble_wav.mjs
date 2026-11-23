// SPDX-License-Identifier: GPL-3.0-only
// RUMBLE listening test (Romain 23/11: "le RUMBLE n'est pas super pour le kick: le refaire, 2-3 variantes à écouter").
//   node tools/alter/alter_rumble_wav.mjs build/emu/felucca.wasm OUTDIR
// The factory KICK alone on circle 1 (4 on the floor, 130 BPM, 4 bars, no send, no DIST), rendered by the emulator: dry,
// then with today's RUMBLE (the firmware, 70 and 127); then three new rumbles made offline from the same dry kick
// (sketches of what the firmware would do, all in plain filters a pi32v2 can run):
//   B "DARK ROOM": a long dark reverb (~2.5 s) driven hard, low-passed at 110 Hz, pumped by the kick (back up just before
//     the next one): the classic Berlin rumble.
//   C "ROLL": a sub sine at the kick's own pitch, swelling after each hit and gone before the next, saturated: a rolling
//     off-beat bass, in tune with the kick.
//   D "GROWL": brown noise through a resonant low-pass at the kick's pitch, driven, pumped: a grinding, unpitched floor.
// Each at MID and HIGH. OUTDIR/NAME.wav + list.json.
import fs from "node:fs";

const [wasmPath, out] = process.argv.slice(2);
const FS = 44100, BPM = 130, BEAT = Math.round(FS * 60 / BPM), wasm = fs.readFileSync(wasmPath);
fs.mkdirSync(out, { recursive: true });
const P_DIST = 33, P_CHOR = 34, P_DLY = 35, P_REV = 36, P_ITYPE = 96, KICK = 22;

async function render(rumble) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const L = [], R = [];
  const run = (n, keep) => {
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      if (!keep) continue;
      const a = new Float32Array(ex.memory.buffer, ex.web_out_l(), m), b = new Float32Array(ex.memory.buffer, ex.web_out_r(), m);
      for (let i = 0; i < m; i++) { L.push(a[i]); R.push(b[i]); }
    }
  };
  run(FS * 2);
  ex.web_alter(18, rumble ? 0 : 1, 0);
  ex.web_alter(4, BPM, 0);
  ex.web_alter(0, 0, KICK);
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
  for (const p of [P_DIST, P_CHOR, P_DLY, P_REV, P_ITYPE]) ex.web_alter(17, 0 | p << 8, 0);
  ex.web_alter(17, 0 | 255 << 8, rumble);
  run(2000);
  ex.web_buttons(1 << 10); run(128); ex.web_buttons(0);
  run(BEAT * 16 + FS, true);
  return { L, R };
}
const rms = (a) => Math.sqrt(a.reduce((p, x) => p + x * x, 0) / a.length);
function hits(x) {                                              // the kick's onsets
  const on = []; let env = 0, last = -BEAT;
  for (let i = 0; i < x.length; i++) {
    const a = Math.abs(x[i]); env = Math.max(a, env * 0.999);
    if (a > 0.05 && i - last > BEAT / 2 && (i < 64 || Math.abs(x[i - 64]) < a * 0.3)) { on.push(i); last = i; }
  }
  return on;
}
function pitch(x, at) {                                         // the kick's tail pitch: zero crossings 80..250 ms after a hit
  let z = 0, p = x[at + 3528];
  for (let i = at + 3528; i < at + 11025; i++) { if ((x[i] >= 0) !== (p >= 0)) z++; p = x[i]; }
  return z / 2 / (7497 / FS);
}
const lp1 = (f) => 1 - Math.exp(-2 * Math.PI * f / FS);
function pump(n, on, depth) {                                   // 1 between hits, pulled under each hit, back up before the next
  const g = new Float32Array(n).fill(1);
  for (const t of on) for (let i = 0; i < BEAT && t + i < n; i++) {
    const ph = i / BEAT;
    g[t + i] = 1 - depth * (ph < 0.08 ? 1 : Math.max(0, 1 - (ph - 0.08) / 0.55) ** 2);
  }
  return g;
}
function darkRoom(d, on, amt) {
  const x = d.L, n = x.length, y = new Float32Array(n);
  const lens = [1557, 1617, 1491, 1422, 1277, 1356].map((l) => l * 3), fb = 0.93, st = lens.map((l) => ({ b: new Float32Array(l), w: 0, lp: 0 }));
  for (let i = 0; i < n; i++) {
    let s = 0;
    for (const c of st) { const v = c.b[c.w]; c.lp += (v - c.lp) * 0.25; c.b[c.w] = x[i] + c.lp * fb; c.w = (c.w + 1) % c.b.length; s += v; }
    y[i] = s / 6;
  }
  const k1 = lp1(110); let a = 0, b = 0, h = 0;
  const g = pump(n, on, 0.95), drv = 4 + amt * 8;
  const r = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    let v = Math.tanh(y[i] * drv) / Math.tanh(drv);
    a += (v - a) * k1; b += (a - b) * k1; h += (b - h) * lp1(30);
    r[i] = (b - h) * g[i];
  }
  return r;
}
function roll(d, on, f0, amt) {
  const n = d.L.length, r = new Float32Array(n);
  let ph = 0;
  for (const t of on) for (let i = 0; i < BEAT && t + i < n; i++) {
    const p = i / BEAT, env = p < 0.25 ? (p / 0.25) ** 2 : Math.max(0, 1 - (p - 0.25) / 0.65) ** 1.5;
    const f = f0 * (1 + 0.04 * Math.exp(-p * 8));
    ph += 2 * Math.PI * f / FS;
    r[t + i] = Math.tanh(Math.sin(ph) * (1.5 + amt * 2.5)) * env;
  }
  const k = lp1(160); let a = 0;
  for (let i = 0; i < n; i++) { a += (r[i] - a) * k; r[i] = a; }
  return r;
}
function growl(d, on, f0, amt) {
  const n = d.L.length, r = new Float32Array(n), g = pump(n, on, 0.97);
  let br = 0, s1 = 0, s2 = 0, seed = 12345;
  const fq = 2 * Math.sin(Math.PI * f0 / FS), q = 0.08;
  for (let i = 0; i < n; i++) {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    br = br * 0.995 + ((seed / 4294967296) - 0.5) * 0.1;
    s2 += fq * s1; const hp = br - s2 - q * s1; s1 += fq * hp;    // (a state-variable low-pass)
    r[i] = Math.tanh(s2 * (3 + amt * 6)) * g[i];
  }
  return r;
}
function mix(d, r, level) {                                     // the rumble under the kick, at level x the kick's RMS
  const k = level * rms(d.L) / (rms(r) || 1);
  return { L: d.L.map((x, i) => x + r[i] * k), R: d.R.map((x, i) => x + r[i] * k) };
}
function wav(path, { L, R }) {
  let pk = 0; for (const x of L) pk = Math.max(pk, Math.abs(x)); for (const x of R) pk = Math.max(pk, Math.abs(x));
  const sc = pk > 0.98 ? 0.98 / pk : 1;
  const n = L.length, b = Buffer.alloc(44 + n * 4);
  b.write("RIFF", 0); b.writeUInt32LE(36 + n * 4, 4); b.write("WAVEfmt ", 8); b.writeUInt32LE(16, 16);
  b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22); b.writeUInt32LE(FS, 24); b.writeUInt32LE(FS * 4, 28);
  b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write("data", 36); b.writeUInt32LE(n * 4, 40);
  const q = (x) => Math.max(-32767, Math.min(32767, Math.round(x * sc * 32767)));
  for (let i = 0; i < n; i++) { b.writeInt16LE(q(L[i]), 44 + i * 4); b.writeInt16LE(q(R[i]), 46 + i * 4); }
  fs.writeFileSync(path, b);
}
const dry = await render(0), on = hits(dry.L), f0 = on.length ? pitch(dry.L, on[1] || on[0]) : 55;
console.log(`hits ${on.length}, kick pitch ~${f0.toFixed(1)} Hz`);
const list = [];
const put = (name, label, s) => { wav(`${out}/${name}.wav`, s); list.push({ name, label }); };
put("0_DRY", "KICK sec", dry);
put("A_NOW_70", "Actuel 70", await render(70));
put("A_NOW_127", "Actuel 127", await render(127));
for (const [lv, tag] of [[0.45, "MID"], [0.8, "HIGH"]]) {
  put(`B_DARK_${tag}`, `DARK ROOM ${tag}`, mix(dry, darkRoom(dry, on, lv), lv));
  put(`C_ROLL_${tag}`, `ROLL ${tag}`, mix(dry, roll(dry, on, f0, lv), lv));
  put(`D_GROWL_${tag}`, `GROWL ${tag}`, mix(dry, growl(dry, on, f0, lv), lv));
}
fs.writeFileSync(`${out}/list.json`, JSON.stringify(list, null, 1));
