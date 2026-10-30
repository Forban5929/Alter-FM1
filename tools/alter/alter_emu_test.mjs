// SPDX-License-Identifier: GPL-3.0-only
// ALTER in the browser build, in Node as the page's worklet runs it:
//   node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm [out.wav]
// Boots, presses PLAY, and listens to each sixteenth of 4 bars: the steps the default circles hit
// (alter_seq.c as_init: HITS 4/16, GHOST 3/16 turned by 2, ACCENT 2/16) must sound, the others stay quiet,
// and the loudness orders accent > hit > ghost. Writes the 4 bars as a WAV to listen to.
import fs from "node:fs";

const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), wavPath = process.argv[3];
const PLAY = 10, FS = 44100, BPM = 120;
let fails = 0;
const check = (what, ok) => { console.log(`alter: ${what.padEnd(70)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const out = [];
const render = (n, keep) => {
  for (let k = 0; k < n; k += 128) {
    const m = Math.min(128, n - k);
    ex.web_render(m);
    if (keep) { const l = new Float32Array(mem.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) out.push(l[i]); }
  }
};
render(FS * 2, false);                                         // the splash
ex.web_buttons(1 << PLAY); render(128, true); ex.web_buttons(0);
const step = FS * 60 / BPM / 4, total = Math.round(step * 64);
render(total, true);

// the euclidean patterns as alter_seq.c draws them
const euclid = (n, k, r) => { const p = new Set(); for (let i = 0; i < n; i++) if ((i * k) % n < k) p.add((i + r) % n); return p; };
const H = euclid(16, 4, 0), G = euclid(16, 3, 2), A = euclid(16, 2, 0);
// the peak in the first 30 ms after each step, against the sound's onset (the first sample above a floor)
const onset = out.findIndex((x) => Math.abs(x) > 0.002);
check(`PLAY starts ALTER (first sound ${(onset / FS * 1000).toFixed(0)} ms after the press)`, onset >= 0 && onset < FS * 0.1);
const lvl = (s) => { let p = 0; const a = Math.round(onset + s * step); for (let i = a; i < a + FS * 0.03; i++) p = Math.max(p, Math.abs(out[i] - (out[i - 1] || 0))); return p; };
const kinds = { accent: [], hit: [], ghost: [], rest: [] };
for (let s = 0; s < 64; s++) {
  const i = s % 16, k = H.has(i) ? (A.has(i) ? "accent" : "hit") : G.has(i) ? (A.has(i) ? "accent" : "ghost") : "rest";
  kinds[k].push(lvl(s));
}
const avg = (a) => a.reduce((x, y) => x + y, 0) / (a.length || 1), min = (a) => Math.min(...a), max = (a) => Math.max(...a);
console.log(`alter: attack (sample step) accent ${avg(kinds.accent).toFixed(4)} hit ${avg(kinds.hit).toFixed(4)} ghost ${avg(kinds.ghost).toFixed(4)} rest ${avg(kinds.rest).toFixed(4)}`);
check("every hit, ghost and accent step sounds (attack over 3 x the quietest rest)", min([...kinds.accent, ...kinds.hit, ...kinds.ghost]) > 3 * Math.max(1e-4, min(kinds.rest)));
check("accent > hit > ghost", avg(kinds.accent) > avg(kinds.hit) && avg(kinds.hit) > avg(kinds.ghost));
check("no step quieter than a rest is counted as a hit (rests stay below the softest hit)", max(kinds.rest) < min([...kinds.hit, ...kinds.accent]));
const peak = out.reduce((p, x) => Math.max(p, Math.abs(x)), 0);
check(`level sane (peak ${peak.toFixed(3)}, no clipping)`, peak > 0.02 && peak < 0.999);
ex.web_buttons(1 << PLAY); render(128, false); ex.web_buttons(0);
out.length = 0; render(FS, true);
const tail = out.slice(FS / 2).reduce((p, x) => Math.max(p, Math.abs(x)), 0);
check(`PLAY again stops it (last half second peak ${tail.toFixed(4)})`, tail < 0.002);

// ---- the PLAY view (alter_ui.c): its keys, SEQ + knobs
const SEQB = 9, K = (n) => ex.web_keys(n);
const onsets = (a) => {                                          // attacks: a jump in the signal, 30 ms apart at least
  let n = 0, last = -1e9;
  for (let i = 1; i < a.length; i++) if (Math.abs(a[i] - a[i - 1]) > 0.0015 && i - last > FS * 0.03) { n++; last = i; }
  return n;
};
const bar = Math.round(step * 16), listen = (n) => { out.length = 0; render(n, true); return out.slice(); };
K(1 << 26); const quiet = listen(FS / 2); K(0); render(FS / 2, false);
check("a key on the PLAY view plays no note", quiet.reduce((p, x) => Math.max(p, Math.abs(x)), 0) < 0.001);
ex.web_buttons(1 << PLAY); render(128, false); ex.web_buttons(0);
render(bar, false);
const base = onsets(listen(bar));
check(`the circles play (${base} attacks in a bar: HITS 4 + GHOST 3)`, base >= 6 && base <= 8);
K(1 << 3); const roll = onsets(listen(bar)); K(0);
check(`ROLL 1/32 held: ${roll} attacks in a bar (~32)`, roll >= 26);
render(bar, false);
K(1); render(120, false); K(0); K(1 << 2); render(120, false); K(0);       // MUTE HITS, MUTE GHOST
const muted = listen(bar).slice(FS / 4).reduce((p, x) => Math.max(p, Math.abs(x)), 0);
check(`MUTE HITS + GHOST: silence (ACCENT never plays alone; peak ${muted.toFixed(4)})`, muted < 0.002);
K(1); render(120, false); K(0); K(1 << 2); render(120, false); K(0);       // back on
render(300, false); ex.web_buttons(1 << SEQB); render(200, false);
for (let i = 0; i < 4; i++) { ex.web_enc(4, 1); render(30, false); }      // SEQ + KNOB 2: HITS 4 -> 8
ex.web_buttons(0); render(bar * 2, false);
const dense = onsets(listen(bar));
check(`SEQ + KNOB 2: HITS 4 -> 8 from the next cycle (${dense} attacks in a bar)`, dense >= 8 && dense <= 10);
K(1 << 7); render(120, false); K(0); render(bar * 2, false);               // DICE HITS: a draw
check("DICE HITS: still playing after a new draw", onsets(listen(bar)) >= 8);
{
  const before = onsets(listen(bar));
  const FILLS = { 1: "ROLL 1/16", 5: "ROLL 1/16T", 8: "RATCHET X2", 10: "RATCHET X3", 13: "DUB DELAY", 15: "DUB REVERB",
                  17: "DESTROY", 20: "DENSITY -", 22: "DENSITY +", 25: "REVERSE", 14: "SHIFT -1", 16: "SHIFT +1" };
  const got = {};
  let worst = 0;
  for (const [k, name] of Object.entries(FILLS)) {
    K(1 << +k); const a = listen(bar); K(0); render(bar * 2, false);
    got[name] = onsets(a);
    worst = Math.max(worst, a.reduce((p, x) => Math.max(p, Math.abs(x)), 0));
  }
  console.log("alter: attacks in a bar held: " + Object.entries(got).map(([n, v]) => `${n} ${v}`).join(", "));
  check(`every fill plays and none clips (peak ${worst.toFixed(3)})`, Object.values(got).every((v) => v > 0) && worst < 0.999);
  check("RATCHET X2 / X3 add attacks", got["RATCHET X2"] > before && got["RATCHET X3"] > got["RATCHET X2"]);
  check("DENSITY - < DENSITY +", got["DENSITY -"] < got["DENSITY +"]);
  const after = onsets(listen(bar));
  check(`let go: the pattern is back (${before} attacks before, ${after} after)`, after === before);
}
if (wavPath) {                                                  // 4 bars again from a fresh start, 16-bit mono
  ex.web_buttons(1 << PLAY); render(128, false); ex.web_buttons(0);
  out.length = 0; render(total + FS, true);
  const b = Buffer.alloc(44 + out.length * 2);
  b.write("RIFF", 0); b.writeUInt32LE(36 + out.length * 2, 4); b.write("WAVEfmt ", 8); b.writeUInt32LE(16, 16);
  b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22); b.writeUInt32LE(FS, 24); b.writeUInt32LE(FS * 2, 28);
  b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34); b.write("data", 36); b.writeUInt32LE(out.length * 2, 40);
  out.forEach((x, i) => b.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(x * 32767))), 44 + i * 2));
  fs.writeFileSync(wavPath, b);
  console.log(`alter: wrote ${wavPath}`);
}
console.log(fails ? `alter: ${fails} FAIL` : "alter: all ok");
process.exit(fails ? 1 : 0);
