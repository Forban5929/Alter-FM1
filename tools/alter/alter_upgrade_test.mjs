// SPDX-License-Identifier: GPL-3.0-only
// « La mise à jour garde tout » (v1.28, Romain 24/11). Une FM-1 qui a servi sous une ANCIENNE version (USER scenes in the
// three banks, SONG slots, MIDI LEARN links, SYSTEM settings) gets the NEW version: its flash is what the old one left
// (the update loader writes only the app area: tests/ldr_test.c checks that part), and the new one must find it all.
//   node tools/alter/alter_upgrade_test.mjs OLD.wasm NEW.wasm
// OLD.wasm: the browser build of a published version (git worktree of tag vX.YY, tools/alter/wasm/build_emu.sh).
import fs from "node:fs";

const [oldW, newW] = [process.argv[2], process.argv[3]].map((p) => fs.readFileSync(p));
let fails = 0;
const check = (what, ok) => { console.log(`upgrade: ${what.padEnd(78)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const B = { HOME: 6, SAVE: 7 };
const E = { SELECT: 0, K1: 3 };

async function boot(wasm, nor) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase();
  if (nor) new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()).set(nor);
  ex.web_boot();
  const t = {
    ex,
    w: (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) ex.web_render(Math.min(128, n - k)); },
    enc: (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); t.w(30); } },
    nor: () => new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()).slice(),
    links: () => Array.from({ length: 42 }, (_, i) => ex.web_alter_peek(0, 400 + i) >>> 0).filter((v) => (v >> 8) & 0x80),
    name: (u) => { const v = ex.web_alter_name(0, u); return String.fromCharCode(v & 255, (v >> 8) & 255, (v >> 16) & 255).replace(/\0/g, ""); },
    pats: () => [0, 1, 2].map((c) => ex.web_alter_pat(c) >>> 0),
    // the scene in USER u: loaded, its three circles' steps
    load: (u) => { ex.web_alter(24, u, 0); t.w(300); return t.pats(); },
    params: () => [0, 1, 2].map((c) => Array.from({ length: 104 }, (_, p) => ex.web_alter_peek(c, 100 + p))),
  };
  t.w(2500);
  return t;
}

// ---- the old version: three USER scenes (banks A, B, C), names, a SONG slot, a MIDI LEARN link, a SYSTEM setting
const o = await boot(oldW);
const SLOTS = [0, 20, 47];                                      // (A01, B05, C16)
const want = {};
SLOTS.forEach((u, k) => {
  o.ex.web_alter(15, 0, (16 | (3 + k) << 8));                   // (circle 1: 3, 4, 5 hits over 16)
  o.ex.web_alter(15, 1, (16 | (5 + k) << 8));
  o.w(400);
  o.ex.web_alter(23, u, 0); o.w(300);                           // SAVE into USER u
});
// a NAME on A01, as Romain does it: SAVE page, row A01, SAVE held, KNOB 1 +2, SAVE
o.ex.web_buttons(1 << B.SAVE); o.w(60); o.ex.web_buttons(0); o.w(60);
o.ex.web_enc(1, -3); o.w(30); o.enc(E.SELECT, -40); o.enc(E.SELECT, 5);   // (SCENES A: NEW + 4 factory, then A01)
o.ex.web_buttons(1 << B.SAVE); o.w(1300); o.ex.web_buttons(0); o.w(100);
o.enc(E.K1, 2);
o.ex.web_buttons(1 << B.SAVE); o.w(60); o.ex.web_buttons(0); o.w(300);
o.ex.web_buttons(1 << B.HOME); o.w(60); o.ex.web_buttons(0); o.w(100);
const oldName = o.name(0);
// SONG: SAVE held + C high = slot A (alt_sg_save_key)
o.ex.web_alter(15, 2, (16 | 7 << 8)); o.w(400);
o.ex.web_buttons(1 << B.SAVE); o.w(300); o.ex.web_keys(1 << 19); o.w(300); o.ex.web_keys(0); o.ex.web_buttons(0); o.w(300);
const songA = o.ex.web_alter_song(4) >>> 0;   // (the full slots' mask)
// MIDI LEARN: SYSTEM > LEARN (HOME held, ALGORITHM), GROUP 0, row 20, KNOB 1 >, then CC21 (as alter_learn_test.mjs)
o.ex.web_buttons(1 << B.HOME); o.w(700); o.ex.web_buttons(0); o.w(100);
o.enc(1, 1);
o.enc(E.SELECT, -60); o.enc(E.K1, -6); o.enc(E.SELECT, 20); o.enc(E.K1, 1); o.ex.web_midi(0xB0, 21, 100); o.w(100);
o.ex.web_buttons(1 << B.HOME); o.w(60); o.ex.web_buttons(0); o.w(100);
o.w(3500);                                                       // (the settings reach the flash)
const oldL = o.links(), oldLinks = oldL.length;
const wantP = {};
SLOTS.forEach((u) => { want[u] = o.load(u); wantP[u] = o.params(); });              // (each scene as the OLD version loads it)
const nor = o.nor();

// ---- the new version on that flash
const n = await boot(newW, nor);
SLOTS.forEach((u) => {
  const got = n.load(u), gp = n.params();
  const diff = [];
  gp.forEach((row, c) => row.forEach((v, p) => { if (v !== wantP[u][c][p]) diff.push(`c${c + 1} P${p} ${wantP[u][c][p]}→${v}`); }));
  check(`USER ${"ABC"[u >> 4]}${String(u % 16 + 1).padStart(2, "0")} : tous les réglages des 3 sons pareils (${diff.length ? diff.slice(0, 6).join(", ") : "0 écart"})`, !diff.length);
  check(`USER ${"ABC"[u >> 4]}${String(u % 16 + 1).padStart(2, "0")} : les 3 cercles comme sauvés (${got.map((p) => p.toString(16)).join(" ")})`,
        got.join() === want[u].join());
});
check(`le NOM de A01 gardé (« ${n.name(0)} », était « ${oldName} »)`, n.name(0) === oldName && oldName !== "");
check(`SONG : cases pleines gardées (${n.ex.web_alter_song(4) >>> 0}, était ${songA})`, songA !== 0 && (n.ex.web_alter_song(4) >>> 0) === songA);
check(`MIDI LEARN : ${n.links().length} lien(s), comme avant (${oldLinks})`, oldLinks > 0 && n.links().length === oldLinks && n.links().join() === oldL.join());
check(`les 16 sons USER : vides, pas de son fantôme (${((n.ex.web_alter_peek(0, 24) >>> 8) & 0xFFFF).toString(2)})`,
      ((n.ex.web_alter_peek(0, 24) >>> 8) & 0xFFFF) === 0);
console.log(fails ? `upgrade: ${fails} FAIL` : "upgrade: tout ok");
process.exit(fails ? 1 : 0);
