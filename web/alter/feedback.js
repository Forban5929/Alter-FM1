// SPDX-License-Identifier: GPL-3.0-only
// Bouton « Feedback » de la page compagnon (v1.1, 22/11) : un petit formulaire, sans compte, qui envoie le retour au
// relais Vercel (tools/alter/feedback-relay) → Issue GitHub « a-trier ». Version, site (public/beta), connexion et
// moteurs des cercles sont ajoutés tout seuls. Jamais prompt/alert/confirm (ils coupent le lien MIDI).
const RELAY = "https://alter-feedback.vercel.app/api/feedback";

const CSS = `
#fbDlg { max-width: 520px; width: calc(100vw - 40px); }
#fbDlg h2 { margin: 0 0 6px; font-size: 1.15em; letter-spacing: .06em; }
#fbDlg .fbHelp { margin: 0 0 14px; opacity: .8; font-size: .92em; }
#fbDlg label { display: block; margin: 0 0 4px; font-size: .85em; letter-spacing: .05em; opacity: .85; }
#fbDlg select, #fbDlg textarea, #fbDlg input { width: 100%; box-sizing: border-box; background: var(--bg); color: var(--ink);
  border: 1px solid var(--rim); padding: 8px 10px; margin-bottom: 12px; font: inherit; }
#fbDlg textarea { min-height: 130px; resize: vertical; }
#fbDlg .fbTrap { position: absolute; left: -9999px; width: 1px; height: 1px; overflow: hidden; }
#fbDlg .fbMeta { font-size: .8em; opacity: .65; margin: -4px 0 12px; }
#fbDlg .fbMsg { min-height: 1.3em; margin: 0 0 10px; font-size: .92em; }
#fbDlg .fbMsg.bad { color: #e06c5a; }
#fbDlg .fbMsg.good { color: var(--gold); }
#fbDlg .dlgBtns { display: flex; gap: 10px; justify-content: flex-end; }
`;

const HTML = `
<h2>FEEDBACK</h2>
<p class="fbHelp">Something sounds wrong, a knob jumps, a bug, an idea? Tell us: every message goes into the list for the next version of ALTER. No account needed.</p>
<label for="fbCat">ABOUT</label>
<select id="fbCat">
  <option value="sound">The sound (levels, a knob, an engine, an effect)</option>
  <option value="bug">A bug or a crash</option>
  <option value="idea">An idea / a wish</option>
  <option value="ui">This page or the synth's screen</option>
  <option value="other">Something else</option>
</select>
<label for="fbText">YOUR MESSAGE</label>
<textarea id="fbText" maxlength="4000" placeholder="What did you do, what did you hear, what did you expect?"></textarea>
<label for="fbContact">CONTACT (optional: e-mail, Instagram… only if you want an answer)</label>
<input id="fbContact" type="text" maxlength="120">
<div class="fbTrap" aria-hidden="true"><input id="fbSite" type="text" tabindex="-1" autocomplete="off"></div>
<p class="fbMeta" id="fbMeta"></p>
<p class="fbMsg" id="fbMsg" role="status"></p>
<div class="dlgBtns"><button class="btn" id="fbNo" type="button">Close</button><button class="btn main" id="fbOk" type="button">Send</button></div>
`;

const $ = (id) => document.getElementById(id);
const site = location.pathname.includes("/beta/") ? "beta" : "public";
let fileVer = "";

async function version() {
  const v = ($("ver")?.textContent || "").trim();     // filled once the FM-1 / emulator answered
  if (v) return v;
  if (!fileVer) {
    try {
      const r = await fetch("../version.txt", { cache: "no-store" });
      const t = r.ok ? (await r.text()).trim() : "";
      fileVer = /^v\d/.test(t) ? t.split(/\s+/)[0] : "?";
    } catch { fileVer = "?"; }
  }
  return fileVer;                                     // (the site's version: no FM-1 answered yet)
}
function engines() {
  const e = [...document.querySelectorAll("[data-eng]")].map((x) => x.textContent.trim()).filter(Boolean);
  return e.length ? e.join(" / ") : "";
}
function link() { return ($("stateTxt")?.textContent || "").trim(); }

function setup() {
  const st = document.createElement("style"); st.textContent = CSS; document.head.appendChild(st);
  const d = document.createElement("dialog"); d.id = "fbDlg"; d.innerHTML = HTML; document.body.appendChild(d);
  const b = document.createElement("button");
  b.className = "btn"; b.id = "bFeedback"; b.type = "button"; b.textContent = "Feedback";
  b.title = "Tell us what to improve: it goes into the list for the next version";
  const guide = $("bGuide");
  if (guide) guide.after(b); else document.body.appendChild(b);

  const msg = (t, k) => { const m = $("fbMsg"); m.textContent = t; m.className = "fbMsg" + (k ? " " + k : ""); };
  let busy = false;
  b.onclick = async () => {
    msg("");
    $("fbMeta").textContent = `Sent with it: ALTER ${await version()} · ${site === "beta" ? "BETA site" : "site"} · ${link() || "not connected"}` +
      (engines() ? ` · engines ${engines()}` : "") + " · your browser's name";
    d.showModal(); $("fbText").focus();
  };
  $("fbNo").onclick = () => d.close();
  $("fbOk").onclick = async () => {
    if (busy) return;
    const text = $("fbText").value.trim();
    if (text.length < 5) { msg("Write a few words first.", "bad"); $("fbText").focus(); return; }
    busy = true; $("fbOk").disabled = true; msg("Sending…");
    try {
      const r = await fetch(RELAY, {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ text, cat: $("fbCat").value, contact: $("fbContact").value.trim(), website: $("fbSite").value,
          version: await version(), site, link: link(), engines: engines(), ua: navigator.userAgent }),
      });
      const j = await r.json().catch(() => ({}));
      if (r.ok && j.ok) {
        msg("Thank you! Your message is in the list for the next version.", "good");
        $("fbText").value = "";
      } else if (r.status === 429) msg("Too many messages in a short time: try again in a few minutes.", "bad");
      else msg("Could not send it right now (" + (j.error || r.status) + "). Try again later.", "bad");
    } catch {
      msg("No connection to the feedback service. Check your internet and try again.", "bad");
    }
    busy = false; $("fbOk").disabled = false;
  };
}
if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", setup); else setup();
