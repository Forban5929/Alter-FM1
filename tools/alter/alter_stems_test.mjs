// SPDX-License-Identifier: GPL-3.0-only
// v1.39 (Romain 25/11, "stems", always 10 channels): the USB input's 3..10 = circles I II III in stereo, the effects on
// 9-10 (fx.c ALT_STEMS; here into a ring of the web build, web_stem_peak). node tools/alter/alter_stems_test.mjs wasm
import fs from "node:fs";
const [wasmPath] = process.argv.slice(2);
const FS = 44100;
const src = fs.readFileSync(new URL("../../firmware/src/alter_sounds.c", import.meta.url), "utf8");
const names = [...src.matchAll(/^\s*\{"([A-Z ]+)", ([0-9]), \{/gm)].map((m) => m[1]);
const idx = (n) => names.indexOf(n);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (n) => { for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
let fails = 0;
const check = (name, ok) => { console.log(`stems: ${name}`.padEnd(96) + (ok ? "ok" : "FAIL")); if (!ok) fails++; };
const P_REV = 37;                                               // (core.h)
render(FS * 2);
ex.web_alter(4, 120, 0);
ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);        // the scene at power-on (TECHNO: its reverb, its delay)
render(FS * 3);
{
  const p = [...Array(8)].map((_, c) => ex.web_stem_peak(c, FS * 3 - 4096 > 4095 ? 4095 : FS));
  check(`TECHNO: the three circles on 3-8 (${p.slice(0, 6).join(" ")}), the effects on 9-10 (${p[6]} ${p[7]})`,
        [p[0], p[2], p[4]].filter((x) => x > 100).length >= 2 && p[6] > 30 && p[7] > 30);
}
ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);        // (stop)
ex.web_alter(24, 0, 2); render(FS / 5);                         // NEW
ex.web_alter(0, 0, idx("KICK")); ex.web_alter(0, 1, idx("STAB"));
ex.web_alter(15, 0, 16 | 4 << 8); ex.web_alter(15, 1, 16 | 3 << 8);
ex.web_alter(2, 2, 1);                                          // circle III muted
ex.web_buttons(1 << 10); render(128); ex.web_buttons(0);
render(FS * 2);
const W = FS;                                                   // the last second
const pk = [...Array(8)].map((_, c) => ex.web_stem_peak(c, W));
check(`circle I (KICK) on 3-4: ${pk[0]} ${pk[1]}`, pk[0] > 300 && pk[1] > 300);
check(`circle II (STAB) on 5-6: ${pk[2]} ${pk[3]}`, pk[2] > 300 && pk[3] > 300);
check(`circle III muted, 7-8 silent: ${pk[4]} ${pk[5]}`, pk[4] === 0 && pk[5] === 0);
ex.web_alter(2, 0, 1);                                          // circle I muted now (its tail dies away)
render(FS * 4);
const pk2 = [...Array(8)].map((_, c) => ex.web_stem_peak(c, W));
check(`circle I muted: 3-4 down to its tail (${pk2[0]} ${pk2[1]}, was ${pk[0]}), II goes on (${pk2[2]})`, pk2[0] < pk[0] / 10 && pk2[2] > 300);
ex.web_stem_enable(0); render(FS / 2); ex.web_stem_enable(1);    // nobody records: nothing written, nothing breaks
render(FS);
check(`recording again after a pause (${ex.web_stem_peak(2, W)})`, ex.web_stem_peak(2, W) > 300);
console.log(`stems: ${fails} FAIL`);
process.exit(fails ? 1 : 0);
