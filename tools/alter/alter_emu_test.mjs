// SPDX-License-Identifier: GPL-3.0-only
// ALTER in the browser build, in Node as the page's worklet runs it:
//   node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm [out.wav]
// Boots, presses PLAY and listens: the three circles (alter_seq.c as_init: 4/16, 3/16 turned by 2, 5/16 turned by
// 1) are three voices (parts 1-3: ALTER, PHYS, NOISE). Then the PLAY view (alter_ui.c): MUTE 1 2 3, SEQ + knob,
// DICE (a tap at once, held = clean), SHIFT, the fills on the selected circle only. Writes 4 bars as a WAV.
import fs from "node:fs";

const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), wavPath = process.argv[3];
const PLAY = 10, SEQB = 9, FS = 44100, BPM = 120, STEP = Math.floor(FS * 60 / BPM / 4), BAR = STEP * 16;   // (whole samples, as the firmware)
let fails = 0;
const check = (what, ok) => { console.log(`alter: ${what.padEnd(72)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
let clock = 0, t0 = 0;                                          // samples rendered; when PLAY was pressed
const render = (n, keep) => {
  const got = [];
  for (let k = 0; k < n; k += 128) {
    const m = Math.min(128, n - k);
    ex.web_render(m);
    if (keep) { const l = new Float32Array(mem.buffer, ex.web_out_l(), m); for (let i = 0; i < m; i++) got.push(l[i]); }
    clock += m;
  }
  return got;
};
const K = (n) => ex.web_keys(n), tap = (k) => { K(1 << k); render(1200); K(0); render(1200); };
const enc = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); render(1300); } };
const play = () => { ex.web_buttons(1 << PLAY); render(128); ex.web_buttons(0); };
const peak = (a) => a.reduce((p, x) => Math.max(p, Math.abs(x)), 0);
// attacks: the envelope (mean |x| over 3 ms) jumps to over 3 x what it was 10 ms before, above a floor (the
// presets' reverb and delay tails never let the output fall silent); rel: and over rel x the loudest (not the
// delay's echoes)
const attacks = (a, gap, rel = 0) => {
  const W = 128, env = new Float32Array(a.length);
  let acc = 0;
  for (let i = 0; i < a.length; i++) { acc += Math.abs(a[i]) - (i >= W ? Math.abs(a[i - W]) : 0); env[i] = acc / W; }
  const out = [], D = Math.round(FS * 0.010), floor = Math.max(0.006, rel * env.reduce((p, x) => Math.max(p, x), 0));
  let last = -1e9;
  for (let i = D; i < a.length; i++)
    if (env[i] > floor && env[i] > 3 * env[i - D] && i - last > gap) { out.push(i - W / 2); last = i; }
  return out;
};
// a bar from the next bar line (on the PLAY grid): the steps that start an attack (0..15)
const bar = () => {
  const PRE = STEP >> 1, into = (clock - t0 + PRE) % BAR;      // (from half a step before the bar line: the first
  render(BAR - into, false);                                    // hit has some sound before it to rise from)
  const at = clock, w = render(BAR + PRE, true), a = w.slice(PRE), s = new Set();
  for (const i of attacks(w, FS * 0.03, 0.3))
    if (i >= PRE * 0.5) s.add(Math.floor((at + i - t0) / STEP + 0.25) % 16);
  return { steps: [...s].sort((x, y) => x - y), peak: peak(a), audio: a };
};
const euclid = (n, k, r) => { const p = []; for (let i = 0; i < n; i++) if ((i * k) % n < k) p.push((i + r) % n); return p.sort((x, y) => x - y); };
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
// power-on = the TECHNO scene (alter_sounds.c): THUD 4/16, STAB 3/16 turned by 3 (a triad), CRUSH 11/16 at PROB 110
const C1 = euclid(16, 4, 0), C2 = euclid(16, 3, 3), C3 = euclid(16, 11, 0);

render(FS * 2, false);                                          // the splash
K(1 << 26); const quiet = render(FS / 2, true); K(0); render(FS / 4, false);
check("a key on the PLAY view plays no note", peak(quiet) < 0.001);
ex.web_alter(4, BPM, 0);                                        // (the test counts at 120: TECHNO is 132)
ex.web_alter(0, 0, 0); ex.web_alter(9, 0, 12); ex.web_alter(10, 0, 0);   // (circle 1: THUD, no echo on its 4th kick, PERC
                                                                // as by default: the attacks counted are the hits)
enc(1, -3);                                                    // circle 1 to FREE (the scene has it PERC)
play(); t0 = clock - 128;
const all = bar();
const union = [...new Set([...C1, ...C2, ...C3])].sort((x, y) => x - y);
// (in the mix the softer NOISE hits hide under ALTER and PHYS: each circle is heard alone below)
check(`PLAY: the mix hits only the circles' steps ${union.join(",")} (heard ${all.steps.join(",")})`,
      all.steps.every((x) => union.includes(x)) && all.steps.length >= 6);
check(`level sane (peak ${all.peak.toFixed(3)}, no clipping)`, all.peak > 0.02 && all.peak < 0.999);
// each circle alone (MUTE the two others: keys 0 2 4 = MUTE 1 2 3)
tap(2); tap(4);
const s1 = bar();
check(`circle 1 alone (ALTER): 4/16 ${s1.steps.join(",")}`, same(s1.steps, C1));
tap(0); tap(2);
const s2 = bar();
check(`circle 2 alone (ALTER STAB, a triad): 3/16 turned by 3 ${s2.steps.join(",")}`, same(s2.steps, C2));
tap(2); tap(4);
const s3 = bar();
check(`circle 3 alone (NOISE CRUSH): hits only on 11/16, most of them (PROB) ${s3.steps.join(",")}`,
      s3.steps.every((x) => C3.includes(x)) && s3.steps.length >= 6);
tap(0); tap(4);                                                 // circle 1 alone again (selected: circle 1)
// SEQ + KNOB 2: HITS 4 -> 8, from the next cycle
// SEQ: a tap keeps the rhythm view open (KNOB 2 = HITS), a second tap closes it (KNOB 2 = the sound again)
ex.web_buttons(1 << SEQB); render(2000); ex.web_buttons(0); render(2000);
enc(4, 4);
ex.web_buttons(1 << SEQB); render(2000); ex.web_buttons(0); render(2000);
bar();
const h8 = bar();
check(`SEQ tapped (kept open) + KNOB 2: circle 1 HITS 4 -> 8 ${h8.steps.join(",")}`, same(h8.steps, euclid(16, 8, 0)));
enc(4, 2); bar();
check("SEQ tapped again: closed (KNOB 2 moves the sound, not HITS)", same(bar().steps, euclid(16, 8, 0)));
{
  const scr = () => { render(4000); return Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice(0, 240 * 24)).join(); };
  const before = scr();
  ex.web_buttons(1 << SEQB); render(FS * 0.7); ex.web_buttons(0); render(2000);
  const menu = scr();
  ex.web_buttons(1 << SEQB); render(2000); ex.web_buttons(0); render(2000);   // a tap: closed again
  enc(4, 2); bar();
  check("SEQ held 0.7 s: the SEQ menu stays open; a tap closes it (HITS untouched after)",
        menu !== before && scr() === before && same(bar().steps, euclid(16, 8, 0)));
}
// the SEQ menu: VELO lane (steps held + a KNOB); SEQ held + ALGORITHM = TURING; its keys: LOCK, NEXT, PREV; back to EUCLID
{
  const seqHold = (algo) => { ex.web_buttons(1 << SEQB); render(FS * 0.7); if (algo) enc(1, algo); ex.web_buttons(0); render(2000); };
  seqHold(0);                                                   // the menu open
  bar();
  const p0 = bar();
  K(1 << 7); render(1500); enc(3, -30); K(0); render(1500);     // hold step 5 (key 7) + KNOB 1: its VELO down
  const p1 = bar(), pk = (b, k) => peak(b.audio.slice(k * STEP, k * STEP + 2600));
  check(`VELO lane: step 5 held + KNOB 1 down: softer (${pk(p0, 4).toFixed(3)} -> ${pk(p1, 4).toFixed(3)}), still a step`,
        pk(p1, 4) < 0.7 * pk(p0, 4) && same(p1.steps.filter((x) => x !== 4), euclid(16, 8, 0).filter((x) => x !== 4)));
  K(1 << 7); render(1500); enc(3, 30); K(0); render(1500);
  ex.web_buttons(1 << SEQB); render(2000); enc(1, 1); ex.web_buttons(0); render(2000);   // TURING (a hold in the menu: closed)
  bar();
  const tu = [...bar().steps, ...bar().steps];
  check(`TURING: it plays its own pattern (${[...new Set(tu)].join(",")})`, tu.length > 0);
  tap(18); bar();                                               // LOCK: frozen
  const f1 = bar().steps, f2 = bar().steps, f3 = bar().steps;
  check(`TURING LOCK key: the pattern frozen (${f1.join(",")})`, same(f1, f2) && same(f2, f3) && f1.length > 0);
  let nx = f1, tries = 0;
  while (same(nx, f1) && tries++ < 3) { tap(16); bar(); nx = bar().steps; }
  check(`TURING NEXT (SHIFT +1): a new pattern (${nx.join(",")}), still frozen`, !same(nx, f1) && same(bar().steps, nx));
  for (let i = 1; i < tries; i++) { tap(14); render(STEP); }
  tap(14); bar();
  const pv = bar().steps;
  check(`TURING PREV (SHIFT -1): the pattern before again (${pv.join(",")})`, same(pv, f1));
  tap(18); render(2000);                                        // LOCK off
  ex.web_buttons(1 << SEQB); render(2000); enc(1, -2); ex.web_buttons(0); render(2000);   // EUCLID
  bar();
  check("SEQ back to EUCLID: 8 hits as before", same(bar().steps, euclid(16, 8, 0)));
}
// SHIFT +1: turned a step at once, kept
tap(16);
bar();
const sh = bar(), sh2 = bar();
check(`SHIFT +1: turned by 1 and kept ${sh.steps.join(",")}`, same(sh.steps, euclid(16, 8, 1)) && same(sh2.steps, sh.steps));
tap(14);
bar();
check("SHIFT -1: back", same(bar().steps, euclid(16, 8, 0)));
// the die's mark on the screen, a triangle at the left of circle 1 (alter_ui.c alt_scene: ALT_CX 120, ALT_CY 116,
// radius 80: x 24..30)
const mark = () => { render(4000); const v = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)[116 * 240 + 26]; return v === 0x89CA || v === 0x29E3; };   // (circle 1's colour, NIGHT / DAY, bytes swapped)
// DICE 1: a draw at once (drastic: rhythm, bursts, pitch, lengths, envelope, timbre), held 0.6 s: clean
check("no draw at first (no mark)", !mark());
tap(7);
const d1 = bar();
check(`DICE 1: the circle plays a new rhythm (${d1.steps.join(",")})`, d1.steps.length > 0 && !same(d1.steps, euclid(16, 8, 0)));
check("DICE 1: a draw is on (its mark on the screen)", mark());
K(1 << 7); render(FS * 0.7); K(0); render(1200);
check("DICE 1 held 0.7 s: clean (no mark), still 8 hits", !mark() && same(bar().steps, euclid(16, 8, 0)));
// the fills: on the selected circle (1)
{
  const before = bar().steps.length;
  const FILLS = { 1: "ROLL 1/8T", 3: "ROLL 1/16", 5: "BUILD UP", 8: "RATCHET X2", 10: "RATCHET X3", 13: "DUB DELAY",
                  15: "DUB REVERB", 17: "DESTROY", 25: "REVERSE" };
  const got = {}, n = {};
  let worst = 0;
  for (const [k, name] of Object.entries(FILLS)) {
    K(1 << +k); render(2000);
    const b = bar(); K(0); bar();
    const c = attacks(b.audio, FS * 0.012).length;
    got[name] = b.steps.length; n[name] = c;
    worst = Math.max(worst, b.peak);
  }
  console.log("alter: attacks in a bar held: " + Object.entries(n).map(([k, v]) => `${k} ${v}`).join(", "));
  check(`every fill plays and none clips (peak ${worst.toFixed(3)})`, Object.values(n).every((v) => v > 0) && worst < 0.999);
  check(`ROLL 1/16 ~ 16 attacks, 1/8T ~ 12, BUILD UP faster than 1/8T (${n["ROLL 1/16"]} ${n["ROLL 1/8T"]} ${n["BUILD UP"]})`,
        n["ROLL 1/16"] >= 14 && n["ROLL 1/8T"] >= 9 && n["ROLL 1/8T"] <= 14 && n["BUILD UP"] > n["ROLL 1/8T"]);
  // the SEQ menu's RATCH lane: black key 13 = RATCH, step 1 held + KNOB 1 up: that hit plays x4
  {
    const seqTap = (hold) => { ex.web_buttons(1 << SEQB); render(hold); ex.web_buttons(0); render(2000); };
    const a0 = attacks(bar().audio, FS * 0.012).length;
    seqTap(FS * 0.7); K(1 << 13); render(1500); K(0); render(1500);
    K(1 << 0); render(1500); enc(3, 5); K(0); render(1500);
    seqTap(2000); bar();
    const a1 = attacks(bar().audio, FS * 0.012).length;
    seqTap(FS * 0.7); K(1 << 0); render(1500); enc(3, -5); K(0); render(1500); seqTap(2000); bar();
    check(`SEQ RATCH lane: step 1 at x4 adds attacks (${a0} -> ${a1})`, a1 >= a0 + 2);
    // a RANGE: CHANCE lane (black key 3), step 1 held + KNOB 3 down: its chance drawn in 0 .. 100 % at each pass
    seqTap(FS * 0.7); K(1 << 3); render(1500); K(0); render(1500);
    K(1 << 0); render(1500); enc(5, -20); K(0); render(1500); seqTap(2000); bar();
    let played = 0;
    for (let b = 0; b < 10; b++) if (bar().steps.includes(0)) played++;
    seqTap(FS * 0.7); K(1 << 0); render(1500); enc(3, 20); K(0); render(1500); seqTap(2000); bar();   // (KNOB 1: no range, 100 %)
    const back = bar().steps.includes(0);
    check(`SEQ RANGE: CHANCE 0..100 % on step 1 plays it in some bars only (${played}/10), KNOB 1 gives it back`,
          played > 0 && played < 10 && back);
    // SHUFFLE: two steps held + KNOB 2 = the lane shown drawn anew (here CHANCE, black key 3); one step held + KNOB 2:
    // nothing. The screen's lane changes with two, not with one
    const scr = () => { render(4000); return Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice(240 * 46, 240 * 157)).join(); };   // (the bars only: not the moving playhead under them)
    seqTap(FS * 0.7); K(1 << 3); render(1500); K(0); render(1500);
    const l0 = scr();
    K(1 << 0); render(1500); enc(4, 1); K(0); render(1500);
    const l1 = scr();
    K((1 << 0) | (1 << 2)); render(1500); enc(4, 1); K(0); render(1500);
    const l2 = scr();
    seqTap(2000);
    check("SEQ SHUFFLE: KNOB 2 with one step held changes nothing, with two it draws the lane anew", l1 === l0 && l2 !== l1);
    // FLAT: OCT- + OCT+ held + the CHANCE key: that lane back to flat (the bars as before the shuffle)
    seqTap(FS * 0.7);
    ex.web_buttons((1 << 12) | (1 << 13)); render(2000); K(1 << 3); render(1500); K(0); ex.web_buttons(0); render(2000);
    const l3 = scr();
    seqTap(2000);
    check("SEQ FLAT: OCT- + OCT+ + the lane's black key puts it back flat", l3 === l0);
    // (the chances stay audible: 30 % and up; put back to 100 % for what follows)
    seqTap(FS * 0.7); K(1 << 3); render(1500); K(0); render(1500);
    K(0x5ad5ad5); render(1500); enc(3, 20); K(0); render(1500); seqTap(2000);   // (all 16 step keys held)
  }
  check("RATCHET X2 / X3 add attacks", n["RATCHET X2"] > before && n["RATCHET X3"] > n["RATCHET X2"]);

  check(`let go: the pattern is back`, same(bar().steps, euclid(16, 8, 0)));
}
// DENSITY: taps, kept
tap(22); bar();
check("DENSITY + tapped: 9 hits, kept", same(bar().steps, euclid(16, 9, 0)));
tap(20); tap(20); bar();
check("DENSITY - tapped twice: 7 hits", same(bar().steps, euclid(16, 7, 0)));
tap(22); bar();
// quick pages: a tap on ENV keeps the circles, KNOB 2 = DEC (shorter notes: less sound), ENV again closes it
{
  const B = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
  const H = (n) => { ex.web_buttons(1 << n); render(Math.floor(44100 * 0.7)); ex.web_buttons(0); render(2000); };   // held: the menu
  const energy = (b) => b.audio.reduce((p, x) => p + Math.abs(x), 0);
  bar(); const e0 = energy(bar());
  B(2); enc(4, -30); B(2); bar();
  const e1 = energy(bar());
  check(`ENV tapped + KNOB 2 (DEC) down: shorter notes (energy ${e0.toFixed(0)} -> ${e1.toFixed(0)})`, e1 < e0 * 0.8);
  enc(4, 3); bar();
  const es = bar().steps;
  check(`ENV tapped again: closed (KNOB 2 = the sound's again, HITS unchanged) ${es.join(",")}`, same(es, euclid(16, 8, 0)));
  B(2); enc(4, 27); B(2);
  // OCT- + OCT+ held + FX: Felucca's own FX page (the circles' screen goes), HOME comes back
  const px = () => { render(4000); return new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)[13 * 240 + 14]; };
  const home = px();
  ex.web_buttons((1 << 12) | (1 << 13)); render(2000); ex.web_buttons((1 << 12) | (1 << 13) | 1); render(2000);
  ex.web_buttons((1 << 12) | (1 << 13)); render(2000); ex.web_buttons(0); render(FS / 2);
  const felucca = px();
  B(6); render(FS / 2);
  check(`OCT- + OCT+ + FX: Felucca's page (screen changed), HOME: the circles again`, felucca !== home && px() === home);
  // OCT- + OCT+ + DICE 1: circle 1's LOCKS; F3 = RHYTHM locked; HOME; a roll keeps the rhythm
  ex.web_buttons((1 << 12) | (1 << 13)); render(2000); tap(7); ex.web_buttons(0); render(2000);
  tap(0); B(6);
  tap(7); bar();
  {
    // (the draw still makes some hits ghosts, too soft to be heard here: no hit may fall outside the 8 steps)
    const st = [...bar().steps, ...bar().steps], E8 = euclid(16, 8, 0);
    check(`LOCKS: RHYTHM locked on circle 1, after DICE 1 every hit stays on its steps (${[...new Set(st)].join(",")})`,
          st.length > 0 && st.filter((x) => !E8.includes(x)).length <= 1);   // (one stray attack allowed: a draw's
                                                                // long note or high pitch can fool the detector; it moved
                                                                // with the test's own timing, 01/11)
  }
  K(1 << 7); render(FS * 0.7); K(0); render(1200);
}
// ALGORITHM: circle 1's envelope mode (FREE PERC STAB DRONE): DRONE rings far longer than PERC
{
  const energy = (b) => b.audio.reduce((p, x) => p + Math.abs(x), 0);
  enc(1, 1); bar(); const perc = energy(bar());
  enc(1, 2); bar(); const drone = energy(bar());
  enc(1, -3); bar(); const free = energy(bar());
  check(`ALGORITHM: PERC ${perc.toFixed(0)} < FREE ${free.toFixed(0)}... DRONE ${drone.toFixed(0)} the most sound`,
        drone > 2 * perc && drone > free);
  check("modes keep the rhythm (FREE again: 8 hits)", same(bar().steps, euclid(16, 8, 0)));
}
// the envelope modes' own knobs (ENV page by the circle's mode): PERC HOLD, STAB LENGTH; an LFO on LENGTH
{
  const B = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
  const H = (n) => { ex.web_buttons(1 << n); render(Math.floor(44100 * 0.7)); ex.web_buttons(0); render(2000); };   // held: the menu
  const energy = (a) => a.reduce((p, x) => p + Math.abs(x), 0);
  enc(1, 1); B(2);                                              // PERC, its ENV page (ATTACK HOLD DECAY PUNCH)
  bar(); const h0 = energy(bar().audio);
  enc(4, 40); bar(); const h1 = energy(bar().audio);            // HOLD up
  check(`PERC: HOLD up holds the hits longer (energy ${h0.toFixed(0)} -> ${h1.toFixed(0)})`, h1 > 1.5 * h0);
  enc(4, -40); B(2);
  enc(1, 1); B(2);                                              // STAB (LENGTH SUSTAIN PLUCK CHORD)
  bar(); const s0 = energy(bar().audio);
  enc(3, 30); bar(); const s1 = energy(bar().audio);            // LENGTH up
  check(`STAB: LENGTH up rings longer (energy ${s0.toFixed(0)} -> ${s1.toFixed(0)})`, s1 > 1.3 * s0);
  enc(3, -30); B(2);
  enc(1, -1);                                                   // PERC again: LFO 1 SQR 1 BAR -> LENGTH +63
  H(3); enc(3, 3); enc(4, 2); enc(5, 63); enc(6, 10);
  bar();
  const lb = bar().audio, e1 = energy(lb.slice(0, 8 * STEP)), e2 = energy(lb.slice(8 * STEP));
  check(`LFO 1 SQR -> LENGTH (PERC): one half of the bar rings longer (${e1.toFixed(0)} / ${e2.toFixed(0)})`,
        Math.max(e1, e2) > 1.5 * Math.min(e1, e2));
  enc(5, -63); enc(6, -10); enc(3, -3); enc(4, -2); B(3);
  enc(1, 2);                                                    // DRONE: the same note is tied, never struck again
  bar(); bar();
  const dr = [...bar().audio, ...bar().audio], fr = [];
  for (let k = 0; k + 2205 <= dr.length; k += 2205) fr.push(peak(dr.slice(k, k + 2205)));
  const jump = Math.max(...fr.slice(1).map((x, i) => x / Math.max(fr[i], 1e-3)));
  check(`DRONE: each hit a soft new layer, no hard attack (largest jump between 50 ms frames x${jump.toFixed(2)})`, jump < 1.5);
  enc(1, -3);                                                   // FREE again
  bar();
  check("modes and LFO off: 8 hits as before", same(bar().steps, euclid(16, 8, 0)));
}
// LFO held 0.5 s: the LFO page. Tabs LFO 1 2 3 (the chosen circle's three), GLOBAL (each K1 WAVE K2 RATE K3 DEPTH
// K4 DEST), MTX (the chosen circle's matrix: SELECT the slot, K1 SRC K2 DEST K3 AMT); ALGORITHM the tab
{
  const B = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
  const H = (n) => { ex.web_buttons(1 << n); render(Math.floor(44100 * 0.7)); ex.web_buttons(0); render(2000); };   // held: the menu
  const top = () => { render(4000); return Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice(0, 240 * 24)).join(); };
  // the loudness of each 16th of a bar (its first 60 ms)
  const steps = (b) => Array.from({ length: 16 }, (_, k) => peak(b.audio.slice(k * STEP, k * STEP + 2600)));
  const swell = (l) => Math.max(l[14] / Math.max(l[0], 1e-4), l[0] / Math.max(l[14], 1e-4));
  const half = (st) => st.length >= 4 && (st.every((x) => x < 8) || st.every((x) => x >= 8));
  const circles = top();
  H(3);
  check("LFO held: the LFO page (the screen's top changes)", top() !== circles);
  enc(3, 3); enc(4, 2); enc(5, 63); enc(6, 9);                  // LFO 1: SQR, 1 BAR, +63, PROB
  bar();
  const pr = [...bar().steps, ...bar().steps];
  check(`LFO 1 SQR 1 BAR -> PROB: circle 1 plays in one half of the bar only (${[...new Set(pr)].join(",")})`, half(pr));
  enc(3, -1); enc(6, -5);                                       // SAW, LEVEL
  bar();
  const lv = steps(bar());
  check(`LFO 1 SAW 1 BAR -> LEVEL: the bar swells (step 0 ${lv[0].toFixed(3)}, step 14 ${lv[14].toFixed(3)})`, swell(lv) > 3);
  enc(1, 3); enc(3, 6); enc(5, 1); enc(6, 4);                   // GLOBAL: RAMP+, 1 BAR, +1, LEVEL: it leads
  bar();
  const gl = bar(), gs = steps(gl);
  check(`the GLOBAL leads on LEVEL: LFO 1's swell gone (step 0 ${gs[0].toFixed(3)}, step 14 ${gs[14].toFixed(3)}; ${gl.steps.join(",")})`,
        swell(gs) < 1.6 && same(gl.steps, euclid(16, 8, 0)));
  enc(5, 62);                                                   // GLOBAL depth +63: the ramp swells the bar
  bar();
  const gr = steps(bar());
  check(`GLOBAL RAMP+ 1 BAR -> LEVEL: the bar swells (step 0 ${gr[0].toFixed(3)}, step 14 ${gr[14].toFixed(3)})`,
        gr[14] > 3 * gr[0] && gr[14] > 0.02);
  enc(5, -63); enc(6, -4);                                      // the GLOBAL off
  enc(1, -3); enc(5, -63); enc(6, -4);                          // LFO 1 off
  enc(1, 1); enc(3, 3); enc(4, 2); enc(5, -64); enc(6, 9);      // LFO 2: SQR, 1 BAR, -64 (the other half), PROB
  bar();
  const p2 = [...bar().steps, ...bar().steps];
  check(`LFO 2 of circle 1, SQR -64 -> PROB: the other half (${[...new Set(p2)].join(",")})`,
        half(p2) && p2.every((x) => x >= 8) !== pr.every((x) => x >= 8));
  enc(5, 64); enc(6, -9);                                       // LFO 2 off
  enc(1, 3);                                                    // MTX: slot 1 POS -> AMP +63
  enc(3, 11); enc(4, 4); enc(5, 63);
  bar();
  const ps = steps(bar());
  check(`MTX slot 1, POS -> AMP: circle 1 swells across its cycle (step 0 ${ps[0].toFixed(3)}, step 14 ${ps[14].toFixed(3)})`,
        ps[14] > 3 * ps[0] && ps[14] > 0.02);
  enc(5, -63);
  B(3);
  check("LFO again: back to the circles", top() === circles);
  bar();
  check("nothing left on: 8 hits as before", same(bar().steps, euclid(16, 8, 0)));
}
// a fill on another circle leaves this one alone: SELECT circle 2 (muted), RATCHET X3 held
enc(0, 1);
{
  K(1 << 10); render(2000); const b = bar(); K(0);
  check(`SELECT circle 2: RATCHET X3 does not touch circle 1 ${b.steps.join(",")}`, same(b.steps, euclid(16, 8, 0)));
}
// PRESETS on circle 2: through the engines (PHYS's last presets, then NOISE's)
tap(2);                                                          // circle 2 back on
enc(2, 3);
check("PRESETS on circle 2: it still plays", bar().steps.length >= 1);
// CHAOS: the three dice at once: everything drawn (circle 3 back on too); it plays, nothing clips
tap(4);
{
  const px = () => { render(4000); return Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice(0, 240 * 40)).join(); };
  const before = px();
  K((1 << 7) | (1 << 9) | (1 << 11)); render(2000); K(0); render(2000);
  const after = px();
  let worst = 0, heard = 0;
  for (let i = 0; i < 4; i++) { const b = bar(); worst = Math.max(worst, b.peak); heard += b.steps.length; }
  check(`CHAOS (DICE 1+2+3): new sounds on screen, it plays (${heard} attacks in 4 bars), peak ${worst.toFixed(3)}`,
        after !== before && heard > 0 && worst > 0.01 && worst < 0.999);
}
// SAVE page: SELECT picks (NEW, TECHNO, HOUSE ..), a tap of SAVE loads: HOUSE = a kick on the four beats
const B = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
const row = (n) => { enc(0, -25); enc(0, n); };                // (the page keeps its last row: from the top)
const holdSave = () => { ex.web_buttons(1 << 7); render(FS * 1.2); ex.web_buttons(0); render(2000); };
{
  B(7); row(2); B(7);
  ex.web_alter(4, BPM, 0);                                      // (HOUSE is 124: the test counts at 120)
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
  bar();
  const hs = bar().steps;
  check(`SAVE, SELECT +2, SAVE: HOUSE (circle 1: four to the floor) ${hs.join(",")}`, same(hs, euclid(16, 4, 0)));
}
// store a scene: circle 1 to 6 hits, SAVE page, row USER 01 (NEW + 4 factory + 1), SAVE held 1 s
{
  enc(0, -1);                                                   // (circle 1 chosen again)
  tap(22); tap(22); bar();                                      // (DENSITY + twice: 6 hits)
  const b6 = bar().steps;
  check(`before storing: 6 hits ${b6.join(",")}`, same(b6, euclid(16, 6, 0)));
  B(7); row(5); holdSave(); B(6);
  B(7); row(0); B(7);                                       // NEW SCENE: no hits anywhere
  ex.web_alter(4, BPM, 0);
  bar();
  const ns = bar();
  check(`NEW SCENE: silence (no hits on the three circles; heard ${ns.steps.join(",")}, peak ${ns.peak.toFixed(4)})`, ns.peak < 0.01);
  B(7); row(5); B(7);                                           // USER 01 back
  ex.web_alter(4, BPM, 0); ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
  bar();
  const us = bar().steps;
  check(`USER 01 loaded: circle 1's 6 hits again ${us.join(",")}`, same(us, euclid(16, 6, 0)));
  B(7); row(1); holdSave(); B(6);                               // TECHNO: protected
  check("a factory scene cannot be overwritten (TECHNO still 4 hits on circle 1)", (() => {
    B(7); enc(1, -1); row(1); B(7); ex.web_alter(4, BPM, 0); ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1); bar();
    return same(bar().steps, euclid(16, 4, 0));
  })());
}
// PATTERNS tab (ALGORITHM): BROKEN = circle 1 5/16, the sound unchanged
{
  B(7); enc(1, 1); row(3); B(7);
  ex.web_alter(4, BPM, 0);
  bar();
  const ps = bar().steps;
  check(`PATTERNS, BROKEN: circle 1 = 5 hits ${ps.join(",")}`, same(ps, euclid(16, 5, 0)));
}
// the flash keeps USER 01: a fresh instance with this one's flash has it
{
  const nor = new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()).slice();
  const { instance: i2 } = await WebAssembly.instantiate(wasm, {});
  const e2 = i2.exports;
  e2._initialize(); e2.web_nor_erase();
  new Uint8Array(e2.memory.buffer, e2.web_nor(), e2.web_nor_size()).set(nor);
  e2.web_boot();
  const r2 = (n) => { for (let k = 0; k < n; k += 128) e2.web_render(Math.min(128, n - k)); };
  r2(FS * 2);
  const B2 = (n) => { e2.web_buttons(1 << n); r2(2000); e2.web_buttons(0); r2(2000); };
  B2(7); for (let k = 0; k < 5; k++) { e2.web_enc(0, 1); r2(1300); }
  r2(4000);
  const fb = new Uint16Array(e2.memory.buffer, e2.web_screen(), 240 * 240);
  // (row USER 01 chosen: its line says YOURS, drawn in circle 2's colour; EMPTY slots say HOLD SAVE: STORE HERE)
  B2(7); e2.web_buttons(1 << 10); r2(128); e2.web_buttons(0); e2.web_alter(4, BPM, 0);
  e2.web_alter(2, 1, 1); e2.web_alter(2, 2, 1);
  const out2 = []; r2(STEP * 16 * 2);
  for (let k = 0; k < STEP * 16; k += 128) { e2.web_render(128); out2.push(...new Float32Array(e2.memory.buffer, e2.web_out_l(), 128)); }
  const n2 = attacks(out2, FS * 0.03, 0.3).length;
  check(`after power-off: USER 01 loads from the flash (circle 1: ${n2} hits in a bar, 6 stored)`, n2 >= 5 && n2 <= 7);
}
// a Felucca phrase left under the circles (its own SAVE page, an old autosave) and an armed REC: neither may sound.
// NEW SCENE (no hits) must be silent
{
  ex.web_alter(6, 0, 0); ex.web_alter(6, 3, 0);                 // Felucca's phrase 1 on parts 1 and 4
  B(11);                                                        // REC (Felucca's would arm: count-in, click)
  B(7); row(0); B(7);
  play(); render(FS / 2); play();                               // stop, start again: no count-in
  render(FS / 4);
  const q = peak(render(FS * 2, true));
  check(`NEW SCENE over a hidden Felucca phrase, REC pressed: silence (peak ${q.toFixed(4)})`, q < 0.002);
}
play();
{ // (after CHAOS the effects ring on: the delay and reverb tails must die away)
  const tails = [1, 2, 4, 8].map(() => peak(render(FS, true)));
  check(`STOP: the notes stop, the effects' tails die away (peaks a second apart ${tails.map((x) => x.toFixed(4)).join(" ")})`,
        tails[3] < 0.003 && tails[3] <= tails[0]);
}
// the page buttons all alike (01/11): a tap the quick page, held 0.5 s the menu (tapped again: the circles); ENV's
// menu draws the envelope, SCL's the scales, EDIT's is SETTINGS (THEME NIGHT / DAY); GLO held = tapped
{
  const T = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
  const H = (n) => { ex.web_buttons(1 << n); render(Math.floor(FS * 0.7)); ex.web_buttons(0); render(2000); };
  const scr = () => { render(4000); return Array.from(new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)); };
  const key = (a) => a.join();
  ex.web_buttons(1 << 6); render(2000); ex.web_buttons(0); render(FS * 1.5);   // HOME: the circles
  const circles = key(scr());
  for (const [n, nm] of [[2, "ENV"], [1, "SCL"], [4, "EDIT"]]) {
    T(n); const quick = key(scr());
    T(n); render(FS * 1.5); const back = key(scr());
    H(n); const menu = key(scr());
    T(n); render(FS * 1.5); const out = key(scr());
    check(`${nm}: tap = quick page, tap = circles, held = its menu, tap = circles`,
          quick !== circles && back === circles && menu !== quick && menu !== circles && out === circles);
  }
  T(5); const gq = key(scr()); T(5);
  H(5); const gh = key(scr()); T(5);
  check("GLO: held = the same quick page as a tap (no menu yet)", gq === gh && gq !== circles);
  H(4);                                                          // SETTINGS: THEME to DAY
  const avg = (a, y) => { const sw = (c) => ((c & 255) << 8) | (c >> 8); let t = 0; for (let x = 0; x < 200; x++) t += lum(sw(a[240 * y + x])); return t / 200; };
  const lum = (c) => ((c >> 11) & 31) + ((c >> 5) & 63) / 2 + (c & 31);
  const night = avg(scr(), 120);
  ex.web_enc(3, 1); render(4000);
  const day = avg(scr(), 120);
  T(4); render(FS * 1.5);
  const dayc = avg(scr(), 236);
  H(4); ex.web_enc(3, -1); render(4000); T(4); render(FS * 1.5);
  const back = key(scr());
  check(`SETTINGS THEME: DAY lights the screen (brightness ${night.toFixed(0)} -> ${day.toFixed(0)}, circles ${dayc.toFixed(0)}), NIGHT as before`,
        day > 60 && night < 20 && dayc > 60 && back === circles);
}
{                                                               // the combos (Romain, 01/11)
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  const shot = (w = FS * 3) => { render(w); let h = 2166136261; const a = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240); for (let i = 0; i < a.length; i++) h = Math.imul(h ^ a[i], 16777619); return h >>> 0; };
  const chord = (ks) => { let m = 0; for (const k of ks) { m |= 1 << k; K(m); render(2000); } render(4000); K(0); render(2000); };
  const seqMenu = () => { ex.web_buttons(1 << SEQB); render(Math.round(FS * 0.8)); ex.web_buttons(0); render(4000); const h = shot(); ex.web_buttons(1 << 6); render(2000); ex.web_buttons(0); return h; };
  const prism = () => {                                          // the centre's coloured pixels (a muted piece turns grey)
    const a = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240); let n = 0;
    for (let y = 100; y < 132; y++) for (let x = 104; x < 136; x++) {
      const v = a[y * 240 + x], p = ((v >> 8) | (v << 8)) & 0xFFFF, r = (p >> 11) * 8, g = ((p >> 5) & 63) * 4, b = (p & 31) * 8;
      if (Math.max(r, g, b) - Math.min(r, g, b) > 40) n++;
    }
    return n;
  };
  const p0 = shot(), l0 = seqMenu(), p1 = shot(), m1 = prism();
  chord([0, 2, 4]);
  const p2 = shot(), m2 = prism(), l1 = seqMenu();
  tap(0); const mm = prism(); render(FS); tap(0); render(FS * 2);   // (MUTE 1 alone: the measure sees a mute)
  check(`MUTE 1+2+3: the chosen circle's rhythm and lanes drawn anew, no circle muted (colour ${m1} -> ${m2}, muted ${mm})`,
        p2 !== p1 && Math.abs(m2 - m1) < (m1 - mm) / 2 && l1 !== l0);
  chord([25, 14, 16]);
  const c1 = shot(FS / 2);
  chord([14, 16, 25]);                                           // (within 3 s of the first: the undo)
  const c2 = shot();
  check("SHIFT -1 + SHIFT +1 + REVERSE: TOTAL CHAOS; again within 3 s: everything back", c1 !== p2 && c2 === p2);
}
if (wavPath) {                                                  // 4 bars from a fresh start, all three circles
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  play();
  const out = render(BAR * 4 + FS, true);
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
