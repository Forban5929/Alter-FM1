// SPDX-License-Identifier: GPL-3.0-only
// A USER scene saved in a browser, turned into a factory scene's bytes:
//   node tools/alter/alter_user_scene.mjs build/emu/felucca.wasm SAVES_DIR SLOT NAME > firmware/src/alter_user_NAME.h
// SAVES_DIR: the JSON files the page's "send my saves" button wrote to its db ({off, b64}: one flash sector each, read
// with ArtifactData list ... out_dir). SLOT: 1..16 (USER 01 = 1). Prints a C array (alter_ui.c alt_dump_user's layout).
import fs from "node:fs";
import path from "node:path";

const [wasmPath, dir, slot, name] = process.argv.slice(2);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase();
const nor = new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size());
for (const f of fs.readdirSync(dir).filter((x) => x.endsWith(".json"))) {
  const d = JSON.parse(fs.readFileSync(path.join(dir, f), "utf8")), doc = d.data || d;
  nor.set(Buffer.from(doc.b64, "base64"), doc.off);
}
ex.web_boot();
for (let k = 0; k < 44100 * 2; k += 128) ex.web_render(128);   // (the PLAY view's first frames read the banks)
const n = ex.web_alter_dump(Number(slot) - 1), b = new Uint8Array(ex.memory.buffer, ex.web_alter_dump_buf(), n);
if (!b[0]) { console.error(`USER ${slot} is empty in these saves`); process.exit(1); }
const rows = [];
for (let i = 0; i < n; i += 24) rows.push("    " + Array.from(b.slice(i, i + 24)).join(", ") + ",");
console.log(`/* ${name}: a USER scene of Romain's, saved in his browser (tools/alter/alter_user_scene.mjs) */\n` +
            `static const uint8_t ALT_USER_${name}[${n}] = {\n${rows.join("\n")}\n};`);
