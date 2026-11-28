// SPDX-License-Identifier: GPL-3.0-only
// v1.2.20 (Romain 28/11): (1) "coupe nette": in PERC / STAB a hit cuts what still sounds and starts afresh (its whole
// envelope, the engine's sweep): hits 16/16 sound like a lone hit; (2) FREE: no 4-step limit, held until the next hit,
// then its own RELEASE (the PAD's tail heard), the tails ring under the next hits; (3) a sound picked by hand brings what
// it asks by default (op 38: its mode, its effects, its chord); (4) no click added.
//   node tools/alter/alter_env_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), FS = 44100;
const names = [...fs.readFileSync(new URL("../../firmware/src/alter_sounds.c", import.meta.url), "utf8")
  .matchAll(/^\s*\{"([A-Z ]+)", ([0-9]), \{/gm)].map((m) => m[1]);
const idx = (n) => names.indexOf(n);
let fails = 0;
const check = (w, ok) => { console.log(`env: ${w.padEnd(92)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports; ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  ex.web_alter(18, 1, 0);
  const render = (n, keep) => { const L = []; for (let k = 0; k < n; k += 128) { const m = Math.min(128, n - k); ex.web_render(m);
    if (keep) { const a = new Float32Array(ex.memory.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) L.push(a[i]); } } return L; };
  render(FS * 2); ex.web_alter(24, 0, 2); render(FS / 2); ex.web_alter(4, 130, 0);
  return { ex, render };
}
const STEP = FS * 60 / 130 / 4;
async function hits(snd, mode, n, secs = 0.7) {
  const { ex, render } = await boot();
  ex.web_alter(0, 0, idx(snd)); ex.web_alter(3, 0, mode);
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1); ex.web_alter(21, 0, 16); ex.web_alter(29, 0, n);
  render(FS / 4); ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
  return render(Math.round(FS * secs), true);
}
const onset = (L) => { for (let i = 0; i < L.length; i++) if (Math.abs(L[i]) > 0.01) return i; return 0; };
const like = (a, o0, b, base) => { const W = Math.round(FS * 0.03); let best = -9;
  for (let d = -40; d <= 160; d++) { let e = 0, en = 0; for (let i = 0; i < W; i++) { const x = a[o0 + i], y = b[base + d + i] || 0; e += (x - y) ** 2; en += x * x; }
    best = Math.max(best, 1 - e / en); } return best; };
const jump = (L) => { let m = 0; for (let i = 1; i < L.length; i++) m = Math.max(m, Math.abs(L[i] - L[i - 1])); return m; };
for (const [snd, mode] of [["STAB", 2], ["KICK", 1]]) {
  const a = await hits(snd, mode, 1), b = await hits(snd, mode, 16), o = onset(a);
  const l = [1, 2, 3].map((k) => like(a, o, b, Math.round(o + k * STEP)));
  check(`${snd} 16/16: hits 2-4 like a lone hit (${l.map((x) => x.toFixed(2)).join(" ")}), no click (${jump(b).toFixed(3)} / lone ${jump(a).toFixed(3)})`,
        l.every((x) => x > 0.9) && jump(b) <= jump(a) * 1.5 + 0.002);
}
{
  const a = await hits("PAD", 0, 1, 2.2);                        // one hit a bar (1.85 s): rings to the next
  const e = (t0, t1) => { let s = 0; const i0 = Math.round(t0 * FS), i1 = Math.round(t1 * FS); for (let i = i0; i < i1; i++) s += a[i] * a[i]; return 10 * Math.log10(s / (i1 - i0) + 1e-20); };
  const early = e(0.2, 0.4), mid = e(1.0, 1.2), late = e(1.6, 1.8);
  check(`FREE PAD: held past 4 steps (${early.toFixed(0)} dB at 0.3 s, ${mid.toFixed(0)} at 1.1 s, ${late.toFixed(0)} at 1.7 s)`, mid > early - 12 && late > -90);
}
{
  const { ex, render } = await boot();
  const P = (c, p) => ex.web_alter_peek(c, 100 + p);
  ex.web_alter(38, 0, idx("PAD")); render(4000);
  const pad = [ex.web_alter_peek(0, 1), P(0, 35), P(0, 36), ex.web_alter_peek(0, 37)];
  ex.web_alter(38, 1, idx("KICK")); render(4000);
  const kick = [P(1, 33), ex.web_alter_peek(1, 37)];
  ex.web_alter(38, 2, idx("GRIND")); render(4000);
  const grind = [P(2, 34), ex.web_alter_peek(2, 37)];
  ex.web_alter(0, 1, idx("KICK")); render(4000);
  const kick0 = P(1, 33);
  check(`picked by hand: PAD triad + DLY/REV 127 (${pad.join(",")}), KICK DIST 30 PERC (${kick.join(",")}), GRIND CHORUS 50 STAB (${grind.join(",")}); a scene's load: none (${kick0})`,
        pad[0] === 1 && pad[1] === 127 && pad[2] === 127 && pad[3] === 0 && kick[0] === 30 && kick[1] === 1 && grind[0] === 50 && grind[1] === 2 && kick0 === 0);
}
console.log(fails ? `env: ${fails} FAIL` : "env: all ok");
process.exit(fails ? 1 : 0);
