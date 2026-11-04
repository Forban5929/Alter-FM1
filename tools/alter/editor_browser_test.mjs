// SPDX-License-Identifier: GPL-3.0-only
// The companion page in a real (headless) Chrome, demo mode: save a patch (dialog left open 5 s), open it back, store
// and load a USER scene. Needs google-chrome and the site served on :8765 (site/editor/ + site/felucca.wasm + site/fonts):
//   mkdir -p S/editor S/fonts; cp web/alter/editor.html S/editor/index.html; cp web/alter/link.js S/editor/;
//   cp build/emu/felucca.wasm S/; cp build/emu/fonts/* S/fonts/; (cd S && python3 -m http.server 8765 &); node tools/alter/editor_browser_test.mjs
import { spawn } from "node:child_process";
import fs from "node:fs";
const chrome = spawn("google-chrome", ["--headless=new", "--no-sandbox", "--disable-gpu", "--remote-debugging-port=9333",
  "--window-size=1280,1000", "about:blank"], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let ws;
for (let i = 0; i < 50 && !ws; i++) {
  await sleep(200);
  try { const l = await (await fetch("http://127.0.0.1:9333/json")).json(); const p = l.find((t) => t.type === "page"); if (p) ws = new WebSocket(p.webSocketDebuggerUrl); } catch {}
}
await new Promise((r) => (ws.onopen = r));
let id = 0; const wait = new Map(); const logs = [];
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && wait.has(m.id)) { wait.get(m.id)(m); wait.delete(m.id); }
  if (m.method === "Runtime.consoleAPICalled") logs.push(m.params.args.map((a) => a.value).join(" "));
  if (m.method === "Runtime.exceptionThrown") logs.push("EXC " + m.params.exceptionDetails.exception?.description); };
const cmd = (method, params = {}) => new Promise((r) => { const i = ++id; wait.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (expr) => (await cmd("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true })).result?.result?.value;
await cmd("Runtime.enable"); await cmd("Page.enable"); await cmd("Network.enable"); await cmd("Network.setCacheDisabled", { cacheDisabled: true });
await cmd("Page.navigate", { url: "http://localhost:8765/editor/?demo" });
const until = async (expr, ms = 15000) => { const t = Date.now(); while (Date.now() - t < ms) { if (await ev(expr)) return true; await sleep(200); } return false; };
const msg = () => ev(`document.getElementById("msg").textContent`);
const res = [];
const check = (w, ok) => { res.push(`${ok ? "ok  " : "FAIL"} ${w}`); };

check("connecté (3 colonnes avec réglages)", await until(`document.querySelectorAll(".strip input[type=range]").length > 40`));
// change a setting
await ev(`(() => { const r = document.getElementById("p0_0"); r.value = 99; r.dispatchEvent(new Event("input")); })()`);
await sleep(800);
await sleep(3000);
// save a patch: the in-page dialog, then wait 5 s with it open (the old prompt dropped the link here)
await ev(`(() => { const o = HTMLAnchorElement.prototype.click; HTMLAnchorElement.prototype.click = function () {
  if (this.download) { window.__dlName = this.download; fetch(this.href).then((r) => r.text()).then((t) => (window.__dl = t)); } else o.call(this); }; })()`);
await ev(`document.getElementById("bSave").click()`);
await sleep(300);
check("fenêtre « Nom du patch » ouverte dans la page", await ev(`document.getElementById("dlg").open`));
await sleep(5000);
await ev(`(() => { document.getElementById("dlgInput").value = "essai"; document.getElementById("dlgOk").click(); })()`);
await sleep(1500);
await until(`!!window.__dl`, 5000);
const dl = (await ev(`window.__dlName`)) === "essai.alter.json";
check(`fichier essai.alter.json téléchargé`, dl);
check(`toujours connecté après 5 s de fenêtre ouverte (${await msg()})`, !(await msg()).includes("coupée") && (await ev(`document.getElementById("state").dataset.on`)) === "demo");
// change, then open the patch back: the value returns
await ev(`(() => { const r = document.getElementById("p0_0"); r.value = 5; r.dispatchEvent(new Event("input")); })()`);
await sleep(800);
const file = await ev(`window.__dl`);
await ev(`(async () => { const f = new File([${JSON.stringify(file)}], "essai.alter.json", { type: "application/json" });
  const dt = new DataTransfer(); dt.items.add(f); const i = document.getElementById("file"); i.files = dt.files; i.dispatchEvent(new Event("change")); })()`);
check("patch rouvert : le réglage revient à 99", await until(`document.getElementById("p0_0").value === "99"`, 10000));
check(`message après ouverture : ${await msg()}`, (await msg()).includes("chargé"));
// store into USER 2 then load it back
await ev(`document.getElementById("slot").value = "1"`);
await ev(`document.getElementById("bStore").click()`);
await sleep(4000);
check(`ranger dans USER 2 : ${await msg()}`, (await msg()).includes("rangée"));
await ev(`(() => { const r = document.getElementById("p0_0"); r.value = 12; r.dispatchEvent(new Event("input")); })()`);
await sleep(800);
await ev(`document.getElementById("slot").value = "1"; document.getElementById("bLoad").click()`);
await sleep(300);
check("fenêtre « Charger USER 2 ? » ouverte", await ev(`document.getElementById("dlg").open`));
await sleep(4000);
await ev(`document.getElementById("dlgOk").click()`);
check("USER 2 chargée : le réglage revient à 99", await until(`document.getElementById("p0_0").value === "99"`, 10000));
await until(`document.getElementById("msg").textContent.includes("chargée")`, 8000);
check(`message : ${await msg()}`, (await msg()).includes("chargée"));
// v2 (04/11): the circles drawn, a circle's action, a rhythm setting, a lane, the tabs
check("v2 : les 3 cercles dessinés (pas autour de chaque cercle)", await until(`[...document.querySelectorAll("#cols [data-steps]")].every((g) => g.children.length >= 4)`, 8000));
const hitsIx = await ev(`window.__alter.v2.cdesc.findIndex((d) => d.name === "HITS")`);
const h0 = await ev(`window.__alter.v2.cval[0][${hitsIx}]`);
await ev(`document.querySelector('#cols .col [data-a="DENSM"]').click()`);
check(`v2 : DENSITÉ − sur le cercle 1 (HITS ${h0} -> ${h0 - 1})`, await until(`window.__alter.v2.cval[0][${hitsIx}] === ${h0 - 1}`, 5000));
await ev(`document.querySelector('[data-tab="rythmes"]').click()`);
const lenIx = await ev(`window.__alter.v2.cdesc.findIndex((d) => d.name === "LEN")`);
await ev(`(() => { const r = document.getElementById("v1_${lenIx}"); r.value = 9; r.dispatchEvent(new Event("input")); })()`);
await sleep(1500);
check("v2 : LEN du cercle 2 réglé à 9 depuis la page (relu de la machine)", await until(`window.__alter.v2.cval[1][${lenIx}] === 9`, 5000));
await ev(`document.querySelector('[data-panel="rythmes"] .lanes [data-do="flat"]').click()`);
check("v2 : couloir VELO du cercle 1 « À plat » : 16 barres égales", await until(`(() => { const b = [...document.querySelectorAll('[data-panel="rythmes"] .lanes')][0].querySelectorAll("rect.bar"); return b.length === 16 && new Set([...b].map((x) => x.getAttribute("height"))).size === 1; })()`, 5000));
await ev(`document.querySelector('[data-tab="effets"]').click()`);
check("v2 : onglet Effets visible (DELAY, REVERB, MASTER, FILTRE A et B)", await ev(`document.querySelectorAll('[data-panel="effets"] .card').length === 6 && !document.querySelector('[data-panel="effets"]').hidden`));
await cmd("Page.captureScreenshot").then((r) => fs.writeFileSync("/tmp/alter_editor.png", Buffer.from(r.result.data, "base64")));
console.log(res.join("\n"));
if (logs.length) console.log("console:", logs.slice(0, 60).join(" | "));
chrome.kill(); process.exit(res.some((r) => r.startsWith("FAIL")) ? 1 : 0);
