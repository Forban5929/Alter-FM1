// SPDX-License-Identifier: GPL-3.0-only
// ALTER LINK: the companion page's SysEx (firmware/src/alter_link.c, docs/alter/LINK.md). Builds requests, reads
// replies. Used by the page (web/alter/editor.html) and the test (tools/alter/alter_link_test.mjs).
export const HDR = [0x7d, 0x41, 0x4c];
export const CMD = { INFO: 1, STATE: 2, DESC: 3, SET: 4, SOUND: 5, SCENE: 6, WATCH: 7, CHANGED: 8, RELOAD: 9, PING: 10 };
export const F_ENUM = 8;

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
  watch: (on) => frame(CMD.WATCH, on ? 1 : 0),
  ping: () => frame(CMD.PING),
};

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
    case CMD.SCENE: {
      const op = b(), slot = b(), rc = b(), used = [];
      while (i < f.length) used.push(b());
      return { cmd, op, slot, rc, used };
    }
    case CMD.WATCH: return { cmd, on: b() };
    case CMD.RELOAD: return { cmd, circ: b() };
    case CMD.PING: return { cmd };
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
