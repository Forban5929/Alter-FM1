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
ex.web_buttons(1 << SEQB); render(FS * 2.2); ex.web_buttons(0); render(2000);
enc(4, 2); bar();
check("SEQ held 2.2 s: closes on release (HITS untouched after)", same(bar().steps, euclid(16, 8, 0)));
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
const mark = () => { render(4000); const v = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)[116 * 240 + 26]; return v !== 0; };
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
  const FILLS = { 1: "ROLL 1/16", 3: "ROLL 1/32", 5: "ROLL 1/16T", 8: "RATCHET X2", 10: "RATCHET X3", 13: "DUB DELAY",
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
  check("ROLL 1/32 ~ 32 attacks", n["ROLL 1/32"] >= 26);
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
          st.length > 0 && st.every((x) => E8.includes(x)));
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
// SCENES: SAVE opens the page, PRESETS +1 points at HOUSE, SAVE loads it: circle 1 = TOMS 5/16 turned by 2
{
  const B = (n) => { ex.web_buttons(1 << n); render(2000); ex.web_buttons(0); render(2000); };
  B(7); enc(2, 1); B(7);
  ex.web_alter(4, BPM, 0);                                      // (HOUSE is 124: the test counts at 120)
  ex.web_alter(2, 1, 1); ex.web_alter(2, 2, 1);
  bar();
  const hs = bar().steps;
  // (TOMS's drum head swoops after the strike: the detector may hear a second attack, so: every hit heard)
  check(`SAVE, PRESETS +1, SAVE: the HOUSE scene (circle 1: TOMS 5/16 turned by 2) ${hs.join(",")}`,
        euclid(16, 5, 2).every((x) => hs.includes(x)));
}
play();
{ // (after CHAOS the effects ring on: the delay and reverb tails must die away)
  const tails = [1, 2, 4, 8].map(() => peak(render(FS, true)));
  check(`STOP: the notes stop, the effects' tails die away (peaks a second apart ${tails.map((x) => x.toFixed(4)).join(" ")})`,
        tails[3] < 0.003 && tails[3] < tails[0]);
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
