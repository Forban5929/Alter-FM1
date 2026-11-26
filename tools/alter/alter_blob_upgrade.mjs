// SPDX-License-Identifier: GPL-3.0-only
// A patch file (BLOB) of an older firmware played on a newer one: node tools/alter/alter_blob_upgrade.mjs OLD.wasm NEW.wasm
import fs from "node:fs";
import { req, parse, splitFrames, ACT, BLOB_PUT } from "../../web/alter/link.js";
const mk = async (p) => {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(p), {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot();
  const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  render(44100 * 2);
  const out = () => { const n = ex.web_link_out_n(), b = Array.from(new Uint8Array(mem.buffer, ex.web_link_out(), n)); ex.web_link_out_clear(); return splitFrames(b).map(parse).filter(Boolean); };
  const one = (bytes) => { new Uint8Array(mem.buffer, ex.web_link_in_buf(), bytes.length).set(bytes); ex.web_link_in(bytes.length); return out()[0]; };
  return { ex, render, one };
};
const [o, n] = [await mk(process.argv[2]), await mk(process.argv[3])];
const sig = (m) => [0, 1, 2].map((c) => [m.ex.web_alter_peek(c, 13) >>> 0, m.ex.web_alter_pat(c)].join(":")).join(" ");
o.one(req.act(0, ACT.TOTAL)); o.render(44100);
const sn = o.one(req.blobSnap()), bytes = [];
while (bytes.length < sn.size) { const g = o.one(req.blobGet(bytes.length)); if (!g?.bytes?.length) break; bytes.push(...g.bytes); }
const was = sig(o);
for (let k = 0; k < bytes.length; k += BLOB_PUT) n.one(req.blobPut(k, bytes.slice(k, k + BLOB_PUT)));
const pl = n.one(req.blobPlay(bytes.length, sn.sum)); n.render(44100);
const ok = pl?.ok === 1 && sig(n) === was;
console.log(`blob: an older file (${bytes.length} bytes) played on the new firmware: ${pl?.ok ? "accepted" : "REFUSED"}, same circles ${sig(n) === was}`.padEnd(90) + (ok ? "ok" : "FAIL"));
process.exit(ok ? 0 : 1);
