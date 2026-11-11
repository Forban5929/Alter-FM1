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
ex.web_alter(18, 1, 0);                                         // (the KICK's RUMBLE off: the hits are counted from the audio)
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
  const FILLS = { 13: "DUB DELAY", 15: "DUB REVERB", 17: "DESTROY", 20: "ROLL 1/16", 22: "ROLL 1/8" };   // (RATCHET X2 left the keys: TRIGGER; 1/8T became 1/32, 03/11, then 1/8, 04/11)
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
  const notes = (k) => { K(1 << k); render(2000); bar(); const h0 = ex.web_alter_hits(0); bar(); const d = ex.web_alter_hits(0) - h0; K(0); bar(); return d; };
  const h16 = notes(20), h8 = notes(22);
  check(`ROLL 1/16 ~ 16 attacks; ROLL 1/8 half as many notes as 1/16 (${n["ROLL 1/16"]}; ${h16} -> ${h8})`,
        n["ROLL 1/16"] >= 14 && h16 > 0 && Math.abs(2 * h8 - h16) <= 2);
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

  check(`let go: the pattern is back`, same(bar().steps, euclid(16, 8, 0)));
}
// DENSITY: taps, kept
tap(10); bar();
check("DENSITY + tapped: 9 hits, kept", same(bar().steps, euclid(16, 9, 0)));
tap(8); tap(8); bar();
check("DENSITY - tapped twice: 7 hits", same(bar().steps, euclid(16, 7, 0)));
tap(10); bar();
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
  // OCT- + OCT+ held + FX: Felucca's pages are gone (05/11): ALTER's own screen stays (its FX page at most), HOME back
  const px = () => { render(4000); return new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)[13 * 240 + 14]; };
  const scr2 = () => px();                                       // (the corner: ALTER's frame on each of its screens)
  const home = px();
  ex.web_buttons((1 << 12) | (1 << 13)); render(2000); ex.web_buttons((1 << 12) | (1 << 13) | 1); render(2000);
  ex.web_buttons((1 << 12) | (1 << 13)); render(2000); ex.web_buttons(0); render(FS / 2);
  const felucca = ex.web_alter_hits ? scr2() : 0;
  B(6); render(FS / 2);
  check(`OCT- + OCT+ + FX: no Felucca page (05/11): an ALTER screen (its card), HOME: the circles again`,
        felucca === home && px() === home);
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
// ALGORITHM: circle 1's envelope mode (FREE PERC STAB; DRONE retired 04/11): PERC rings less than FREE; turned on,
// it stops at STAB
{
  const energy = (b) => b.audio.reduce((p, x) => p + Math.abs(x), 0);
  enc(1, 1); bar(); const perc = energy(bar());
  enc(1, 1); bar(); const stab = energy(bar());
  enc(1, 4); bar(); const past = energy(bar());
  enc(1, -6); bar(); const free = energy(bar());
  check(`ALGORITHM: PERC ${perc.toFixed(0)} / STAB ${stab.toFixed(0)} / FREE ${free.toFixed(0)}; past STAB: STAB still (${past.toFixed(0)})`,
        Math.abs(past - stab) < 0.2 * stab && Math.abs(stab - perc) > 0.05 * perc);
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
  enc(1, -1);                                                   // FREE: a note never lasts more than 4 steps
  {
    const lk = (b) => { new Uint8Array(mem.buffer, ex.web_link_in_buf(), b.length).set(b); ex.web_link_in(b.length);
      const o = Array.from(new Uint8Array(mem.buffer, ex.web_link_out(), ex.web_link_out_n())); ex.web_link_out_clear(); return o; };
    const P_ATK = 1, P_DEC = 2, P_SUS = 3, P_REL = 4, v14 = (v) => [(v + 8192) & 127, ((v + 8192) >> 7) & 127];
    const set = (id, v) => lk([0xf0, 0x7d, 0x41, 0x4c, 4, 0, id, ...v14(v), 0xf7]);
    const snd = lk([0xf0, 0x7d, 0x41, 0x4c, 2, 0, 0xf7])[7];          // (STATE: circle 1's sound, to put back)
    set(P_ATK, 0); set(P_DEC, 127); set(P_SUS, 127); set(P_REL, 127);   // a sound that would ring 10 s
    ex.web_alter(15, 0, 16 | 1 << 8);                                   // one hit per bar
    bar(); bar();
    const a = bar().audio, head = energy(a.slice(0, 3 * STEP)) / 3, tail = energy(a.slice(5 * STEP)) / 11;
    check(`FREE: a held sound stops within 4 steps (steps 1-3 ${head.toFixed(1)} / steps 6-16 ${tail.toFixed(2)})`, head > 0 && tail < head * 0.02);
    ex.web_alter(15, 0, 32 | 32 << 8); bar(); bar();                     // LEN 32 asked: 16 at most (Romain 04/11)
    check(`LEN 32 asked: the circle plays 16 steps (pattern ${(ex.web_alter_pat(0) >>> 0).toString(16)})`, (ex.web_alter_pat(0) >>> 0) === 0xffff);
    ex.web_alter(15, 0, 16 | 8 << 8); ex.web_alter(0, 0, snd);          // (8 hits, circle 1's own sound again)
    bar();
  }
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
// a fill on another circle leaves this one alone: CIRCLE 2 tapped (the 2nd black key; muted), DESTROY held
tap(3);
{
  K(1 << 17); render(2000); const b = bar(); K(0);
  check(`CIRCLE 2 key: DESTROY does not touch circle 1 ${b.steps.join(",")}`, same(b.steps, euclid(16, 8, 0)));
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
const holdSave = () => { ex.web_buttons(1 << 7); render(FS * 1.2); ex.web_buttons(0); render(2000); B(7); };   // (held: the NAME page; tapped: stored, 05/11)
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
  tap(10); tap(10); bar();                                      // (DENSITY + twice: 6 hits)
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
  B(7); row(1); ex.web_buttons(1 << 7); render(FS * 1.2); ex.web_buttons(0); render(2000); B(6);   // TECHNO: protected (no NAME page)
  check("a factory scene cannot be overwritten (TECHNO still 4 hits on circle 1)", (() => {
    B(7); enc(1, -1); row(1); B(7); ex.web_alter(4, BPM, 0); ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1); bar();
    return same(bar().steps, euclid(16, 4, 0));
  })());
}
// (the PATTERNS tabs: gone, 10/11, Romain: SONG's A B C are the variations)
// the flash keeps USER 01: a fresh instance with this one's flash has it
{
  const nor = new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()).slice();
  const { instance: i2 } = await WebAssembly.instantiate(wasm, {});
  const e2 = i2.exports;
  e2._initialize(); e2.web_nor_erase();
  new Uint8Array(e2.memory.buffer, e2.web_nor(), e2.web_nor_size()).set(nor);
  e2.web_boot();
  e2.web_alter(18, 1, 0);                                       // (no RUMBLE: the hits counted from the audio)
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
  H(5); const gh = key(scr()); T(5); render(FS * 1.5); const gout = key(scr());
  check("GLO: held = the MIXER (04/11), tapped = the circles", gh !== gq && gh !== circles && gout === circles);
  H(6); const sys = key(scr()); T(6); render(FS * 1.5); const sout = key(scr());
  check("HOME: held = SYSTEM (04/11), tapped = the circles", sys !== circles && sout === circles);
  {                                                              // EDIT held = DEEP (05/11): KNOB 1 on its first row
    const k0 = ex.web_alter_knobp(0, 0);                         //  (SOUND: E1, the engine's first knob here)
    H(4); const dp = key(scr()); ex.web_enc(3, 3); render(8000); const k1 = ex.web_alter_knobp(0, 0);
    ex.web_enc(3, -3); render(8000); T(4); render(FS * 1.5); const dout = key(scr());
    check(`EDIT held = DEEP: KNOB 1 moves its row (${k0} -> ${k1}, back ${ex.web_alter_knobp(0, 0)}), tapped = the circles`,
          dp !== circles && k1 > k0 && ex.web_alter_knobp(0, 0) === k0 && dout === circles);
  }
  H(6); ex.web_enc(1, -5); render(4000);                         // SYSTEM's SETUP: THEME to DAY (SETTINGS until 05/11)
  const avg = (a, y) => { const sw = (c) => ((c & 255) << 8) | (c >> 8); let t = 0; for (let x = 0; x < 200; x++) t += lum(sw(a[240 * y + x])); return t / 200; };
  const lum = (c) => ((c >> 11) & 31) + ((c >> 5) & 63) / 2 + (c & 31);
  const night = avg(scr(), 120);
  ex.web_enc(3, 1); render(4000);
  const day = avg(scr(), 120);
  T(6); render(FS * 1.5);
  const dayc = avg(scr(), 236);
  H(6); ex.web_enc(3, -1); render(4000); T(6); render(FS * 1.5);
  const back = key(scr());
  check(`SYSTEM THEME: DAY lights the screen (brightness ${night.toFixed(0)} -> ${day.toFixed(0)}, circles ${dayc.toFixed(0)}), NIGHT as before`,
        day > 60 && night < 25 && dayc > 60 && back === circles);   // (night ~20: the grain row measured, 05/11 tile)
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
  const m2 = prism(), l1 = seqMenu();
  chord([0, 2, 4]); render(FS * 2);                              // (the three unmuted again)
  const m3 = prism(), l2 = seqMenu(), p2 = shot();
  check(`MUTE 1+2+3 (05/11: no SHUFFLE): three mutes, then three unmutes; the lanes unchanged (colour ${m1} -> ${m2} -> ${m3})`,
        l1 === l0 && l2 === l0 && m2 < m1 && Math.abs(m3 - m1) < (m1 - m2) / 2 + 4);
  chord([5, 1, 3]);
  const c1 = shot(FS / 2);
  chord([1, 3, 5]);                                           // (within 3 s of the first: the undo)
  const c2 = shot();
  check("CIRCLE 1 + 2 + 3: TOTAL CHAOS; again within 3 s: everything back", c1 !== p2 && c2 === p2);
}
{                                                               // the level alignment after a random draw (alter_fx.c)
  const g = () => [0, 1, 2].map((c) => ex.web_alter_cal(c, 1));
  [0, 1, 2].forEach((c) => ex.web_alter(2, c, 0));
  if (peak(render(FS, true)) < 0.001) play();                    // (playing: the measure listens)
  K(1 << 7); render(1000); K(1 << 7 | 1 << 9); render(1000); K(1 << 7 | 1 << 9 | 1 << 11); render(4000); K(0);
  render(FS * 9);
  const ga = g();
  ex.web_alter(0, 0, 0); render(FS / 2);                         // (a sound chosen by hand: 0 dB again)
  const gb = g();
  check(`CHAOS: each circle's level aligned (gains ${ga.map((x) => (20 * Math.log10(x / 32768)).toFixed(1)).join(" / ")} dB); a sound chosen: 0 dB`,
        ga.some((x) => x !== 32768) && ga.every((x) => x >= 4129 && x <= 519300) && gb[0] === 32768);
}
{                                                               // LE HASARD (Romain 03/11) : jamais RESO, jamais d'accord, budget d'effets
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2); ex.web_alter(1, 0, 0); render(FS / 2);
  const combo = (ks) => { K(ks.reduce((m, k) => m | (1 << k), 0)); render(3000); K(0); render(3000); };
  let reso = 0, chords = 0, worst = 0, n = 0;
  for (let r = 0; r < 24; r++) {
    if (r & 1) { combo([1, 3, 5]); render(FS * 3); } else combo([7, 9, 11]);   // CHAOS TOTAL (3 s : pas l'annulation) / CHAOS
    render(FS * 4);                                             // (v0.65: playing, the draw lands on the 1 after a whole bar)
    for (let c = 0; c < 3; c++, n++) {
      reso += ex.web_alter_peek(c, 0) === 10; chords += ex.web_alter_peek(c, 1) !== 0;
      if (r & 1) worst = Math.max(worst, ex.web_alter_peek(c, 2));   // (v0.66: CHAOS keeps the effects as they were)
    }
  }
  check(`CHAOS / CHAOS TOTAL x24 : ${n} sons tirés, RESO ${reso}, accords ${chords}, budget d'effets max ${worst} (≤ 392)`,
        reso === 0 && chords === 0 && worst <= 392);
}
{                                                               // v0.65 (Romain 11/11): CHAOS TOTAL playing = a glitch, the draw on the 1 after a whole bar
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2); ex.web_alter(1, 0, 0); render(FS / 2);
  if (peak(render(FS / 2, true)) < 0.001) play();
  const eng = () => [0, 1, 2].map((c) => ex.web_alter_peek(c, 0)).join();   // (the engines: no LFO moves them)
  const e0 = eng();
  ex.web_alter(31, 0, 1); render(FS / 4);
  const e1 = eng();
  render(FS * 5);
  const e2 = eng();
  render(FS * 3);                                               // (past the 3 s of the undo)
  ex.web_alter(31, 0, 1); render(FS / 4); ex.web_alter(31, 0, 1); render(FS * 4);   // (pressed twice: cancelled)
  const e3 = eng();
  check(`CHAOS TOTAL playing: nothing at once (${e0} = ${e1}), the new scene within two bars (${e2}); pressed twice: cancelled (${e3})`,
        e0 === e1 && e2 !== e0 && e3 === e2);
}
{                                                               // v0.66 (Romain 11/11): CHAOS draws no effect; CHAOS TOTAL leaves alone those RANDOM turns off
  const { instance: i2 } = await WebAssembly.instantiate(wasm, {});   // (a machine of its own: no REC, fill or move from the tests before)
  const e2 = i2.exports; e2._initialize(); e2.web_nor_erase(); e2.web_boot();
  const R2 = (n) => { for (let k = 0; k < n; k += 128) e2.web_render(Math.min(128, n - k)); };
  R2(FS * 2); e2.web_alter(1, 0, 0); R2(FS / 2);
  const P = { DIST: 33, CHOR: 34, DLY: 35, REV: 36 };
  const fx = () => [0, 1, 2].map((c) => [P.DIST, P.CHOR, P.DLY, P.REV].map((q) => e2.web_alter_peek(c, 100 + q)).join("/")).join(" ");
  [0, 1, 2].forEach((c) => { e2.web_alter(17, c | P.DIST << 8, 21 + c); e2.web_alter(17, c | P.DLY << 8, 31 + c); e2.web_alter(17, c | P.REV << 8, 41 + c); e2.web_alter(17, c | P.CHOR << 8, 11 + c); });
  R2(FS / 4);
  const f0 = fx();
  let same = 0;
  for (let r = 0; r < 4; r++) { e2.web_alter(31, 0, 2); R2(FS / 2); same += fx() === f0; }   // (CHAOS at once)
  e2.web_alter(32, 0, 2 | 4); e2.web_alter(22, 0, 0); R2(FS / 4);                          // TOTAL, DELAY and REVERB left alone
  const ft = [0, 1, 2].map((c) => e2.web_alter_peek(c, 100 + P.DLY) === 31 + c && e2.web_alter_peek(c, 100 + P.REV) === 41 + c).every(Boolean);
  R2(FS * 4); e2.web_alter(32, 0, 0); e2.web_alter(22, 0, 0); R2(FS / 4);              // TOTAL, all drawn
  const fa = fx();
  check(`CHAOS keeps every effect (${same}/4: ${f0}); TOTAL with DELAY + REVERB off keeps the sends (${ft}); with all on draws them (${fa})`,
        same === 4 && ft && fa !== f0);
}
{                                                               // REC (Romain 01/11): the knob moves on the steps, in a loop
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  ex.web_alter(1, 0, 0); render(FS / 2);                         // (scène 1 chargée : web_boot ne la recharge pas, et le
                                                                 //  CHAOS TOTAL du test d'avant laissait ses tirages)
  const REC = 11, OD = 12, OU = 13, kp = () => ex.web_alter_knobp(0, 0);
  const tapB = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
  const vals = (n, every) => { const v = []; for (let i = 0; i < n; i++) { render(every); v.push(kp()); } return v; };
  tapB(6); if (!ex.web_playing()) play(); render(BAR);       // (HOME: no page left open by the tests before)
  if ((ex.web_lit_buttons() >> REC) & 1) tapB(REC);           // (REC left on by the test of Felucca's REC above)
  const span = (a) => Math.max(...a) - Math.min(...a), base = span(vals(64, STEP));   // (4 bars: a whole LFO swing)   // (the scene's own movement: its LFOs)
  tapB(REC);
  const litOn = (ex.web_lit_buttons() >> REC) & 1;
  render(BAR - 4000 - (BAR % STEP));                             // (from about the cycle's start)
  for (let i = 0; i < 8; i++) { ex.web_enc(3, i < 4 ? 6 : -6); render(STEP); }   // up over 4 steps, down over 4
  render(BAR);
  tapB(REC);
  const n1 = ex.web_alter_rec(0), litOff = (ex.web_lit_buttons() >> REC) & 1, dimOff = (ex.web_dim_buttons() >> REC) & 1;
  const v = vals(64, STEP / 4), lo = Math.min(...v), hi = Math.max(...v), uniq = new Set(v).size;
  check(`REC tapped: lit; knob 1 turned for 8 steps: ${n1} steps written; REC again: faint, the move plays on its own (${lo}..${hi}, ${uniq} values)`,
        litOn && !litOff && dimOff && n1 >= 6 && hi - lo >= 20 && uniq >= 8);
  ex.web_buttons(1 << REC); render(Math.floor(FS * 1.15)); ex.web_buttons(0); render(2000);
  const n2 = ex.web_alter_rec(0), still = span(vals(64, STEP));
  check(`REC held 1 s: the chosen circle's moves cleared (${n2} steps), the knob back to its own movement (${still}, ${base} before)`, n2 === 0 && still <= base + 6 &&   /* (the scene's LFO swing varies a few points with the phase) */ !((ex.web_lit_buttons() >> REC) & 1));
  tapB(REC); ex.web_enc(3, 5); render(STEP * 2); tapB(REC);
  const n3 = ex.web_alter_rec(0);
  tap(7);                                                        // DICE 1: a new draw
  const n4 = ex.web_alter_rec(0);
  tapB(REC); ex.web_enc(3, 5); render(STEP * 2); tapB(REC);
  const n5 = ex.web_alter_rec(0);
  ex.web_buttons(1 << OD | 1 << OU); render(2000); ex.web_buttons(1 << OD | 1 << OU | 1 << REC); render(2000); ex.web_buttons(0); render(4000);
  check(`DICE 1 clears its circle's moves (${n3} -> ${n4}); OCT- + OCT+ + REC: all cleared (${n5} -> ${ex.web_alter_rec(0)})`,
        n3 > 0 && n4 === 0 && n5 > 0 && ex.web_alter_rec(0) === 0 && !((ex.web_lit_buttons() >> REC) & 1));
}
{                                                               // a USER scene keeps REC's moves and the RANGEs (05/11)
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  ex.web_alter(1, 0, 0); render(FS / 2);
  const REC = 11, W = (op, a, b) => ex.web_alter(op, a, b);
  const Bt = (b, hold = 2000) => { ex.web_buttons(1 << b); render(hold); ex.web_buttons(0); render(2000); };
  const urow = (n) => { enc(1, -3); enc(0, -25); enc(0, n); };   // (the SCENES tab, from the top)
  Bt(6); if (!ex.web_playing()) play(); render(BAR);
  if ((ex.web_lit_buttons() >> REC) & 1) Bt(REC);
  Bt(REC); for (let i = 0; i < 6; i++) { ex.web_enc(3, i < 3 ? 6 : -6); render(STEP); } render(BAR); Bt(REC);
  W(19, 0 << 8 | 0, (0x0005 | 30 << 16 | 110 << 24) >>> 0);    // VELO RANGE 30..110 on steps 1 and 3
  const n0 = ex.web_alter_rec(0), r0 = ex.web_alter_rng(0, 0) >>> 0;
  Bt(7); urow(9); Bt(7, FS * 1.2); Bt(7); Bt(6);                         // SAVE held on USER 05; HOME
  W(1, 0, 0); render(FS);                                        // a factory scene: none
  const n1 = ex.web_alter_rec(0), r1 = ex.web_alter_rng(0, 0) & 0xFFFF;
  Bt(7); urow(9); Bt(7); render(FS);                              // USER 05 loaded
  const n2 = ex.web_alter_rec(0), r2 = ex.web_alter_rng(0, 0) >>> 0;
  const lo = (r2 >> 16) & 255, hi = r2 >>> 24;
  check(`USER scene keeps REC's moves (${n0} -> ${n1} -> ${n2}) and the RANGEs (mask ${r0 & 0xFFFF} -> ${r1} -> ${r2 & 0xFFFF}, ${lo}..${hi}, 30..110 set)`,
        n0 > 0 && n1 === 0 && n2 === n0 && r1 === 0 && (r2 & 0xFFFF) === 5 && Math.abs(lo - 30) <= 5 && Math.abs(hi - 110) <= 5);
  const nor = new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()).slice();   // after power-off: from the flash
  const { instance: i3 } = await WebAssembly.instantiate(wasm, {}); const e3 = i3.exports;
  e3._initialize(); e3.web_nor_erase(); new Uint8Array(e3.memory.buffer, e3.web_nor(), e3.web_nor_size()).set(nor);
  e3.web_boot(); e3.web_alter(18, 1, 0);
  for (let k = 0; k < FS * 2; k += 128) e3.web_render(128);
  const B3 = (b, hold = 2000) => { e3.web_buttons(1 << b); for (let k = 0; k < hold; k += 128) e3.web_render(128); e3.web_buttons(0); for (let k = 0; k < 2000; k += 128) e3.web_render(128); };
  const enc3 = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { e3.web_enc(role, Math.sign(n)); for (let k = 0; k < 1300; k += 128) e3.web_render(128); } };
  B3(7); enc3(1, -3); enc3(0, -25); enc3(0, 9); B3(7); for (let k = 0; k < FS; k += 128) e3.web_render(128);
  check(`after power-off: USER 05's moves (${e3.web_alter_rec(0)}) and RANGE (mask ${e3.web_alter_rng(0, 0) & 0xFFFF}) from the flash`,
        e3.web_alter_rec(0) === n0 && (e3.web_alter_rng(0, 0) & 0xFFFF) === 5);
}
// 32 + 32 USER slots (05/11): bank B (the SAVE page's 2nd and 4th tabs) keeps a scene and a pattern, after power-off too
{
  const W = (op, a, b) => ex.web_alter(op, a, b);
  const bits = (m) => { let n = 0; for (m >>>= 0; m; m >>>= 1) n += m & 1; return n; };
  const store = () => { ex.web_buttons(1 << 7); render(FS * 1.2); ex.web_buttons(0); render(2000); B(7); B(6); };
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  W(15, 0, 16 | 5 << 8); render(FS);
  B(7); enc(1, -3); enc(1, 1); enc(0, -25); enc(0, 4); store();             // SCENES B, B05
  W(15, 0, 16 | 10 << 8); render(FS);
  B(7); enc(1, 3); enc(0, -25); enc(0, 1); store();                          // PATTERNS B, B02
  W(15, 0, 16 | 2 << 8); render(FS);
  const nor = new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()).slice();
  const { instance: i5 } = await WebAssembly.instantiate(wasm, {}); const e5 = i5.exports;
  e5._initialize(); e5.web_nor_erase(); new Uint8Array(e5.memory.buffer, e5.web_nor(), e5.web_nor_size()).set(nor);
  e5.web_boot(); e5.web_alter(18, 1, 0);
  const r5 = (n) => { for (let k = 0; k < n; k += 128) e5.web_render(Math.min(128, n - k)); };
  const B5 = (b) => { e5.web_buttons(1 << b); r5(2000); e5.web_buttons(0); r5(2000); };
  const enc5 = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { e5.web_enc(role, Math.sign(n)); r5(1300); } };
  r5(FS * 2);
  B5(7); enc5(1, -3); enc5(1, 1); enc5(0, -25); enc5(0, 4); B5(7); r5(FS);
  const sc = bits(e5.web_alter_pat(0));
  B5(7); enc5(1, 2); enc5(0, -25); enc5(0, 1); B5(7); r5(FS);
  const pt = bits(e5.web_alter_pat(0));
  B5(7); enc5(1, -1); enc5(0, -25); enc5(0, 8); B5(7); r5(FS);               // PATTERNS A, A04: empty
  check(`USER bank B after power-off: scene B05 (${sc} hits, 5 stored), pattern B02 (${pt}, 10), A04 empty (${bits(e5.web_alter_pat(0))})`,
        sc === 5 && pt === 10 && bits(e5.web_alter_pat(0)) === 10);
}
// NAME (05/11): SAVE held on a USER row opens the NAME page (from the first sound's letters), KNOB 1 2 3 the characters,
// SAVE stores; the list says it; the flash keeps it; HOME leaves without storing
{
  const nm = (e, t, u) => { const v = e.web_alter_name(t, u); return String.fromCharCode(v & 255, (v >> 8) & 255, (v >> 16) & 255).replace(/\0/g, ""); };
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  B(7); enc(1, -3); enc(0, -25); enc(0, 6);                      // SCENES A, A02
  ex.web_buttons(1 << 7); render(FS * 1.2); ex.web_buttons(0); render(2000);   // held: NAME
  enc(3, 1); enc(5, -1);                                         // KNOB 1 +1, KNOB 3 -1
  const shown = nm(ex, 0, 1);
  B(7); B(6);
  const named = nm(ex, 0, 1);
  B(7); enc(0, -25); enc(0, 7); ex.web_buttons(1 << 7); render(FS * 1.2); ex.web_buttons(0); render(2000); B(6);   // A03: HOME
  const nor = new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()).slice();
  const { instance: i6 } = await WebAssembly.instantiate(wasm, {}); const e6 = i6.exports;
  e6._initialize(); e6.web_nor_erase(); new Uint8Array(e6.memory.buffer, e6.web_nor(), e6.web_nor_size()).set(nor); e6.web_boot();
  for (let k = 0; k < FS * 2; k += 128) e6.web_render(128);
  check(`NAME: A02 stored as "${named}" (the scene's first sound, KNOB 1 +1, KNOB 3 -1; nothing before: "${shown}"), after power-off "${nm(e6, 0, 1)}", A03 left by HOME: empty (${ex.web_alter_name(0, 2)})`,
        named.length === 3 && shown === "" && nm(e6, 0, 1) === named && ex.web_alter_name(0, 2) === 0);
}
// SONG (05/11): SAVE held + C D E high stores the rhythms in a slot, a tap plays it from the next bar, a slot's key held + the
// others tapped writes a chain (4 bars a link) that loops; ARP held: the SONG page (KNOB 2 a link's bars); the flash keeps it all
{
  const W = (op, a, b) => ex.web_alter(op, a, b), OCTDN = 12, OCTUP = 13, ARP = 8, KA = 19, KB = 21, KC = 23;
  const bits = (m) => { let n = 0; for (; m; m >>>= 1) n += m & 1; return n; };
  const P = () => bits(ex.web_alter_pat(0) >>> 0), S = (w) => ex.web_alter_song(w);
  const withBtn = (b, f) => { ex.web_buttons(1 << b); render(1300); f(); ex.web_buttons(0); render(1300); };
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  if (peak(render(FS, true)) < 0.002) play();
  W(4, BPM, 0);
  W(15, 0, 16 | 4 << 8); render(BAR);
  withBtn(7, () => tap(KA));
  W(15, 0, 16 | 7 << 8); render(BAR);
  withBtn(7, () => tap(KB));
  check(`SONG: SAVE held + C D E high stores slots A and B (mask ${S(4)}), circle 1 plays 7 (${P()})`, S(4) === 3 && P() === 7);
  tap(KA); const early = P(); render(BAR + STEP);
  check(`SONG: A tapped plays from the next bar (${early} -> ${P()} hits, slot ${S(0)})`, P() === 4 && S(0) === 1);
  tap(KC); render(BAR + STEP);
  check(`SONG: C (empty) tapped: nothing changes (slot ${S(0)}, ${P()} hits)`, S(0) === 1 && P() === 4);
  K(1 << KB); render(1300); K((1 << KB) | (1 << KA)); render(1300); K(1 << KB); render(1300); K(0); render(1300);   // B held, A tapped
  check(`SONG: B held + A tapped writes a chain of ${S(2)} links, on (${S(3)})`, S(2) === 2 && S(3) === 1);
  const runs = (n) => {                                         // the slot playing, a quarter bar apart: its runs
    const r = []; let ok = true;
    for (let i = 0; i < n * 4; i++) {
      const sl = S(0), p = P();
      if ((sl === 1 && p !== 4) || (sl === 2 && p !== 7)) ok = false;
      if (r.length && r[r.length - 1][0] === sl) r[r.length - 1][1]++; else r.push([sl, 1]);
      render(BAR / 4);
    }
    return { r, ok };
  };
  let { r, ok } = runs(13);
  check(`SONG: the chain B 4 A 4 loops (${r.map((x) => `${"-AB"[x[0]]}x${x[1] / 4}`).join(" ")}, patterns ${ok ? "right" : "WRONG"})`,
        ok && r.length >= 4 && r[1][0] === 2 && r[1][1] === 16 && r[2][0] === 1 && r[2][1] === 16 && r[3][0] === 2);
  ex.web_buttons(1 << ARP); render(FS * 0.7); ex.web_buttons(0); render(1300);   // ARP held: the SONG page
  enc(0, -5); enc(4, -1);                                       // SELECT link 1 (B), KNOB 2: 4 -> 2 bars
  render(4000);
  const fb = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240);
  let ink = 0; for (let i = 96 * 240; i < 190 * 240; i++) ink += fb[i] !== fb[0];
  ({ r, ok } = runs(13));
  check(`SONG page: link 1 to 2 bars (${r.map((x) => `${"-AB"[x[0]]}x${x[1] / 4}`).join(" ")}; its list drawn: ${ink} px)`,
        ok && ink > 500 && r.some((x, i) => i > 0 && i < r.length - 1 && x[0] === 2 && x[1] === 8) &&
        r.some((x, i) => i > 0 && i < r.length - 1 && x[0] === 1 && x[1] === 16));
  B(6);                                                         // HOME: the page left (the chain stored)
  ex.web_buttons(1 << ARP); render(1300); ex.web_buttons(0); render(1300);   // ARP tapped: the chain off
  const off = S(3); render(BAR * 2); const held = S(0);
  ({ r } = runs(5));
  check(`SONG: ARP tapped: the chain off (${off}), slot ${held} goes on alone (${r.map((x) => `${"-AB"[x[0]]}x${x[1] / 4}`).join(" ")})`,
        off === 0 && r.length === 1 && r[0][0] === held);
  const nor = new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()).slice();   // after power-off: from the flash
  const { instance: i4 } = await WebAssembly.instantiate(wasm, {}); const e4 = i4.exports;
  e4._initialize(); e4.web_nor_erase(); new Uint8Array(e4.memory.buffer, e4.web_nor(), e4.web_nor_size()).set(nor);
  e4.web_boot(); e4.web_alter(18, 1, 0);
  for (let k = 0; k < FS * 2; k += 128) e4.web_render(128);
  check(`after power-off: SONG keeps slots (mask ${e4.web_alter_song(4)}) and the chain (${e4.web_alter_song(2)} links, off: ${e4.web_alter_song(3)})`,
        e4.web_alter_song(4) === 3 && e4.web_alter_song(2) === 2 && e4.web_alter_song(3) === 0);
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
// TRIGGER (black key 25): a hit of the chosen circle at once, whatever its sound; with REC held the nearest step is added
// to the circle (it plays every cycle); REC held 1 s clears the added steps
{
  const W = (op, a, b) => ex.web_alter(op, a, b), RECB = 11;
  const bits = (m) => { let n = 0; for (; m; m >>>= 1) n += m & 1; return n; };
  W(1, 0, 0); W(4, BPM, 0); render(FS);
  if (peak(render(FS, true)) < 0.002) play();                     // (the transport on)
  for (let c = 0; c < 3; c++) W(2, c, 1);                         // the three muted: only TRIGGER sounds
  W(0, 0, 24); W(3, 0, 0); render(FS);                            // circle 1: RESO CLANK, FREE
  const quiet = peak(render(FS / 2, true));
  K(1 << 25);
  const now = render(Math.round(FS * 0.03), true); K(0); render(STEP * 2);   // (sounding within 30 ms: at once, not on the grid)
  const first = now.findIndex((x) => Math.abs(x) > 0.005);
  check(`TRIGGER: muted ${quiet.toFixed(3)}, a tap plays the chosen circle at once (${peak(now).toFixed(3)} after ${(first / FS * 1000).toFixed(0)} ms)`,
        quiet < 0.002 && peak(now) > 0.01 && first >= 0);
  const before = bits(ex.web_alter_pat(0) >>> 0);
  let after = before;
  for (let t = 0; t < 4 && after === before; t++) {               // (a tap on a step that hits already adds nothing)
    render(STEP * 3);
    ex.web_buttons(1 << RECB); render(1500); K(1 << 25); render(STEP * 2); K(0); render(800); ex.web_buttons(0); render(2000);
    after = bits(ex.web_alter_pat(0) >>> 0);
  }
  // a USER scene keeps the added step: saved in USER 03, a factory scene (none), USER 03 again (it is back)
  const Bt = (b, hold = 2000) => { ex.web_buttons(1 << b); render(hold); ex.web_buttons(0); render(2000); };
  const urow = (n) => { enc(1, -3); enc(0, -25); enc(0, n); };   // (the SCENES tab, from the top)
  Bt(7); urow(7); Bt(7, FS * 1.2); Bt(7); Bt(6);                         // SAVE held 1.2 s on USER 03; HOME
  W(1, 0, 0); render(FS);
  const fac = bits(ex.web_alter_pat(0) >>> 0);
  Bt(7); urow(7); Bt(7); render(FS);                              // USER 03 loaded
  const back = bits(ex.web_alter_pat(0) >>> 0);
  ex.web_buttons(1 << RECB); render(FS * 1.3); ex.web_buttons(0); render(2000);   // REC held 1 s: cleared
  const cleared = bits(ex.web_alter_pat(0) >>> 0);
  check(`TRIGGER + REC adds the step (${before} -> ${after} hits); USER scene keeps it (${fac} -> ${back}); REC held 1 s clears it (${cleared})`,
        after === before + 1 && fac === before && back === after && cleared === before);
  for (let c = 0; c < 3; c++) W(2, c, 0);
  W(1, 0, 0); render(FS);
}
// REC + the fills (03/11): ROLL 1/16 held adds the steps it plays; ROLL 1/32 adds them doubled; DUB DELAY held writes
// its steps, which then throw into the delay on every cycle (the send up there, back elsewhere); REC held 1 s clears all
{
  const W = (op, a, b) => ex.web_alter(op, a, b), RECB = 11;
  const bits = (m) => { let n = 0; for (; m; m >>>= 1) n += m & 1; return n; };
  const fx = (bit) => { let n = 0; for (let k = 0; k < 32; k++) if (ex.web_alter_fxr(0, k) & bit) n++; return n; };
  const Bt = (b, hold = 2000) => { ex.web_buttons(1 << b); render(hold); ex.web_buttons(0); render(2000); };
  W(1, 0, 0); W(4, BPM, 0); render(FS);
  if (peak(render(FS, true)) < 0.002) play();
  tap(1);                                                         // (circle 1 chosen: the first black key)
  const p0 = bits(ex.web_alter_pat(0) >>> 0);
  Bt(RECB);                                                       // REC on
  K(1 << 20); render(STEP * 4); K(0); render(STEP);               // ROLL 1/16 over ~4 steps
  const p1 = bits(ex.web_alter_pat(0) >>> 0);
  K(1 << 22); render(STEP * 3); K(0); render(STEP);               // ROLL 1/8 over ~3 steps (adds, no doubling)
  const r32 = fx(1);
  K(1 << 13); render(STEP * 3); K(0); render(STEP);               // DUB DELAY over ~3 steps
  const dly = fx(2);
  Bt(RECB);                                                       // REC off
  let up = 0, down = 0;
  for (let i = 0; i < 64; i++) { render(STEP / 2); const s = ex.web_alter_send(0); if (s === 127) up++; else down++; }
  check(`REC + ROLL 1/16 adds steps (${p0} -> ${p1}), ROLL 1/8 doubles none (${r32}), DUB DELAY writes ${dly} that replay (send up ${up}/64)`,
        p1 >= Math.min(32, p0 + 2) && r32 === 0 && dly >= 2 && up > 0 && down > 0);
  const urow = (n) => { enc(1, -3); enc(0, -25); enc(0, n); };
  const kept = fx(15);
  Bt(7); urow(8); Bt(7, FS * 1.2); Bt(7); Bt(6);                         // USER 04 saved
  W(1, 0, 0); render(FS);
  const fac = fx(15);
  Bt(7); urow(8); Bt(7); render(FS);                              // USER 04 loaded
  check(`a USER scene keeps the REC'd fills (${kept} steps; factory ${fac}; loaded ${fx(15)})`, kept > 0 && fac === 0 && fx(15) === kept);
  ex.web_buttons(1 << RECB); render(FS * 1.3); ex.web_buttons(0); render(2000);   // REC held 1 s: cleared
  check(`REC held 1 s clears the REC'd fills (${fx(15)}) and the added steps (${bits(ex.web_alter_pat(0) >>> 0)} vs ${p0})`,
        fx(15) === 0 && bits(ex.web_alter_pat(0) >>> 0) === p0);
  W(1, 0, 0); render(FS);
}
// RUMBLE (04/11): the KICK alone, its knob 4: 0 = between the hits as before, 45 = a tail there, ducked at the hit
{
  const W = (op, a, b) => ex.web_alter(op, a, b), BEAT = Math.floor(FS / 2);
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);           // (from a fresh power-on: the scene TECHNO playing)
  if (peak(render(FS, true)) < 0.002) play();
  const tailDb = (r) => {
    W(18, 0, 0); W(1, 0, 0); W(4, 120, 0); W(0, 0, 22); W(2, 1, 1); W(2, 2, 1); W(5, 0, 0); W(17, 255 << 8, r);
    render(FS * 4);
    const a = render(BEAT * 8, true); let pk = 0, first = 0;
    for (const x of a) pk = Math.max(pk, Math.abs(x));
    {                                                           // (the beat's phase: where the hits' energy is, folded on the beat)
      const fold = new Float64Array(BEAT); for (let i = 0; i < a.length; i++) fold[i % BEAT] += a[i] * a[i];
      let best = -1, W10 = Math.floor(BEAT * 0.1), acc = 0;
      for (let i = 0; i < W10; i++) acc += fold[i];
      for (let ph = 0; ph < BEAT; ph++) { if (acc > best) { best = acc; first = ph; } acc += fold[(ph + W10) % BEAT] - fold[ph]; }
    }
    let t = 0, h = 0, nt = 0, nh = 0;
    for (let i = first; i < a.length; i++) { const ph = (i - first) % BEAT; if (ph > BEAT * 0.3 && ph < BEAT * 0.9) { t += a[i] * a[i]; nt++; } else if (ph < BEAT * 0.1) { h += a[i] * a[i]; nh++; } }
    return 10 * Math.log10((t / nt) / (h / nh) + 1e-12);
  };
  const off = tailDb(0), on = tailDb(70), max = tailDb(127);
  check(`RUMBLE: the KICK's tail between hits ${off.toFixed(1)} dB (0) -> ${on.toFixed(1)} (70) -> ${max.toFixed(1)} (127), under the hit`,
        on > off + 3 && max > on + 3 && max < -3);
  W(18, 1, 0); W(1, 0, 0); render(FS);
}
// TURING drawn at random (Romain 05/11): LEN 5..16 nearer 16, DENS 40..60 %, LOCK 50..75 %, RANGE 1..4 (TOTAL CHAOS)
{
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  const got = [];
  for (let n = 0; n < 30; n++) {
    ex.web_alter(22, 0, 0); render(FS * 3.2);                    // CHAOS TOTAL at once (v0.65: the keys wait for a bar line; 3 s apart: not undone)
    for (let c = 0; c < 3; c++) {
      const v = ex.web_alter_peek(c, 9) >>> 0;
      if ((v & 255) === 1) got.push({ len: (v >> 8) & 255, dens: (v >> 16) & 255, lock: v >>> 24, range: ex.web_alter_peek(c, 10) });
    }
  }
  const pct = (x) => Math.round(x * 100 / 127), ok = got.every((g) => g.len >= 5 && g.len <= 16 && pct(g.dens) >= 40 && pct(g.dens) <= 60 &&
    pct(g.lock) >= 50 && pct(g.lock) <= 75 && g.range >= 1 && g.range <= 4);
  const mean = got.reduce((a, g) => a + g.len, 0) / (got.length || 1);
  check(`TURING drawn: ${got.length} of 90 circles, all in range: ${ok}, mean LEN ${mean.toFixed(1)} (> 10.5: nearer 16)`, got.length >= 15 && ok && mean > 10.5);
}
// an external clock (Romain 05/11: TRS from Ableton): START, 24 clocks a quarter, STOP; the circles on its tempo
{
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  const res = [];
  ex.web_alter(1, 0, 0); render(FS);
  for (const bpm of [100, 140]) {
    ex.web_clock_mode(2); render(FS / 2);
    ex.web_midi_rt(0xFA, 2); render(16);
    const per = FS * 60 / bpm / 24; let acc = 0; const g0 = 0;   // (START: the circles from step 0)
    for (let i = 0; i < 24 * 16; i++) { ex.web_midi_rt(0xF8, 2); acc += per; const r = Math.round(acc); render(r); acc -= r; }
    const g1 = ex.web_gstep(); ex.web_midi_rt(0xFC, 2); render(FS); res.push([bpm, g1 - g0, ex.web_gstep() - g1]);
    ex.web_clock_mode(0); render(FS / 4);
  }
  check(`TRS clock: 16 beats = ${res.map((r) => `${r[1]} steps at ${r[0]} BPM`).join(", ")} (64 ± 1), STOP holds (${res.map((r) => r[2]).join(", ")})`,
        res.every((r) => Math.abs(r[1] - 64) <= 1 && r[2] === 0));
}
// SONG (05/11): an edit made while a slot plays stays in that slot (A > B > A: A comes back as changed)
{
  const W = (op, a, b) => ex.web_alter(op, a, b), bits = (m) => { let n = 0; for (m >>>= 0; m; m >>>= 1) n += m & 1; return n; };
  ex.web_nor_erase(); ex.web_boot(); render(FS * 2);
  W(1, 0, 0); render(FS);
  if (peak(render(FS, true)) < 0.002) play();
  W(4, BPM, 0); W(18, 1, 0);
  W(15, 0, 16 | 4 << 8); render(BAR); ex.web_buttons(1 << 7); render(1300); tap(19); ex.web_buttons(0); render(1300);
  W(15, 0, 16 | 7 << 8); render(BAR); ex.web_buttons(1 << 7); render(1300); tap(21); ex.web_buttons(0); render(1300);
  tap(19); render(BAR * 2);                                     // A plays
  W(15, 0, 16 | 11 << 8); render(BAR);                          // an edit while A plays: 11 hits
  tap(21); render(BAR * 2); const inB = bits(ex.web_alter_pat(0));
  tap(19); render(BAR * 2); const backA = bits(ex.web_alter_pat(0));
  check(`SONG: an edit while A plays stays in A (B ${inB} hits, A again ${backA}: 11)`, inB === 7 && backA === 11);
}
console.log(fails ? `alter: ${fails} FAIL` : "alter: all ok");
process.exit(fails ? 1 : 0);
