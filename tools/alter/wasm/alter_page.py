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
DETAIL = ('<b>ALTER</b> : <i>Switch on</i>, puis <b>PLAY</b> (touche <kbd>-</kbd>). '
              '3 cercles = 3 sons : <b>1 ALTER</b> (dehors), <b>2 PHYS</b>, <b>3 NOISE</b> (dedans). '
              'SELECT (<kbd>↑</kbd><kbd>↓</kbd>) = cercle choisi ; KNOB 1-4 = son du cercle choisi ; PRESETS = sons des 3 moteurs. <b>SAVE</b> (<kbd>8</kbd>) = page SCÈNES / PATTERNS (SELECT choisit, ALGORITHM change d\'onglet, SAVE charge, SAVE tenu 1 s enregistre dans un slot USER ; NEW et usine protégés). '
              '<b>Boutons de page, tous pareils</b> : <b>appui court</b> = page rapide (les 4 KNOBS changent sur les cercles ; même bouton ou HOME = fermée) ; <b>tenu 0,5 s</b> = le <b>menu</b> du bouton (reste ouvert ; même bouton tapé, HOME ou un autre bouton = sortie). '
              '<b>SEQ</b> (<kbd>0</kbd>) court = les 4 réglages du rythme ; <b>menu SEQ</b> : SEQ tenu + ALGORITHM = moteur du cercle EUCLID · TURING ; touches blanches = les 16 pas : en tenir + KNOB 1 = leur valeur, deux pas tenus + KNOB 2 = SHUFFLE (tout l’onglet tiré au hasard, les 16 pas) ; OCT− + OCT+ tenus + touche noire d’un onglet = cet onglet remis à plat, KNOB 3 / KNOB 4 = bas / haut d’une plage (chaque passage tire une valeur au hasard dedans) ; ALGORITHM = couloir VELO · CHANCE · DELAY · REVERB · PITCH · RATCH (×1 à ×4 par pas) ; touches noires = raccourcis (les 6 couloirs, puis EUCLID/TURING, cercle précédent, cercle suivant) ; la lumière des touches blanches suit le pas joué. <b>TURING</b> en jeu : SHIFT −1 = motif précédent, SHIFT +1 = motif suivant, la touche blanche d’après = LOCK (figé) · <b>ENV</b> (<kbd>3</kbd>) court = 4 réglages, <b>menu = l’enveloppe dessinée</b>, selon le mode du cercle (ALGORITHM) : FREE = ATK DEC SUS REL · PERC = ATTACK HOLD DECAY PUNCH · STAB = LENGTH SUSTAIN PLUCK CHORD · DRONE = SWELL BODY TAIL OPEN (chaque coup = une nouvelle couche douce, 2 au plus, la plus ancienne s’efface) · <b>FX</b> (<kbd>1</kbd>) court = DIST INSERT DELAY REVERB ; <b>menu FX</b> (le clavier joue) : ALGORITHM = onglet DIST · DELAY · REVERB · MASTER · FILTER, PRESETS = preset de l’onglet, SELECT = cercle (MASTER : la rangée LOW MID HIGH COMP / DUCK SRC SHAPE CLIP ; FILTER : la rangée FREQ RES TYPE DRIVE / MOD SRC FM MIX / SLOPE DETUNE MODTYP MORPH, OCT− = réglage A, OCT+ = réglage B, MORPH glisse de A à B) · '
              '<b>LFO</b> (<kbd>4</kbd>) court = le <b>LFO 1 du cercle choisi</b> : WAVE · RATE (division du tempo) · DEPTH · DEST ; une valeur bougée par un LFO montre sur sa barre une zone de la couleur du cercle (la plage du LFO) ; sans barre (PROB, enveloppe) : une petite vague à côté du nom ; <b>menu LFO</b> : onglets LFO 1 · LFO 2 · LFO 3 (les 3 LFO du cercle choisi, SELECT change de cercle) · GLOBAL (les 3 cercles ensemble) · MTX (matrice du cercle choisi : SELECT = case, KNOB 1 source, 2 destination, 3 quantité) ; la vague défile sur une grille d’une mesure (DEPTH change sa taille) ; ALGORITHM = onglet · <b>EDIT</b> (<kbd>5</kbd>) court = les 4 autres réglages du moteur, <b>menu = RÉGLAGES</b> (THEME : NIGHT sombre / DAY clair, KNOB 1) · <b>SCL</b> (<kbd>2</kbd>) court = GAMME NOTE ACCORD (OFF OCT 5TH TRIAD 7TH 9TH) DOSE, <b>menu = les gammes</b> (liste + clavier, KNOB 1 choisit) · '
              '<b>GLO</b> (<kbd>6</kbd>) TEMPO VOLUME PAN SWING (pas encore de menu : tenu = court). <b>OCT− + OCT+</b> (<kbd>[</kbd><kbd>]</kbd>) tenus + HOME = NEW SCENE (scène vide, tout de suite) ; + un autre bouton = vraie page Felucca ; '
              '+ DÉ n = VERROUS du cercle n. HOME (<kbd>7</kbd>) = retour. <b>REC</b> (<kbd>=</kbd>) = enregistre les knobs du cercle choisi sur ses pas : appui = REC allumé (point rouge), tout ce que tu tournes sur ce cercle (son, ENV, FX…) s’écrit au pas qui joue, par-dessus ; nouvel appui = arrêt, le mouvement rejoue en boucle, lissé d’un pas à l’autre ; REC tenu 1 s = efface le cercle choisi ; OCT− + OCT+ + REC = efface tout ; un DÉ ou un CHAOS efface aussi ; pas encore sauvé avec la scène.<br>'
              '<b>ALGORITHM</b> (<kbd>←</kbd><kbd>→</kbd>) = mode d\'enveloppe du cercle choisi : FREE · PERC · STAB · DRONE. '
              '<b>3 MUTE</b> ensemble = le cercle choisi tiré au hasard : longueur (4 à 16 pas), nombre de coups, couloirs VELO, CHANCE et PITCH, tout de suite (les mutes ne bougent pas). <b>CHAOS TOTAL</b> = les 3 touches CERCLE ensemble (3 premières noires) : tout sans limite sur les 3 cercles (moteurs, sons, modes PERC/STAB/DRONE, effets, LFO, matrice, rythmes, couloirs, gamme ; le volume protégé) ; le même combo dans les 3 s = retour à l’état d’avant. Après un hasard (DÉ, 3 MUTE, CHAOS, CHAOS TOTAL), chaque cercle <b>aligne son niveau</b> sur ses premiers coups (≈ 4 s), puis le garde ; ton VOLUME reste à toi ; un son ou une scène choisis à la main gardent leur niveau. <b>CHAOS</b> = les 3 DÉ ensemble (tout est tiré à l\'extrême : moteurs, sons, effets, rythmes, gamme ; définitif).<br>'
              'Clavier : blanches <b>MUTE</b> 1 2 3 · <b>DÉ</b> 1 2 3 (appui = nouveau tirage tout de suite, tenu = motif propre) · '
              '<b>SHIFT</b> −1 +1 (tourne le cercle choisi) ; noires, de gauche à droite : <b>CERCLE</b> 1 2 3 (choisit le cercle, comme SELECT) · '
              'appui <b>DENSITÉ</b> − + (un coup de moins / de plus, gardé) · tenir, sur le cercle choisi, calé sur la grille : <b>DUB DELAY</b>, <b>DUB REVERB</b>, <b>DESTROY</b> · '
              '<b>ROLL</b> 1/16 · 1/32 · appui <b>TRIGGER</b> (un coup du cercle choisi tout de suite, comme un pad, quel que soit son son ; avec REC tenu ou allumé, le coup est aussi ajouté au cercle sur le pas le plus proche et rejoue à chaque tour, sauvé avec la scène ; REC tenu 1 s efface aussi ces coups, OCT− + OCT+ + REC efface tout). <b>Avec REC allumé, les effets tenus s’enregistrent aussi</b> sur les pas où ils passent et rejouent à chaque tour : ROLL 1/16 ajoute des coups, ROLL 1/32 ajoute des coups doublés, DUB DELAY, DUB REVERB et DESTROY s’appliquent à ces pas ; sauvé avec la scène ; REC tenu 1 s efface. Les 5 dernières blanches : libres.')
SVG = '<svg class="altkb" viewBox="0 0 680 290" role="img" aria-label="Le clavier en live"><text x="112" y="40" text-anchor="middle" class="g">CERCLE 1 2 3</text><text x="245" y="40" text-anchor="middle" class="g">DENSITÉ − +</text><text x="378" y="40" text-anchor="middle" class="g">DUB DLY · REV · DESTROY</text><text x="511" y="40" text-anchor="middle" class="g">ROLL 1/16 · 1/32</text><text x="611" y="40" text-anchor="middle" class="g">TRIGGER</text><rect x="36" y="56" width="38" height="150" rx="3" fill="#7F77DD" fill-opacity=".28" stroke="#7F77DD"/><text x="55.0" y="178" text-anchor="middle" class="n">1</text><text x="55.0" y="198" text-anchor="middle" class="k">Z</text><rect x="74" y="56" width="38" height="150" rx="3" fill="#7F77DD" fill-opacity=".28" stroke="#7F77DD"/><text x="93.0" y="178" text-anchor="middle" class="n">2</text><text x="93.0" y="198" text-anchor="middle" class="k">X</text><rect x="112" y="56" width="38" height="150" rx="3" fill="#7F77DD" fill-opacity=".28" stroke="#7F77DD"/><text x="131.0" y="178" text-anchor="middle" class="n">3</text><text x="131.0" y="198" text-anchor="middle" class="k">C</text><rect x="150" y="56" width="38" height="150" rx="3" fill="#888780" fill-opacity=".28" stroke="#888780"/><text x="169.0" y="198" text-anchor="middle" class="k">V</text><rect x="188" y="56" width="38" height="150" rx="3" fill="#1D9E75" fill-opacity=".28" stroke="#1D9E75"/><text x="207.0" y="178" text-anchor="middle" class="n">1</text><text x="207.0" y="198" text-anchor="middle" class="k">B</text><rect x="226" y="56" width="38" height="150" rx="3" fill="#1D9E75" fill-opacity=".28" stroke="#1D9E75"/><text x="245.0" y="178" text-anchor="middle" class="n">2</text><text x="245.0" y="198" text-anchor="middle" class="k">N</text><rect x="264" y="56" width="38" height="150" rx="3" fill="#1D9E75" fill-opacity=".28" stroke="#1D9E75"/><text x="283.0" y="178" text-anchor="middle" class="n">3</text><text x="283.0" y="198" text-anchor="middle" class="k">M</text><rect x="302" y="56" width="38" height="150" rx="3" fill="#888780" fill-opacity=".28" stroke="#888780"/><text x="321.0" y="198" text-anchor="middle" class="k">,</text><rect x="340" y="56" width="38" height="150" rx="3" fill="#1D9E75" fill-opacity=".28" stroke="#1D9E75"/><text x="359.0" y="178" text-anchor="middle" class="n">−1</text><text x="359.0" y="198" text-anchor="middle" class="k">.</text><rect x="378" y="56" width="38" height="150" rx="3" fill="#1D9E75" fill-opacity=".28" stroke="#1D9E75"/><text x="397.0" y="178" text-anchor="middle" class="n">+1</text><text x="397.0" y="198" text-anchor="middle" class="k">/</text><rect x="416" y="56" width="38" height="150" rx="3" fill="#1D9E75" fill-opacity=".28" stroke="#1D9E75"/><text x="435.0" y="178" text-anchor="middle" class="n">L</text><text x="435.0" y="198" text-anchor="middle" class="k">Q</text><rect x="454" y="56" width="38" height="150" rx="3" class="wk"/><text x="473.0" y="198" text-anchor="middle" class="k">W</text><rect x="492" y="56" width="38" height="150" rx="3" class="wk"/><text x="511.0" y="198" text-anchor="middle" class="k">R</text><rect x="530" y="56" width="38" height="150" rx="3" class="wk"/><text x="549.0" y="198" text-anchor="middle" class="k">Y</text><rect x="568" y="56" width="38" height="150" rx="3" class="wk"/><text x="587.0" y="198" text-anchor="middle" class="k">U</text><rect x="606" y="56" width="38" height="150" rx="3" class="wk"/><text x="625.0" y="198" text-anchor="middle" class="k">O</text><rect x="61" y="56" width="26" height="92" rx="3" fill="#7F77DD" stroke="#7F77DD"/><text x="74" y="118" text-anchor="middle" class="nb">1</text><text x="74" y="140" text-anchor="middle" class="kb">S</text><rect x="99" y="56" width="26" height="92" rx="3" fill="#7F77DD" stroke="#7F77DD"/><text x="112" y="118" text-anchor="middle" class="nb">2</text><text x="112" y="140" text-anchor="middle" class="kb">D</text><rect x="137" y="56" width="26" height="92" rx="3" fill="#7F77DD" stroke="#7F77DD"/><text x="150" y="118" text-anchor="middle" class="nb">3</text><text x="150" y="140" text-anchor="middle" class="kb">F</text><rect x="213" y="56" width="26" height="92" rx="3" fill="#1D9E75" stroke="#1D9E75"/><text x="226" y="118" text-anchor="middle" class="nb">−</text><text x="226" y="140" text-anchor="middle" class="kb">H</text><rect x="251" y="56" width="26" height="92" rx="3" fill="#1D9E75" stroke="#1D9E75"/><text x="264" y="118" text-anchor="middle" class="nb">+</text><text x="264" y="140" text-anchor="middle" class="kb">J</text><rect x="327" y="56" width="26" height="92" rx="3" fill="#D85A30" stroke="#D85A30"/><text x="340" y="140" text-anchor="middle" class="kb">L</text><rect x="365" y="56" width="26" height="92" rx="3" fill="#D85A30" stroke="#D85A30"/><text x="378" y="140" text-anchor="middle" class="kb">;</text><rect x="403" y="56" width="26" height="92" rx="3" fill="#D85A30" stroke="#D85A30"/><text x="416" y="140" text-anchor="middle" class="kb">’</text><rect x="479" y="56" width="26" height="92" rx="3" fill="#D85A30" stroke="#D85A30"/><text x="492" y="140" text-anchor="middle" class="kb">E</text><rect x="517" y="56" width="26" height="92" rx="3" fill="#D85A30" stroke="#D85A30"/><text x="530" y="140" text-anchor="middle" class="kb">T</text><rect x="593" y="56" width="26" height="92" rx="3" fill="#D85A30" stroke="#D85A30"/><text x="606" y="140" text-anchor="middle" class="kb">I</text><text x="93" y="226" text-anchor="middle" class="g">MUTE 1 2 3</text><text x="245" y="226" text-anchor="middle" class="g">DÉ 1 2 3</text><text x="378" y="226" text-anchor="middle" class="g">TOUR −1 +1</text><text x="436" y="244" text-anchor="middle" class="g">LOCK</text><text x="549" y="226" text-anchor="middle" class="g">libres</text><rect x="60" y="262" width="12" height="12" rx="2" fill="#7F77DD"/><text x="78" y="272" text-anchor="start" class="g">quel cercle</text><rect x="220" y="262" width="12" height="12" rx="2" fill="#1D9E75"/><text x="238" y="272" text-anchor="start" class="g">son rythme</text><rect x="380" y="262" width="12" height="12" rx="2" fill="#D85A30"/><text x="398" y="272" text-anchor="start" class="g">effet tenu (cercle choisi)</text></svg>'
GUIDE = ("<style>.altg{max-width:680px;margin:20px auto 0;color:var(--fg);text-align:left;font-size:14px;line-height:1.5}"
         ".altg h3{font-size:12px;letter-spacing:.08em;color:var(--dim);margin:18px 0 6px;font-weight:600}"
         ".altg ul{margin:0;padding-left:18px}.altg li{margin:3px 0}"
         ".altkb{width:100%;height:auto;display:block}.altkb text{font:12px system-ui,sans-serif;fill:var(--fg)}"
         ".altkb .g{fill:var(--dim)}.altkb .n{font-weight:600}.altkb .k{font:11px ui-monospace,monospace;fill:var(--dim)}"
         ".altkb .nb,.altkb .kb{fill:#fff}.altkb .nb{font-weight:600}.altkb .kb{font:11px ui-monospace,monospace;opacity:.8}"
         ".altkb .wk{fill:none;stroke:var(--rule)}.altg details{margin-top:16px;color:var(--dim);font-size:13px}.altg summary{cursor:pointer}</style>"
         '<div class="altg">'
         "<p><b>Démarrer</b> : <i>Switch on</i>, puis <b>PLAY</b> <kbd>-</kbd>. 3 cercles = 3 sons : <b>1 ALTER</b> (dehors), <b>2 PHYS</b>, <b>3 NOISE</b> (dedans).</p>"
         "<h3>CLAVIER (la lettre = touche de l’ordinateur)</h3>" + SVG +
         "<h3>BOUTONS</h3><ul>"
         "<li><b>SELECT</b> <kbd>↑</kbd><kbd>↓</kbd> cercle choisi · <b>KNOB 1-4</b> son du cercle · <b>PRESETS</b> changer de son · <b>ALGORITHM</b> <kbd>←</kbd><kbd>→</kbd> FREE / PERC / STAB / DRONE</li>"
         "<li><b>Boutons de page</b> FX <kbd>1</kbd> SCL <kbd>2</kbd> ENV <kbd>3</kbd> LFO <kbd>4</kbd> EDIT <kbd>5</kbd> GLO <kbd>6</kbd> SEQ <kbd>0</kbd> : <b>appui</b> = 4 réglages rapides, <b>tenu</b> = menu · <b>HOME</b> <kbd>7</kbd> = retour</li>"
         "<li><b>SAVE</b> <kbd>8</kbd> scènes (SAVE tenu 1 s = enregistrer) · <b>REC</b> <kbd>=</kbd> enregistre les knobs sur les pas (tenu 1 s = effacer)</li>"
         "</ul><h3>COMBOS</h3><ul>"
         "<li><b>3 MUTE</b> = rythme du cercle au hasard · <b>3 DÉ</b> = CHAOS · <b>3 CERCLE</b> = CHAOS TOTAL (refait en moins de 3 s = annulé)</li>"
         "<li><b>OCT− + OCT+</b> <kbd>[</kbd><kbd>]</kbd> tenus + HOME = scène vide · + DÉ n = verrous du cercle n</li>"
         "</ul><details><summary>Tout le détail</summary><p>" + DETAIL + "</p></details></div>")
s = s.replace('<p class="keys">', GUIDE + '\n<p class="keys">', 1)
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
