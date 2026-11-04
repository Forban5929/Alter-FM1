// SPDX-License-Identifier: GPL-3.0-only
// ALTER's FX page and MASTER block in the browser build (alter_fx.c, alter_fxpage.c):
//   node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm
// FX held opens the page (FX tapped closes it), PRESETS changes the tab's preset, the master's EQ, COMP, CLIP and
// DUCK act on the sound, a USER scene keeps the effects.
import fs from "node:fs";

const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
const FXB = 0, PLAY = 10, SAVEB = 7, FS = 44100, BPM = 120, STEP = Math.floor(FS * 60 / BPM / 4), BEAT = STEP * 4;
let fails = 0;
const check = (what, ok) => { console.log(`fx: ${what.padEnd(78)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n, keep) => {
  const got = [];
  for (let k = 0; k < n; k += 128) {
    const m = Math.min(128, n - k);
    ex.web_render(m);
    if (keep) { const l = new Float32Array(mem.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) got.push(l[i]); }
  }
  return got;
};
const enc = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); render(1300); } };
const B = (n, hold = 2000) => { ex.web_buttons(1 << n); render(hold); ex.web_buttons(0); render(2000); };
const scr = () => { render(4000); return Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)).join(); };
const rms = (a) => Math.sqrt(a.reduce((p, x) => p + x * x, 0) / Math.max(1, a.length));
const peak = (a) => a.reduce((p, x) => Math.max(p, Math.abs(x)), 0);
const W = (op, a, b) => ex.web_alter(op, a, b);

render(FS * 2);
// the page: FX held 0.7 s opens it, it stays; PRESETS moves the tab's preset (the screen shows it); FX tapped closes
{
  const home = scr();
  B(FXB, FS * 0.7);
  const page = scr();
  enc(1, 3);                                                     // ALGORITHM: to MASTER
  const m0 = scr();
  enc(2, 1);                                                     // PRESETS: the next master preset
  const m1 = scr();
  B(FXB);
  render(FS * 1.7);                                              // (the middle's word gone)
  const back = scr();
  check("FX held: the FX page stays open; ALGORITHM + PRESETS change it; FX tapped: the circles",
        page !== home && m0 !== page && m1 !== m0 && back === home);
  B(FXB, FS * 0.7); enc(1, -3); B(FXB);                          // (back to its DIST tab, closed)
  W(7, 5, 0);                                                    // (MASTER FLAT)
}
W(4, BPM, 0);
ex.web_buttons(1 << PLAY); render(128); ex.web_buttons(0);
render(FS);
const solo = (c) => { for (let i = 0; i < 3; i++) W(2, i, i === c || c < 0 ? 0 : 1); };
// EQ: LOW 0 takes the lows away (under ~100 Hz), HIGH 0 the highs (over ~6 kHz), MID 0 much of the rest; all three
// 0: silence
{
  const lp = (a, f) => { const k = 1 - Math.exp(-2 * Math.PI * f / FS); let y = 0; return a.map((x) => (y += (x - y) * k)); };
  const hp = (a, f) => { const b = lp(a, f); return a.map((x, i) => x - b[i]); };   // (4 poles each: the bands' edges sharp)
  const lows = (a) => rms(lp(lp(lp(lp(a, 100), 100), 100), 100)), highs = (a) => rms(hp(hp(hp(hp(a, 6000), 6000), 6000), 6000));
  solo(-1); render(FS);
  const a = render(BEAT * 8, true);
  W(8, 0, 0); render(FS); const lo = render(BEAT * 8, true); W(8, 0, 64);
  W(8, 2, 0); render(FS); const hi = render(BEAT * 8, true); W(8, 2, 64);
  solo(1); render(FS); const am = render(BEAT * 8, true);         // (MID: on the chords, circle 2; the kick is mostly low)
  W(8, 1, 0); render(FS); const mi = render(BEAT * 8, true); W(8, 1, 64); solo(-1);
  for (let b = 0; b < 3; b++) W(8, b, 0);
  render(FS);
  const z = peak(render(BEAT * 4, true));
  for (let b = 0; b < 3; b++) W(8, b, 64);
  render(FS);
  check(`EQ LOW 0: lows ${lows(a).toFixed(4)} -> ${lows(lo).toFixed(4)}`, lows(lo) < lows(a) * 0.4);
  check(`EQ HIGH 0: highs ${highs(a).toFixed(4)} -> ${highs(hi).toFixed(4)}`, highs(hi) < highs(a) * 0.4);
  check(`EQ MID 0 (the chords): rms ${rms(am).toFixed(4)} -> ${rms(mi).toFixed(4)}; all three 0: silence (${z.toFixed(5)})`, rms(mi) < rms(am) * 0.8 && z < 0.0005);
}
// COMP 127 on the whole mix: the peaks come nearer the average (a lower crest factor)
{
  solo(-1); render(BEAT * 2);
  const a = render(BEAT * 8, true), ca = peak(a) / rms(a);
  W(8, 3, 127); render(FS);
  const b = render(BEAT * 8, true), cb = peak(b) / rms(b);
  W(8, 3, 0);
  check(`COMP 127: crest factor down (${ca.toFixed(2)} -> ${cb.toFixed(2)})`, cb < ca * 0.9);
}
// CLIP 127: nothing over half scale (its top), louder on average
{
  const a = render(BEAT * 8, true);
  W(8, 7, 127); render(FS);
  const b = render(BEAT * 8, true);
  W(8, 7, 0);
  check(`CLIP 127: peak under its top (${peak(a).toFixed(3)} -> ${peak(b).toFixed(3)}), rms up`, peak(b) < 0.52 && rms(b) > rms(a));
}
// DUCK on the beat: circle 3 alone, just after each beat much quieter than late in it; off: no such dip
const dip = () => {
  render(BEAT * 2);
  const a = render(BEAT * 8, true);
  let early = 0, late = 0;
  for (let b = 0; b < 8; b++) {
    // (where the beats fall is not known here: the quietest and the loudest 50 ms of each beat)
    const seg = a.slice(b * BEAT, (b + 1) * BEAT), win = [];
    for (let k = 0; k + 2205 <= seg.length; k += 1102) win.push(rms(seg.slice(k, k + 2205)));
    early += Math.min(...win); late += Math.max(...win);
  }
  return early / late;
};
{
  W(7, 2, 0); solo(1);                                           // circle 2 alone (HOUSE-like stab tails), its sound
  W(0, 1, 7); W(3, 1, 0); W(15, 1, 16 | 16 << 8);               // (circle 2: GONG FREE on every step, held full: a steady
  for (const [id, v] of [[1, 0], [2, 127], [3, 127], [4, 60]]) {  //  sound; DRONE retired 04/11, FREE notes stop in 4 steps)
    const b = [0xf0, 0x7d, 0x41, 0x4c, 4, 1, id, (v + 8192) & 127, ((v + 8192) >> 7) & 127, 0xf7];
    new Uint8Array(mem.buffer, ex.web_link_in_buf(), b.length).set(b); ex.web_link_in(b.length); ex.web_link_out_clear();
  }
  render(BEAT * 4);
  const off = dip();
  W(8, 5, 0); W(8, 6, 90); W(8, 4, 127); render(BEAT * 2);       // SRC 1/4, SHAPE, DUCK 127
  const on = dip();
  W(8, 4, 0);
  check(`DUCK 127 on 1/4: each beat dips (quiet/loud ${off.toFixed(2)} -> ${on.toFixed(2)})`, on < off * 0.75);
}
// a USER scene keeps the effects: MASTER PUMP 1/4 stored in USER 01, FLAT, USER 01 loaded: it pumps as before
{
  const row = (n) => { enc(0, -25); enc(0, n); };
  W(7, 5, 4); W(8, 4, 127); render(FS);                          // MASTER: PUMP 1/4, DUCK 127
  const ref = dip();
  B(SAVEB); row(5); B(SAVEB, FS * 1.2); B(6);                    // USER 01: SAVE held 1.2 s; HOME
  W(7, 5, 0); render(FS);
  const flat = dip();
  B(SAVEB); row(5); B(SAVEB);                                    // USER 01 loaded
  W(4, BPM, 0); solo(1); render(FS);
  const back = dip();
  check(`USER 01 keeps the MASTER (pump ${ref.toFixed(2)}, FLAT ${flat.toFixed(2)}, loaded ${back.toFixed(2)})`,
        Math.abs(back - ref) < 0.06 && flat - back > 0.08);
}
// the MASTER FILTER (alter_filter.c): off = untouched; LP takes the highs, HP the lows; DRIVE keeps the loudness;
// RES past 1 screams on silence at the cutoff, held to about the mix's level; a USER scene keeps it, a factory one has none
{
  const F = (k, v) => W(12, k, v), FA = (a) => a.forEach((v, k) => F(k, v));
  const lp = (a, f) => { const k = 1 - Math.exp(-2 * Math.PI * f / FS); let y = 0; return a.map((x) => (y += (x - y) * k)); };
  const hp = (a, f) => { const b = lp(a, f); return a.map((x, i) => x - b[i]); };
  const lows = (a) => rms(lp(lp(lp(a, 150), 150), 150)), highs = (a) => rms(hp(hp(hp(a, 4000), 4000), 4000));
  const db = (x) => 20 * Math.log10(x + 1e-9);
  W(1, 0, 0); W(4, BPM, 0); solo(-1); render(FS * 2);
  const take = () => { render(FS); return render(FS * 3, true); };
  const off = take();
  FA([127, 0, 0, 0, 64, 0, 0, 127, 0, 64, 64]);                  // on, open: LP 18 kHz, no drive
  const open = take();
  check(`FILTER open: the level kept (${db(rms(off)).toFixed(1)} -> ${db(rms(open)).toFixed(1)} dB)`,
        Math.abs(db(rms(open)) - db(rms(off))) < 1.5);
  F(0, 60); const low = take();                                  // LP ~450 Hz
  check(`FILTER LP 450 Hz: the highs go (${db(highs(open)).toFixed(1)} -> ${db(highs(low)).toFixed(1)} dB)`,
        db(highs(open)) - db(highs(low)) > 20);
  F(0, 73); F(2, 127); const hi = take(); F(2, 0); F(0, 127);    // HP 1 kHz
  check(`FILTER HP 1 kHz: the lows go (${db(lows(open)).toFixed(1)} -> ${db(lows(hi)).toFixed(1)} dB)`,
        db(lows(open)) - db(lows(hi)) > 20);
  F(3, 127); const drv = take(); F(3, 0);
  check(`FILTER DRIVE 127: the loudness kept (${db(rms(drv)).toFixed(1)} dB)`, Math.abs(db(rms(drv)) - db(rms(open))) < 3);
  FA([60, 0, 0, 0, 64, 0, 0, 127, 0, 64, 64]);                   // A: LP 450 Hz ..
  [127, 0, 0, 0, 64, 0, 0, 127, 0, 64, 64].forEach((v, k) => F(11 + k, v));   // .. B: open
  F(22, 0); const ma = take(); F(22, 127); const mb = take(); F(22, 0);
  check(`FILTER MORPH A -> B: from closed to open (highs ${db(highs(ma)).toFixed(1)} -> ${db(highs(mb)).toFixed(1)} dB)`,
        db(highs(mb)) - db(highs(ma)) > 20);
  [127, 0, 0, 0, 64, 0, 0, 0, 0, 64, 64].forEach((v, k) => F(11 + k, v));    // (B off again)
  for (let c = 0; c < 3; c++) W(2, c, 1);                        // silence
  FA([73, 127, 0, 0, 64, 0, 0, 127, 0, 64, 64]); render(FS * 2);
  const osc = render(FS, true); let zc = 0;
  for (let i = 1; i < osc.length; i++) if (osc[i - 1] < 0 && osc[i] >= 0) zc++;
  check(`FILTER RES 127 on silence: it screams at the cutoff (${zc} Hz, ${db(rms(osc)).toFixed(1)} dB)`,
        zc > 850 && zc < 1150 && db(rms(osc)) > -40 && db(rms(osc)) < db(rms(open)) + 6);
  solo(-1);
  W(13, 0, 2); F(22, 0); render(FS);                             // TAPE into A (B too: it was off)
  const row = (n) => { enc(0, -25); enc(0, n); };
  B(SAVEB); row(6); B(SAVEB, FS * 1.2); B(6);                    // USER 02: SAVE held 1.2 s; HOME
  W(1, 0, 0); render(FS);
  const fac = ex.web_alter_flt(1);                               // a factory scene: none (-1)
  B(SAVEB); row(6); B(SAVEB); render(FS);                        // USER 02 loaded
  const back = ex.web_alter_flt(1);
  check(`FILTER: a factory scene has none (${fac}), USER 02 brings it back (${back})`, fac === -1 && back >= 0);
  W(1, 0, 0); render(FS);
}
console.log(fails ? `fx: ${fails} FAIL` : "fx: all ok");
process.exit(fails ? 1 : 0);
