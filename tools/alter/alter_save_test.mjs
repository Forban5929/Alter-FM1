// SPDX-License-Identifier: GPL-3.0-only
// A USER scene keeps what the dice / CHAOS / CHAOS TOTAL drew, TURING's pattern (frozen) and the PROGRESSION (10/11):
//   node tools/alter/alter_save_test.mjs build/emu/felucca.wasm
import fs from "node:fs";

const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`save: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const PLAY = 10;

async function boot(nor) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase();
  if (nor) new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()).set(nor);
  ex.web_boot();
  const t = {
    ex,
    w: (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); },
    tap: (b) => { ex.web_buttons(1 << b); t.w(60); ex.web_buttons(0); t.w(60); },
    peek: (c, w) => ex.web_alter_peek(c, w) >>> 0,
    rms: (ms) => {                       // (the output's level over ms, dBFS)
      let e = 0, n = 0;
      for (let k = 0, m = Math.round(ms * 44.1); k < m; k += 128) {
        const b = Math.min(128, m - k);
        ex.web_render(b);
        const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), b), r = new Float32Array(ex.memory.buffer, ex.web_out_r(), b);
        for (let i = 0; i < b; i++) e += l[i] * l[i] + r[i] * r[i], n += 2;
      }
      return 10 * Math.log10(e / n + 1e-12);
    },
    state: () => [0, 1, 2].map((c) => ({ h: t.peek(c, 13), tu: t.peek(c, 14), r: t.peek(c, 9) })),
  };
  t.w(2500);
  return t;
}

let t = await boot();
t.tap(PLAY); t.w(500);
let saved = null;
for (let tries = 0; tries < 30 && !saved; tries++) {   // (a CHAOS TOTAL with a TURING circle in it)
  t.ex.web_alter(22, 0, 0); t.w(1500);
  const st = t.state();
  if (st.some((s) => (s.r & 255) === 1) && st.some((s) => (s.r & 255) === 0)) saved = st;
}
check("a CHAOS TOTAL drew a TURING circle and a EUCLID one", !!saved);
const lvBefore = t.rms(6000);
const before = t.state();
const lens = before.map((s) => (s.r >> 8) & 255);
check(`CHAOS TOTAL: LEN 5..16 (${lens.join(" ")})`, lens.every((l) => l >= 5 && l <= 16));
t.ex.web_alter(23, 0, 0);
check("saved into USER A01", t.ex.web_alter_peek !== undefined);
t.ex.web_alter(24, 0, 2); t.w(800);                     // NEW SCENE
const fresh = t.state();
check("NEW SCENE: the draws gone", fresh.every((s, c) => s.h !== before[c].h));
t.ex.web_alter(24, 0, 0); t.w(400);                     // USER A01 back
const back = t.state();
for (let c = 0; c < 3; c++) {
  const tur = (before[c].r & 255) === 1;
  check(`circle ${c + 1}: the same LEN, type`, (back[c].r & 0xFFFF) === (before[c].r & 0xFFFF));
  if (tur) {
    check(`circle ${c + 1} (TURING): its pattern back, frozen`, (back[c].tu & 0xFFFF) === (before[c].tu & 0xFFFF) &&
          ((back[c].tu >> 16) & 1) === 1);
  } else
    check(`circle ${c + 1}: its draw and pattern back (hash)`, back[c].h === before[c].h);
}
const lvBack = t.rms(6000);
console.log(`save: level before / back ${lvBefore.toFixed(1)} / ${lvBack.toFixed(1)} dBFS (CHANCE coins: +-4 dB between two windows)`);
{                                                       // stored again from what came back: the same bytes
  t.ex.web_alter(23, 1, 0);
  const d = (u) => { const n = t.ex.web_alter_dump(u); return Array.from(new Uint8Array(t.ex.memory.buffer, t.ex.web_alter_dump_buf(), n)); };
  const a1 = d(0), a2 = d(1), diff = [];
  for (let i = 0; i < a1.length; i++) if (a1[i] !== a2[i]) diff.push(`${i}:${a1[i]}>${a2[i]}`);
  check(`stored again after the recall: the same scene (${diff.length ? diff.slice(0, 10).join(" ") : "all " + a1.length + " bytes"})`,
        diff.length === 0);
}
const later = t.state();
check("TURING frozen: still the same pattern 3 s later",
      before.every((s, c) => (s.r & 255) !== 1 || (later[c].tu & 0xFFFF) === (s.tu & 0xFFFF)));

// a scene stored without the draws (an older firmware's object) still loads: no draw, the rest as it was
t.ex.web_alter(25, 2, 0);
t.ex.web_alter(24, 0, 2); t.w(500); t.ex.web_alter(24, 2, 0); t.w(400);
const old = t.state();
check("an object stored before the draws: loads (LEN back, no draw)", old.every((s, c) => (s.r & 0xFFFF) === (later[c].r & 0xFFFF) &&
      ((s.tu >> 17) & 1) === 0));
// the PROGRESSION (SCL page knob 4): circle 1's lowest note each bar follows 1-5-6-4; OFF: the same every bar
{
  const bars = (prog) => {
    t.ex.web_alter(1, 0, 0); t.w(300);                  // factory scene 1
    t.ex.web_alter(26, 0, prog); t.w(100);
    const lo = {};
    const g0 = t.ex.web_gstep();
    for (let i = 0; i < 600; i++) {
      t.w(20);
      const b = Math.floor(t.ex.web_gstep() / 16), n = t.peek(0, 15);
      if (b > Math.floor(g0 / 16) && n) lo[b] = Math.min(lo[b] ?? 999, n);
      if (Object.keys(lo).length >= 5) break;
    }
    return Object.keys(lo).sort((a, b) => a - b).slice(0, 4).map((b) => lo[b]);
  };
  const off = bars(0), on = bars(1);
  check(`PROGRESSION OFF: the same lowest note every bar (${off.join(" ")})`, off.length === 4 && off.every((n) => n === off[0]));
  check(`PROGRESSION 1-5-6-4: it moves each bar (${on.join(" ")})`, on.length === 4 && new Set(on).size >= 3);
}
console.log(fails ? `save: ${fails} FAIL` : "save: all ok");
process.exit(fails ? 1 : 0);
