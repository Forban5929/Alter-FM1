// SPDX-License-Identifier: GPL-3.0-only
// Reads alter_knob_audit.mjs's JSON (one or more files) and prints, per engine and setting, across its sounds:
// JUMPS, DEAD zones, NOTHING. node tools/alter/alter_knob_report.mjs a.json b.json > report.json
import fs from "node:fs";
const all = process.argv.slice(2).flatMap((f) => JSON.parse(fs.readFileSync(f)));
const L = Math.log;
const by = new Map();
for (const r of all) {
  const key = `${r.eng}|${r.k}|${r.label}`;
  if (!by.has(key)) by.set(key, { eng: r.eng, k: r.k, label: r.label, enumerated: r.enumerated, min: r.min, max: r.max, n: 0, jumps: [], dead: [], nothing: [], spread: [] });
  const g = by.get(key); g.n++;
  if (r.enumerated) continue;
  const p = r.pts, span = r.max - r.min;
  let dbs = p.map((x) => x.db), lo = Math.min(...dbs), hi = Math.max(...dbs);
  const hzr = Math.max(...p.map((x) => x.hz)) / Math.max(1, Math.min(...p.map((x) => x.hz)));
  const lens = p.map((x) => Math.max(0.02, x.len)), lr = Math.max(...lens) / Math.min(...lens);
  g.spread.push(hi - lo);
  if (hi - lo < 1 && hzr < 1.08 && lr < 1.1) { g.nothing.push(r.sound); continue; }
  for (let i = 1; i < p.length; i++) {
    const a = p[i - 1], b = p[i];
    const ddb = Math.abs(b.db - a.db), dhz = Math.abs(L(b.hz / a.hz)), dln = Math.abs(L(Math.max(0.02, b.len) / Math.max(0.02, a.len)));
    if (ddb >= 3 || dhz >= L(1.4) || (dln >= L(1.5) && Math.max(a.len, b.len) > 0.1))
      g.jumps.push({ sound: r.sound, from: a.v, to: b.v, ddb: +(b.db - a.db).toFixed(1), hz: [Math.round(a.hz), Math.round(b.hz)], len: [+a.len.toFixed(2), +b.len.toFixed(2)] });
  }
  /* DEAD: the longest stretch over which nothing heard moves (all of it within 1 dB, x1.08 brightness, x1.1 length) */
  let best = 0, bestAt = null;
  for (let i = 0; i < p.length; i++) {
    let dl = p[i].db, dh = p[i].db, hl = p[i].hz, hh = p[i].hz, ll = Math.max(0.02, p[i].len), lh = ll;
    for (let j = i + 1; j < p.length; j++) {
      dl = Math.min(dl, p[j].db); dh = Math.max(dh, p[j].db); hl = Math.min(hl, p[j].hz); hh = Math.max(hh, p[j].hz);
      ll = Math.min(ll, Math.max(0.02, p[j].len)); lh = Math.max(lh, Math.max(0.02, p[j].len));
      if (dh - dl >= 1 || hh / hl >= 1.08 || lh / ll >= 1.1) break;
      if (p[j].v - p[i].v > best) { best = p[j].v - p[i].v; bestAt = [p[i].v, p[j].v]; }
    }
  }
  if (best / span >= 0.4) g.dead.push({ sound: r.sound, from: bestAt[0], to: bestAt[1], pct: Math.round(best / span * 100) });
}
const out = [...by.values()].map((g) => ({ ...g, spread: g.spread.length ? +(g.spread.reduce((a, b) => a + b) / g.spread.length).toFixed(1) : 0 }));
console.log(JSON.stringify(out, null, 1));
