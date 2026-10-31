// SPDX-License-Identifier: GPL-3.0-only
// ALTER's sounds and scenes, played by the real firmware (the browser build) and written as WAV files:
//   node tools/alter/alter_render.mjs build/emu/felucca.wasm OUTDIR
// OUTDIR/wav/sound_NN_NAME.wav: each sound of alter_sounds.c ALT_PRE alone on circle 1 (its 4/16, the mode it is made for, 120 BPM,
// 4 bars); OUTDIR/wav/scene_N_NAME.wav: each scene (8 bars at its tempo). Prints each one's loudness (RMS of the
// loudest bar, dBFS) and peak, to set the sounds' LEVEL near one another.
import fs from "node:fs";

const [wasmPath, out] = process.argv.slice(2);
const FS = 44100, wasm = fs.readFileSync(wasmPath);
const src = fs.readFileSync(new URL("../../firmware/src/alter_sounds.c", import.meta.url), "utf8");
const sounds = [...src.matchAll(/^\s*\{"([A-Z]+)", ([012]), \{/gm)].map((m) => ({ name: m[1], eng: +m[2] }));
const scenes = [...src.matchAll(/^\s*\{"([A-Z]+)", "([^"]+)", (\d+),/gm)].map((m) => ({ name: m[1], about: m[2], bpm: +m[3] }));
const ENG = ["ALTER", "PHYS", "NOISE"];
fs.mkdirSync(`${out}/wav`, { recursive: true });

async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n) => {
    const l = [], r = [];
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      const a = new Float32Array(ex.memory.buffer, ex.web_out_l(), m), b = new Float32Array(ex.memory.buffer, ex.web_out_r(), m);
      for (let i = 0; i < m; i++) { l.push(a[i]); r.push(b[i]); }
    }
    return [l, r];
  };
  render(FS * 2);                                               // the splash: the view loads the first scene
  return { ex, render };
}
const play = (ex, render) => { ex.web_buttons(1 << 10); render(128); ex.web_buttons(0); };
function stats(l, bar) {
  let peak = 0, best = 0;
  for (let b = 0; b + bar <= l.length; b += bar) {
    let s = 0;
    for (let i = b; i < b + bar; i++) { s += l[i] * l[i]; peak = Math.max(peak, Math.abs(l[i])); }
    best = Math.max(best, s / bar);
  }
  return { rms: 10 * Math.log10(best + 1e-12), peak };
}
function wav(path, [l, r]) {
  const n = l.length, b = Buffer.alloc(44 + n * 4);
  b.write("RIFF", 0); b.writeUInt32LE(36 + n * 4, 4); b.write("WAVEfmt ", 8); b.writeUInt32LE(16, 16);
  b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22); b.writeUInt32LE(FS, 24); b.writeUInt32LE(FS * 4, 28);
  b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write("data", 36); b.writeUInt32LE(n * 4, 40);
  const q = (x) => Math.max(-32767, Math.min(32767, Math.round(x * 32767)));
  for (let i = 0; i < n; i++) { b.writeInt16LE(q(l[i]), 44 + i * 4); b.writeInt16LE(q(r[i]), 46 + i * 4); }
  fs.writeFileSync(path, b);
}

const rows = [];
for (let i = 0; i < sounds.length; i++) {
  const { ex, render } = await boot();
  ex.web_alter(4, 120, 0);
  ex.web_alter(0, 0, i);
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
  ex.web_alter(5, 0, 0);                                       // (the mode the sound is made for)
  render(2000);
  play(ex, render);
  const bar = Math.round(FS * 2), a = render(bar * 4 + FS);
  const st = stats(a[0].slice(0, bar * 4), bar);
  const name = `sound_${String(i + 1).padStart(2, "0")}_${ENG[sounds[i].eng]}_${sounds[i].name}`;
  wav(`${out}/wav/${name}.wav`, a);
  rows.push({ name, ...st });
}
for (let i = 0; i < scenes.length; i++) {
  const { ex, render } = await boot();
  ex.web_alter(1, i, 0);
  render(2000);
  play(ex, render);
  const bar = Math.round(FS * 240 / scenes[i].bpm), a = render(bar * 8 + FS);
  const st = stats(a[0].slice(0, bar * 8), bar);
  const name = `scene_${i + 1}_${scenes[i].name}`;
  wav(`${out}/wav/${name}.wav`, a);
  rows.push({ name, ...st });
}
for (const r of rows)
  console.log(`${r.name.padEnd(28)} loudest bar ${r.rms.toFixed(1).padStart(6)} dBFS RMS   peak ${r.peak.toFixed(3)}${r.peak >= 0.999 ? "  CLIPS" : ""}`);
