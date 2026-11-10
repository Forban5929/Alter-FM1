// SPDX-License-Identifier: GPL-3.0-only
// ALTER LINK: the companion page's SysEx (firmware/src/alter_link.c, docs/alter/LINK.md). Builds requests, reads
// replies. Used by the page (web/alter/editor.html) and the test (tools/alter/alter_link_test.mjs).
export const HDR = [0x7d, 0x41, 0x4c];
export const CMD = { INFO: 1, STATE: 2, DESC: 3, SET: 4, SOUND: 5, SCENE: 6, WATCH: 7, CHANGED: 8, RELOAD: 9, PING: 10,
  VLIST: 11, VDESC: 12, VALL: 13, VSET: 14, LANE: 15, LSET: 16, ACT: 17, PLAY: 18, VCHANGED: 19, LCHANGED: 20,
  PAT: 21, SONG: 22, BLOB: 23 };
// the actions (alter_link.c LA_*): a circle's, then CHAOS / TOTAL (any circle number)
export const ACT = { MUTE: 0, DIE: 1, CLEAN: 2, DENSM: 3, DENSP: 4, SHL: 5, SHR: 6, TRIG: 7, RANDOM: 8, LANES: 9,
  FLATALL: 10, SHUF0: 11, FLAT0: 17, CHAOS: 23, TOTAL: 24, SELECT: 25, PLAY: 26, STOP: 27, REC: 28 };
export const SHARED = 3;   // (VALL / VSET: the shared settings)
export const F_ENUM = 8;

// a NAME: 3 characters of " A-Z 0-9 -" (others: a space)
export const NAME_CH = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
export const nameBytes = (n) => [...(String(n).toUpperCase() + "   ").slice(0, 3)].map((ch) => (NAME_CH.includes(ch) ? ch : " ").charCodeAt(0));
const v14 = (v) => { const u = Math.max(-8192, Math.min(8191, v | 0)) + 8192; return [u & 127, (u >> 7) & 127]; };
export const frame = (cmd, ...args) => [0xf0, ...HDR, cmd, ...args.map((b) => b & 127), 0xf7];
export const req = {
  info: () => frame(CMD.INFO),
  state: (c) => frame(CMD.STATE, c),
  desc: (c, id, eng) => (eng == null ? frame(CMD.DESC, c, id) : frame(CMD.DESC, c, id, eng)),
  set: (c, id, v) => frame(CMD.SET, c, id, ...v14(v)),
  sound: (c, n) => frame(CMD.SOUND, c, n),
  sceneList: () => frame(CMD.SCENE, 0),
  sceneSave: (slot) => frame(CMD.SCENE, 1, slot),
  sceneLoad: (slot) => frame(CMD.SCENE, 2, slot),
  // v3 (05/11): the NAMEs (3 characters), the USER patterns, SONG
  userList: (kind) => frame(kind, 3),                       // kind: CMD.SCENE or CMD.PAT
  userSave: (kind, slot, name) => (name == null ? frame(kind, 1, slot) : frame(kind, 1, slot, ...nameBytes(name))),
  userLoad: (kind, slot) => frame(kind, 2, slot),
  song: () => frame(CMD.SONG, 0),
  songStore: (s) => frame(CMD.SONG, 1, s),
  songPlay: (s) => frame(CMD.SONG, 2, s),
  songChain: (links) => frame(CMD.SONG, 3, links.length, ...links.map((l) => (l.slot & 3) | ((l.bars & 3) << 2))),
  songOn: (on) => frame(CMD.SONG, 4, on ? 1 : 0),
  watch: (on) => frame(CMD.WATCH, on ? 1 : 0),
  ping: () => frame(CMD.PING),
  vlist: () => frame(CMD.VLIST),
  vdesc: (scope, i, c = 0) => frame(CMD.VDESC, scope, i, c),
  vall: (c) => frame(CMD.VALL, c),
  vset: (c, i, v) => frame(CMD.VSET, c, i, ...v14(v)),
  lane: (c, ln) => frame(CMD.LANE, c, ln),
  lset: (c, ln, k, v) => frame(CMD.LSET, c, ln, k, ...v14(v)),
  act: (c, a) => frame(CMD.ACT, c, a),
  play: () => frame(CMD.PLAY),
  // 10/11: a whole scene to and from a file (alter_link.c lk_blob); a value in these requests: its 14 bits as they are
  blobSnap: () => frame(CMD.BLOB, 0),
  blobGet: (off) => frame(CMD.BLOB, 1, off & 127, (off >> 7) & 127),
  blobPut: (off, bytes) => frame(CMD.BLOB, 2, off & 127, (off >> 7) & 127, ...pack7(bytes)),
  blobPlay: (size, sum) => frame(CMD.BLOB, 3, size & 127, (size >> 7) & 127, sum & 127, (sum >> 7) & 127),
};
export const BLOB_PUT = 49;   // (bytes a BLOB put frame carries: the synth reads 64 bytes of a frame)
// 8-bit bytes 7 at a time in 8 (a byte of their top bits, then their low 7 bits) and back
export function pack7(bytes) {
  const out = [];
  for (let k = 0; k < bytes.length; k += 7) {
    const g = bytes.slice(k, k + 7);
    out.push(g.reduce((t, x, j) => t | ((x >> 7) & 1) << j, 0), ...g.map((x) => x & 127));
  }
  return out;
}
export function unpack7(f, n) {
  const out = [];
  for (let i = 0; out.length < n && i < f.length;) {
    const top = f[i++];
    for (let j = 0; j < 7 && out.length < n && i < f.length; j++) out.push(f[i++] | (((top >> j) & 1) << 7));
  }
  return out;
}
export const blobSum = (bytes) => bytes.reduce((sm, x) => (sm * 31 + x) & 0x1fff, 0);

// split a byte stream into frames F0..F7
export function splitFrames(bytes) {
  const out = []; let cur = null;
  for (const b of bytes) {
    if (b === 0xf0) cur = [];
    else if (b === 0xf7) { if (cur) out.push(cur); cur = null; }
    else if (cur && b < 0x80) cur.push(b);
  }
  return out;
}

// a frame's bytes between F0 and F7 -> {cmd, ...} or null (not ours)
export function parse(f) {
  if (f.length < 4 || f[0] !== HDR[0] || f[1] !== HDR[1] || f[2] !== HDR[2]) return null;
  let i = 4;
  const b = () => f[i++] ?? 0;
  const v = () => { const lo = b(), hi = b(); return (lo | (hi << 7)) - 8192; };
  const s = () => { let r = ""; while (i < f.length && f[i] !== 0) r += String.fromCharCode(f[i++]); i++; return r; };
  const cmd = f[3];
  switch (cmd) {
    case CMD.INFO: {
      const version = s(), ncirc = b(), neng = b(), engines = [];
      for (let k = 0; k < neng; k++) engines.push(s());
      const nsound = b(), sounds = [];
      for (let k = 0; k < nsound; k++) { const eng = b(); sounds.push({ eng, name: s() }); }
      const nuser = b(), nid = b(), ids = [];
      for (let k = 0; k < nid; k++) ids.push(b());
      sounds.forEach((so) => (so.rumble = !!b()));   // (08/11: appended; an older firmware: none)
      return { cmd, version, ncirc, engines, sounds, nuser, ids };
    }
    case CMD.STATE: {
      const circ = b(), eng = b(), sound = b(), mode = b(), n = b(), values = [];
      for (let k = 0; k < n; k++) values.push(v());
      return { cmd, circ, eng, sound, mode, values };
    }
    case CMD.DESC: {
      const circ = b(), id = b(), fmt = b(), min = v(), max = v(), def = v(), label = s(), unit = s(), names = [];
      if (fmt === F_ENUM) while (i < f.length) names.push(s());
      return { cmd, circ, id, fmt, min, max, def, label, unit, names };
    }
    case CMD.SET: case CMD.CHANGED: return { cmd, circ: b(), id: b(), value: v() };
    case CMD.SCENE: case CMD.PAT: {
      const op = b(), slot = b(), rc = b(), used = [], names = [];
      while (i < f.length) {
        used.push(b());
        if (op === 3) { const c = [b(), b(), b()]; names.push(c[0] ? String.fromCharCode(...c) : ""); }
      }
      return { cmd, op, slot, rc, used, names };
    }
    case CMD.BLOB: {
      const op = b();
      if (op === 0) return { cmd, op, size: v(), sum: v() };
      if (op === 1) { const off = v(), n = v(); return { cmd, op, off, n, bytes: unpack7(f.slice(i), n) }; }
      if (op === 2) return { cmd, op, off: v(), n: v() };
      return { cmd, op, ok: b() };
    }
    case CMD.SONG: {
      const op = b(), rc = b(), full = b(), play = b(), asked = b(), on = b(), link = b(), left = b(), n = b(), chain = [];
      for (let k = 0; k < n; k++) { const l = b(); chain.push({ slot: l & 3, bars: (l >> 2) & 3 }); }
      return { cmd, op, rc, full: [full & 1, (full >> 1) & 1, (full >> 2) & 1], play, asked, on, link, left, chain };
    }
    case CMD.WATCH: return { cmd, on: b() };
    case CMD.RELOAD: return { cmd, circ: b() };
    case CMD.PING: return { cmd };
    case CMD.VLIST: {
      const ng = b(), groups = [];
      for (let k = 0; k < ng; k++) groups.push(s());
      const nc = b(), ns = b(), nl = b(), lanes = [];
      for (let k = 0; k < nl; k++) lanes.push(s());
      return { cmd, groups, nc, ns, lanes, nact: b() };
    }
    case CMD.VDESC: {
      const scope = b(), i2 = b(), grp = b(), min = v(), max = v(), name = s(), names = [];
      while (i < f.length) names.push(s());
      return { cmd, scope, i: i2, grp, min, max, name, names };
    }
    case CMD.VALL: {
      const circ = b(), n = b(), values = [];
      for (let k = 0; k < n; k++) values.push(v());
      return { cmd, circ, values };
    }
    case CMD.VSET: case CMD.VCHANGED: return { cmd, circ: b(), i: b(), value: v() };
    case CMD.LANE: case CMD.LCHANGED: {
      const circ = b(), lane = b(), values = [];
      while (i + 1 < f.length) values.push(v());
      return { cmd, circ, lane, values };
    }
    case CMD.LSET: return { cmd, circ: b(), lane: b(), step: b(), value: v() };
    case CMD.ACT: return { cmd, circ: b(), act: b(), ok: b() };
    case CMD.PLAY: {
      const pb = b(), playing = pb & 1, rec = (pb >> 1) & 1, sel = b(), circles = [];
      for (let c = 0; c < 3; c++) {
        const pos = b(), len = b(), m = b(), p0 = b(), p1 = b(), p2 = b(), hits = b();
        circles.push({ pos, len, mute: m & 1, die: (m >> 1) & 1, pat: p0 | (p1 << 7) | (p2 << 14), hits });
      }
      return { cmd, playing, rec, sel, circles };
    }
    default: return { cmd };
  }
}

// how a value reads, as the device shows it (roughly: the page needn't match the device to the letter)
export function show(d, val) {
  if (!d) return String(val);
  if (d.fmt === F_ENUM) return d.names[val - d.min] ?? String(val);
  if (d.fmt === 11) return val ? "ON" : "OFF";
  return d.unit ? `${val} ${d.unit}` : String(val);
}

// a patch file: the three circles' sounds, as the page saves and opens them
export const PATCH_KIND = "alter-fm1-patch";
