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
              '<p class="keys"><b>ALTER</b> : <i>Switch on</i>, puis <b>PLAY</b> (ou la touche <kbd>-</kbd>) : les 3 cercles jouent ALTER. '
              'KNOB 1-4 = MRPH, SPRD, HARM, MODE. PLAY à nouveau = stop.</p>\n<p class="keys">', 1)
s = s.replace('<nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>',
              '<nav><a href="https://github.com/Forban5929/alter-fm1">ALTER (source)</a><a href="https://github.com/hugelton/Felucca">Felucca</a></nav>')
open(p, "w", encoding="utf-8").write(s)
if len(sys.argv) > 3 and sys.argv[2] == "--artifact":
    for a in ("<!doctype html>\n", '<html lang="en">\n', "<head>\n", '<meta charset="utf-8">\n',
              '<meta name="viewport" content="width=device-width, initial-scale=1">\n', "</head>\n", "<body>\n", "</body>\n", "</html>\n"):
        s = s.replace(a, "", 1)
    open(sys.argv[3], "w", encoding="utf-8").write(s)
