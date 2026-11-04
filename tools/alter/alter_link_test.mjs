// SPDX-License-Identifier: GPL-3.0-only
// ALTER LINK (firmware/src/alter_link.c) against the browser build: node tools/alter/alter_link_test.mjs build/emu/felucca.wasm
import fs from "node:fs";
import { req, parse, splitFrames, CMD, F_ENUM, ACT, SHARED } from "../../web/alter/link.js";

const wasm = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
let fails = 0;
const check = (what, ok) => { console.log(`link: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
render(44100);
const out = () => {
  const n = ex.web_link_out_n(), b = Array.from(new Uint8Array(mem.buffer, ex.web_link_out(), n));
  ex.web_link_out_clear();
  return splitFrames(b).map(parse).filter(Boolean);
};
const send = (bytes) => {
  new Uint8Array(mem.buffer, ex.web_link_in_buf(), bytes.length).set(bytes);
  ex.web_link_in(bytes.length);
  return out();
};
const one = (bytes) => send(bytes)[0];

const info = one(req.info());
check(`INFO: ${info?.version}, ${info?.ncirc} cercles, moteurs ${info?.engines?.join(" ")}`,
  info && info.ncirc === 3 && info.engines.length === 4);
check(`INFO: ${info.sounds.length} sons, ${info.nuser} places USER, ${info.ids.length} réglages`, info.sounds.length >= 23 && info.nuser === 16 && info.ids.length === 22);

const st = [0, 1, 2].map((c) => one(req.state(c)));
check(`STATE des 3 cercles : moteurs ${st.map((s) => info.engines[s.eng]).join(" ")}`, st.every((s, c) => s && s.circ === c && s.values.length === 22));

const descs = info.ids.map((id) => one(req.desc(0, id)));
check(`DESC des 22 réglages du cercle 1 (${descs.slice(0, 4).map((d) => d.label).join(" ")} ...)`, descs.every((d) => d && d.label));
const it = descs[info.ids.indexOf(info.ids[16])];
check(`DESC d'une liste (INSERT TYPE : ${it.names.slice(0, 3).join(" ")} ...)`, it.fmt === F_ENUM && it.names.length > 1);

// SET: E1 of circle 1, then read back; an out-of-range value is kept in range
const E0 = info.ids[0], d0 = descs[0];
const r1 = one(req.set(0, E0, 77));
check("SET E1 = 77, relu dans STATE", r1?.value === 77 && one(req.state(0)).values[0] === 77);
const r2 = one(req.set(0, E0, 5000));
check(`SET hors limites = gardé dans ses limites (${r2?.value} = max ${d0.max})`, r2?.value === d0.max);

// SOUND: load sound n on circle 2, its engine follows
const n = info.sounds.findIndex((s) => s.eng === 0 && s.name === "ANVIL");
const s2 = one(req.sound(1, n));
check(`SOUND ANVIL sur le cercle 2 : moteur ${info.engines[s2?.eng]}, son ${info.sounds[s2?.sound]?.name}`, s2?.eng === 0 && s2?.sound === n);

// WATCH: a change on the device is pushed; the page's own SET is not
one(req.watch(1));
ex.web_link_sync(); out();
one(req.set(0, E0, 40)); ex.web_link_sync();
check("WATCH : le propre SET de la page ne revient pas en écho", out().length === 0);
ex.web_enc(3, 3); render(4000); one(req.ping()); ex.web_link_sync();   /* KNOB 1 (role 3) of the PLAY view */
const pushed = out();
check(`WATCH : un knob tourné sur la machine est envoyé (${pushed.map((p) => p.cmd === CMD.CHANGED ? "CHANGED" : p.cmd === CMD.RELOAD ? "RELOAD" : p.cmd).join(" ") || "rien"})`,
  pushed.some((p) => p.cmd === CMD.CHANGED || p.cmd === CMD.RELOAD));

// SCENE: save into USER slot 3, change the sound, load it back
const w0 = ex.web_flash_writes_count();
const sv = one(req.sceneSave(3));
check("SCENE : sauver dans la place USER 4 (écrit en mémoire flash)", sv?.rc === 1 && ex.web_flash_writes_count() > w0);
const before = one(req.state(1)).values.join();
one(req.sound(1, (n + 1) % info.sounds.length));
const ld = one(req.sceneLoad(3));
const after = one(req.state(1));
check("SCENE : recharger la place 4 remet le son du cercle 2", ld?.rc === 1 && after.values.join() === before && after.eng === 0);
const ls = one(req.sceneList());
check(`SCENE : liste des places USER (${ls?.used?.filter(Boolean).length} occupée)`, ls?.used?.length === 16 && ls.used[3] === 1);
check("SCENE : une place vide se charge pas (rc 0)", one(req.sceneLoad(9))?.rc === 0);

// every engine's descriptions, asked by engine (the page reads them once): sane for all 22 settings
for (let e = 0; e < info.engines.length; e++) {
  const ds = info.ids.map((id) => one(req.desc(2, id, e)));
  const ok = ds.every((d) => d && d.label && d.min <= d.max && d.def >= d.min && d.def <= d.max &&
    (d.fmt !== F_ENUM || (d.names.length === d.max - d.min + 1 && d.names.every((n) => n.length))));
  check(`DESC du moteur ${info.engines[e]} (22 réglages, listes complètes)`, ok);
}

// a scene loaded on the device (as SAVE does) while watched: RELOAD for each circle that changed
one(req.watch(1)); ex.web_link_sync(); out();
ex.web_alter(1, 2, 0); render(8000); one(req.ping()); ex.web_link_sync(); ex.web_link_sync();
const rl = out();
check(`scène changée sur la machine : la page est prévenue (${rl.filter((p) => p.cmd === CMD.RELOAD).length} RELOAD, ${rl.filter((p) => p.cmd === CMD.CHANGED).length} CHANGED)`,
  rl.some((p) => p.cmd === CMD.RELOAD || p.cmd === CMD.CHANGED));

// the page silent for > 3 s (a dialog, a busy tab): watching stops; the next PING starts it again and what changed
// meanwhile is pushed
for (let k = 0; k < 6; k++) { render(44100); ex.web_link_sync(); }
out();
ex.web_enc(3, 4); render(4000); ex.web_link_sync();
const silent = out().length;
one(req.ping()); ex.web_link_sync();
const back = out();
check(`page muette > 3 s : rien envoyé (${silent}), puis un PING et le changement arrive (${back.length})`, silent === 0 && back.length > 0);

// ---- v2 (04/11): every other setting, the lanes, the actions, the circles as they play ----
one(req.watch(0));
const vl = one(req.vlist());
check(`VLIST : ${vl?.groups?.length} groupes, ${vl?.nc} réglages par cercle, ${vl?.ns} partagés, couloirs ${vl?.lanes?.join(" ")}`,
  vl && vl.nc >= 30 && vl.ns >= 40 && vl.lanes.length === 6 && vl.nact === 29);
const cdesc = [], sdesc = [];
for (let i = 0; i < vl.nc; i++) cdesc.push(one(req.vdesc(1, i, 0)));
for (let i = 0; i < vl.ns; i++) sdesc.push(one(req.vdesc(0, i)));
const sane = (d) => d && d.name && d.min <= d.max && (!d.names.length || d.names.length === d.max - d.min + 1);
check(`VDESC : ${vl.nc + vl.ns} réglages décrits, listes complètes (ex. ${cdesc.slice(0, 3).map((d) => d.name).join(", ")} ; matrice ${cdesc.find((d) => d.grp === 6)?.name})`,
  cdesc.every(sane) && sdesc.every(sane));
const ix = (list, name, grp) => list.findIndex((d) => d.name === name && (grp == null || d.grp === grp));
const all = (c) => one(req.vall(c)).values;
const LEN = ix(cdesc, "LEN"), HITS = ix(cdesc, "HITS");
let r = one(req.vset(1, LEN, 7));
check(`VSET LEN du cercle 2 = 7 (gardé ${r?.value}), HITS ramené dans LEN`, r?.value === 7 && all(1)[HITS] <= 7);
r = one(req.vset(1, LEN, 40));
check(`VSET LEN 40 : ${r?.value} (16 au plus)`, r?.value === 16);
const RT = ix(sdesc, "TYPE", 10), BPM = ix(sdesc, "TEMPO");
one(req.vset(SHARED, RT, 3)); one(req.vset(SHARED, BPM, 133));
const shv = all(SHARED);
check(`VSET partagés : réverb ${sdesc[RT].names[shv[RT]]}, tempo ${shv[BPM]}`, sdesc[RT].names[shv[RT]] === "CLOUD" && shv[BPM] === 133);
const DEP = ix(cdesc, "DEPTH", 3);
r = one(req.vset(0, DEP, -20));
check(`VSET LFO 1 DEPTH du cercle 1 = -20 (gardé ${r?.value})`, r?.value === -20 && all(0)[DEP] === -20);

// the lanes
const ln0 = one(req.lane(2, 0));
check(`LANE : VELO du cercle 3, 16 valeurs (${ln0?.values?.slice(0, 4).join(" ")} ...)`, ln0?.values?.length === 16);
r = one(req.lset(2, 4, 5, 9));
check(`LSET PITCH pas 6 = 9 : gardé ${r?.value} (-7..7)`, r?.value === 7 && one(req.lane(2, 4)).values[5] === 7);

// the actions
const hitsBefore = all(0)[HITS];
check("ACT DENSITÉ + sur le cercle 1", one(req.act(0, ACT.DENSP))?.ok === 1 && all(0)[HITS] === Math.min(hitsBefore + 1, all(0)[LEN]));
const v0 = one(req.lane(0, 0)).values.join();
one(req.act(0, ACT.SHUF0 + 0));
check("ACT HASARD VÉLO : le couloir VELO du cercle 1 change", one(req.lane(0, 0)).values.join() !== v0);
one(req.act(0, ACT.FLATALL));
const flat = [0, 1, 2, 3, 4, 5].map((ln) => one(req.lane(0, ln)).values);
check("ACT TOUT À PLAT : CHANCE 127, DLY REV PITCH RATCH 0", flat[1].every((x) => x === 127) && flat.slice(2).every((l) => l.every((x) => x === 0)));
one(req.act(0, ACT.LANES));
check("ACT TOUTES LES SÉQUENCES : les couloirs ne sont plus à plat", [1, 2, 3].some((ln) => one(req.lane(0, ln)).values.some((x, k) => x !== flat[ln][k])));
const mu = all(0)[ix(cdesc, "MUTE")];
one(req.act(0, ACT.MUTE));
check("ACT MUTE : le cercle 1 bascule", all(0)[ix(cdesc, "MUTE")] === 1 - mu);
one(req.act(0, ACT.MUTE));
const s1 = one(req.state(1)).values.join();
check("ACT CHAOS : le son du cercle 2 change", one(req.act(0, ACT.CHAOS))?.ok === 1 && one(req.state(1)).values.join() !== s1);
check("ACT inconnue : refusée (ok 0)", one(req.act(0, 99))?.ok === 0);

// pushes: a value changed on the device, a lane, and the circles playing
one(req.watch(1)); ex.web_link_sync(); out();
ex.web_alter(15, 0, 9 | 3 << 8); render(2000); ex.web_link_sync();   // (circle 1: LEN 9 HITS 3, as the device's knobs)
let pu = out();
check(`poussée : un réglage changé sur la machine arrive (${pu.filter((p) => p.cmd === CMD.VCHANGED).map((p) => cdesc[p.i]?.name + "=" + p.value).join(" ")})`,
  pu.some((p) => p.cmd === CMD.VCHANGED && p.circ === 0 && p.i === LEN && p.value === 9));
ex.web_buttons(1 << 10); render(2000); ex.web_buttons(0);
let plays = 0, moved = new Set();
for (let k = 0; k < 30; k++) { render(1470); one(req.ping()); ex.web_link_sync(); for (const p of out()) if (p.cmd === CMD.PLAY) { plays++; moved.add(p.circles[0].pos); } }
check(`poussée : PLAY pendant 1 s de jeu (${plays} images, ${moved.size} pas différents vus)`, plays >= 10 && moved.size >= 3);

// junk: not ours, or bad lengths -> no reply
check("trames étrangères ou fausses : aucune réponse", send([0xf0, 0x7d, 0x46, 0x4c, 1, 0xf7]).length === 0 && send(req.state(7)).length === 0 && send([0xf0, 0x7d, 0x41, 0x4c, 4, 0, 0xf7]).length === 0);

console.log(fails ? `link: ${fails} FAIL` : "link: tout ok");
process.exit(fails ? 1 : 0);
