#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ALTER's touches on the browser page (build/emu/index.html, made from web/emu/index.html): the name, a French
# line on how to play it, the source link. With --artifact OUT: also the page without its <html>/<head>/<body>
# wrapper, as claude.ai artifacts want it (the same files beside it: worklet.js, felucca.wasm, fonts/).
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = s.replace("<title>Felucca</title>", "<title>ALTER</title>")
s = s.replace('content="Felucca, the multi-engine synth firmware for the FM-1, running in your browser: the same code the instrument runs."',
              'content="ALTER, un instrument pour le M-VAVE FM-1 (fork de Felucca), dans le navigateur : le même code que la machine."')
s = s.replace('<p class="keys">',
              '<p class="keys"><b>ALTER</b> : <i>Switch on</i>, puis <b>PLAY</b> (touche <kbd>-</kbd>). '
              '3 cercles = 3 sons : <b>1 ALTER</b> (dehors), <b>2 PHYS</b>, <b>3 NOISE</b> (dedans). '
              'SELECT (<kbd>↑</kbd><kbd>↓</kbd>) = cercle choisi ; KNOB 1-4 = son du cercle choisi ; PRESETS = presets des 3 moteurs. '
              '<b>Pages rapides</b> (appui court = reste ouverte, même bouton ou HOME = fermée, tenu 2 s = se ferme au relâchement) : '
              '<b>SEQ</b> (<kbd>0</kbd>) LEN HITS ROT PROB · <b>ENV</b> (<kbd>3</kbd>) ATK DEC SUS REL · <b>FX</b> (<kbd>1</kbd>) DIST INSERT DELAY REVERB · '
              '<b>LFO</b> (<kbd>4</kbd>) · <b>EDIT</b> (<kbd>5</kbd>) les 4 autres réglages du moteur · <b>SCL</b> (<kbd>2</kbd>) GAMME NOTE GLIDE DOSE · '
              '<b>GLO</b> (<kbd>6</kbd>) TEMPO VOLUME PAN SWING. <b>OCT− + OCT+</b> (<kbd>[</kbd><kbd>]</kbd>) tenus + bouton = vraie page Felucca ; '
              '+ DÉ n = VERROUS du cercle n. HOME (<kbd>7</kbd>) = retour.<br>'
              '<b>ALGORITHM</b> (<kbd>←</kbd><kbd>→</kbd>) = mode d\'enveloppe du cercle choisi : FREE · PERC · STAB · DRONE. '
              '<b>CHAOS</b> = les 3 DÉ ensemble (tout est tiré à l\'extrême : moteurs, sons, effets, rythmes, gamme ; définitif).<br>'
              'Clavier : blanches <b>MUTE</b> 1 2 3 · <b>DÉ</b> 1 2 3 (appui = nouveau tirage tout de suite, tenu = motif propre) · '
              '<b>SHIFT</b> −1 +1 (tourne le cercle choisi) ; noires (sur le cercle choisi, calées sur la grille) : tenir <b>ROLL</b> 1/16 1/32 triolet · '
              '<b>RATCHET</b> ×2 ×3 · <b>DUB DELAY</b>, <b>DUB REVERB</b>, <b>DESTROY</b> · '
              'appui <b>DENSITÉ</b> − + (un coup de moins / de plus, gardé) · tenir <b>REVERSE</b>.</p>\n<p class="keys">', 1)
s = s.replace('<nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>',
              '<nav><a href="https://github.com/Forban5929/alter-fm1">ALTER (source)</a><a href="https://github.com/hugelton/Felucca">Felucca</a></nav>')
open(p, "w", encoding="utf-8").write(s)
if len(sys.argv) > 3 and sys.argv[2] == "--artifact":
    for a in ("<!doctype html>\n", '<html lang="en">\n', "<head>\n", '<meta charset="utf-8">\n',
              '<meta name="viewport" content="width=device-width, initial-scale=1">\n', "</head>\n", "<body>\n", "</body>\n", "</html>\n"):
        s = s.replace(a, "", 1)
    open(sys.argv[3], "w", encoding="utf-8").write(s)
