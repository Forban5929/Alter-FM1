#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ALTER's touches on the browser page (build/emu/index.html, made from web/emu/index.html): the name, a French
# line on how to play it, the source link. With --artifact OUT: also the page without its <html>/<head>/<body>
# wrapper, as claude.ai artifacts want it (the same files beside it: worklet.js, felucca.wasm, fonts/).
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = s.replace("<title>Felucca</title>", "<title>ALTER</title>")
# the screen scaled up smooth, as the eye sees the real 240 x 240 one (Romain, 01/11: "pixelise, pas moderne")
s = s.replace(".device canvas { position: absolute; image-rendering: crisp-edges; image-rendering: pixelated;",
              ".device canvas { position: absolute; image-rendering: auto;")
s = s.replace('content="Felucca, the multi-engine synth firmware for the FM-1, running in your browser: the same code the instrument runs."',
              'content="ALTER, un instrument pour le M-VAVE FM-1 (fork de Felucca), dans le navigateur : le même code que la machine."')
s = s.replace('<p class="keys">',
              '<p class="keys"><b>ALTER</b> : <i>Switch on</i>, puis <b>PLAY</b> (touche <kbd>-</kbd>). '
              '3 cercles = 3 sons : <b>1 ALTER</b> (dehors), <b>2 PHYS</b>, <b>3 NOISE</b> (dedans). '
              'SELECT (<kbd>↑</kbd><kbd>↓</kbd>) = cercle choisi ; KNOB 1-4 = son du cercle choisi ; PRESETS = sons des 3 moteurs. <b>SAVE</b> (<kbd>8</kbd>) = page SCÈNES / PATTERNS (SELECT choisit, ALGORITHM change d\'onglet, SAVE charge, SAVE tenu 1 s enregistre dans un slot USER ; NEW et usine protégés). '
              '<b>Boutons de page, tous pareils</b> : <b>appui court</b> = page rapide (les 4 KNOBS changent sur les cercles ; même bouton ou HOME = fermée) ; <b>tenu 0,5 s</b> = le <b>menu</b> du bouton (reste ouvert ; même bouton tapé, HOME ou un autre bouton = sortie). '
              '<b>SEQ</b> (<kbd>0</kbd>) court = les 4 réglages du rythme ; <b>menu SEQ</b> : SEQ tenu + ALGORITHM = moteur du cercle EUCLID · TURING ; touches blanches = les 16 pas : en tenir + KNOB 1 = leur valeur, deux pas tenus + KNOB 2 = SHUFFLE (tout l’onglet tiré au hasard, les 16 pas) ; OCT− + OCT+ tenus + touche noire d’un onglet = cet onglet remis à plat, KNOB 3 / KNOB 4 = bas / haut d’une plage (chaque passage tire une valeur au hasard dedans) ; ALGORITHM = couloir VELO · CHANCE · DELAY · REVERB · PITCH · RATCH (×1 à ×4 par pas) ; touches noires = raccourcis (les 6 couloirs, puis EUCLID/TURING, cercle précédent, cercle suivant) ; la lumière des touches blanches suit le pas joué. <b>TURING</b> en jeu : SHIFT −1 = motif précédent, SHIFT +1 = motif suivant, la touche blanche d’après = LOCK (figé) · <b>ENV</b> (<kbd>3</kbd>) court = 4 réglages, <b>menu = l’enveloppe dessinée</b>, selon le mode du cercle (ALGORITHM) : FREE = ATK DEC SUS REL · PERC = ATTACK HOLD DECAY PUNCH · STAB = LENGTH SUSTAIN PLUCK CHORD · DRONE = SWELL BODY TAIL OPEN (chaque coup = une nouvelle couche douce, 2 au plus, la plus ancienne s’efface) · <b>FX</b> (<kbd>1</kbd>) court = DIST INSERT DELAY REVERB ; <b>menu FX</b> (le clavier joue) : ALGORITHM = onglet DIST · DELAY · REVERB · MASTER, PRESETS = preset de l’onglet, SELECT = cercle (MASTER : la rangée LOW MID HIGH COMP / DUCK SRC SHAPE CLIP) · '
              '<b>LFO</b> (<kbd>4</kbd>) court = le <b>LFO 1 du cercle choisi</b> : WAVE · RATE (division du tempo) · DEPTH · DEST ; une valeur bougée par un LFO montre sur sa barre une zone de la couleur du cercle (la plage du LFO) ; sans barre (PROB, enveloppe) : une petite vague à côté du nom ; <b>menu LFO</b> : onglets LFO 1 · LFO 2 · LFO 3 (les 3 LFO du cercle choisi, SELECT change de cercle) · GLOBAL (les 3 cercles ensemble) · MTX (matrice du cercle choisi : SELECT = case, KNOB 1 source, 2 destination, 3 quantité) ; la vague défile sur une grille d’une mesure (DEPTH change sa taille) ; ALGORITHM = onglet · <b>EDIT</b> (<kbd>5</kbd>) court = les 4 autres réglages du moteur, <b>menu = RÉGLAGES</b> (THEME : NIGHT sombre / DAY clair, KNOB 1) · <b>SCL</b> (<kbd>2</kbd>) court = GAMME NOTE ACCORD (OFF OCT 5TH TRIAD 7TH 9TH) DOSE, <b>menu = les gammes</b> (liste + clavier, KNOB 1 choisit) · '
              '<b>GLO</b> (<kbd>6</kbd>) TEMPO VOLUME PAN SWING (pas encore de menu : tenu = court). <b>OCT− + OCT+</b> (<kbd>[</kbd><kbd>]</kbd>) tenus + bouton = vraie page Felucca ; '
              '+ DÉ n = VERROUS du cercle n. HOME (<kbd>7</kbd>) = retour.<br>'
              '<b>ALGORITHM</b> (<kbd>←</kbd><kbd>→</kbd>) = mode d\'enveloppe du cercle choisi : FREE · PERC · STAB · DRONE. '
              '<b>3 MUTE</b> ensemble = le cercle choisi tiré au hasard : longueur (4 à 16 pas), nombre de coups, couloirs VELO, CHANCE et PITCH, tout de suite (les mutes ne bougent pas). <b>CHAOS TOTAL</b> = SHIFT −1 + SHIFT +1 + REVERSE ensemble : tout sans limite sur les 3 cercles (moteurs, sons, modes PERC/STAB/DRONE, effets, LFO, matrice, rythmes, couloirs, gamme ; le volume protégé) ; le même combo dans les 3 s = retour à l’état d’avant. <b>CHAOS</b> = les 3 DÉ ensemble (tout est tiré à l\'extrême : moteurs, sons, effets, rythmes, gamme ; définitif).<br>'
              'Clavier : blanches <b>MUTE</b> 1 2 3 · <b>DÉ</b> 1 2 3 (appui = nouveau tirage tout de suite, tenu = motif propre) · '
              '<b>SHIFT</b> −1 +1 (tourne le cercle choisi) ; noires (sur le cercle choisi, calées sur la grille) : tenir <b>ROLL</b> 1/8 triolet · 1/16 · BUILD UP (1/8 puis 1/16 puis 1/32, de plus en plus fort) · '
              '<b>RATCHET</b> ×2 ×3 · <b>DUB DELAY</b>, <b>DUB REVERB</b>, <b>DESTROY</b> · '
              'appui <b>DENSITÉ</b> − + (un coup de moins / de plus, gardé) · tenir <b>REVERSE</b>.</p>\n<p class="keys">', 1)
s = s.replace('<nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>',
              '<nav><a href="https://github.com/Forban5929/alter-fm1">ALTER (source)</a><a href="https://github.com/hugelton/Felucca">Felucca</a></nav>')
# EXPORT: the ALTER saves kept in this browser (the flash sectors holding ALTER's objects: magic ATLS ATLR ATLP), as a
# file and as text to copy, so Romain can hand them to Claude (a USER scene turned into a factory one)
EXPORT = r"""<p class="keys" id="altexp"><button type="button" id="expbtn">Envoyer mes sauvegardes ALTER à Claude</button>
<span id="expmsg"></span>
<textarea id="exptxt" readonly hidden style="width:100%;height:120px;font:11px ui-monospace,monospace;margin-top:8px"></textarea></p>
<script>
document.getElementById("expbtn").addEventListener("click", async () => {
  const msg = document.getElementById("expmsg");
  let raw = {};
  try { raw = JSON.parse(localStorage.getItem("felucca-emu-flash") || "{}"); } catch (e) {}
  const out = {};
  for (const [off, b64] of Object.entries(raw)) {
    let t = "";
    try { t = atob(b64); } catch (e) { continue; }
    if (t.includes("ATLS") || t.includes("ATLR") || t.includes("ATLP")) out[off] = b64;
  }
  if (!Object.keys(out).length) { msg.textContent = " Aucune sauvegarde ALTER dans ce navigateur."; return; }
  let db = null;
  try { db = await window.claude?.use?.("db"); } catch (e) {}
  if (db) {
    try {
      const when = new Date().toISOString();
      for (const [off, b64] of Object.entries(out)) await db.doc("saves/s" + off).set({ off: Number(off), b64, when });
      msg.textContent = " Envoyé : " + Object.keys(out).length + " secteurs. Dis-le à Claude dans la conversation.";
      return;
    } catch (e) { msg.textContent = " Envoi impossible (" + (e && e.code || e) + ") : copie le texte ci-dessous."; }
  }
  const ta = document.getElementById("exptxt");
  ta.hidden = false;
  ta.value = JSON.stringify({ alter_saves: 1, sectors: out });
  ta.select();
});
</script>
"""
s = s.replace("<footer>", EXPORT + "<footer>", 1)
open(p, "w", encoding="utf-8").write(s)
if len(sys.argv) > 3 and sys.argv[2] == "--artifact":
    for a in ("<!doctype html>\n", '<html lang="en">\n', "<head>\n", '<meta charset="utf-8">\n',
              '<meta name="viewport" content="width=device-width, initial-scale=1">\n', "</head>\n", "<body>\n", "</body>\n", "</html>\n"):
        s = s.replace(a, "", 1)
    open(sys.argv[3], "w", encoding="utf-8").write(s)
