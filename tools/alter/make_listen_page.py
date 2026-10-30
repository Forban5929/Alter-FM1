#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Builds the listening page (one HTML file, the clips embedded as MP3) from build/alter:
#   mp3/<clip>.mp3 (ffmpeg from wav/), rings.txt (alter_seq_demo's output: "<clip> <rings>")
#   python3 tools/alter/make_listen_page.py build/alter  ->  build/alter/alter_ecoute.html
import base64, html, math, os, sys

D = sys.argv[1] if len(sys.argv) > 1 else "build/alter"

SECTIONS = [
    ("Le séquenceur : 3 cercles", "seq", [
        ("16_seq_base", "BASE", "LIQUID", "HITS en noir, GHOST en doux, ACCENT renforce certains coups. Les trois cercles font 16 et 8 pas."),
        ("17_seq_polyrythmie", "POLYRYTHMIE", "METAL", "Cercles de 16, 12 et 5 pas : ils tournent les uns contre les autres, le motif complet ne revient qu'après 15 mesures."),
        ("18_seq_changements_live", "CHANGEMENTS LIVE", "SKIN", "Des réglages changés au milieu des mesures 2, 4 et 6 : on ne les entend qu'au début du cycle suivant, sans cassure."),
        ("19_seq_probabilite", "PROBABILITÉ", "LIQUID", "Chaque cercle sous 100 % : des coups sautent au hasard, le groove respire sans changer de motif."),
    ]),
    ("Le dé", "de", [
        ("20_de_dose_faible", "DOSE FAIBLE", "SKIN", "Le dé sur HITS, dose basse, gamme pentatonique mineure : petites variations. Nouveau tirage toutes les 2 mesures."),
        ("21_de_dose_forte", "DOSE FORTE", "METAL", "Dé sur les 3 cercles, dose haute, gamme phrygienne : hauteurs, timbres et intensités changent franchement."),
        ("22_de_verrous", "VERROUS", "METAL", "Même réglage, hauteur et glide verrouillés : seuls le timbre et l'intensité bougent."),
        ("23_acid_glide", "ACID + GLIDE", "SKIN", "Dé sur HITS en gamme mineure, avec glides : les notes glissent sans nouvelle attaque. MORPH et SPREAD verrouillés."),
    ]),
    ("Le son : les 6 presets", "presets", [
        ("01_preset_THUD", "THUD", "LIQUID", "Coup grave avec chute de hauteur rapide : un kick métallique sec."),
        ("02_preset_BELL", "BELL", "SKIN", "Cloche inharmonique jouée sur quelques notes, les aigus restent longtemps."),
        ("03_preset_CLANG", "CLANG", "METAL", "Frappe de métal saturée par la modulation croisée, avec un peu de bruit."),
        ("04_preset_SNAP", "SNAP", "LIQUID", "Burst de bruit fort sur une base tonale courte : esprit caisse claire."),
        ("05_preset_STAB", "STAB", "SKIN", "Stab carré et court, les aigus meurent vite. Joué sur quelques notes."),
        ("06_preset_RUST", "RUST", "METAL", "Drone tenu, métallique, qui dérive lentement."),
    ]),
    ("Le son : les 3 modes", "modes", [
        ("07_mode_SKIN_dtun", "SKIN", "SKIN", "AMOUNT = DETUNE monte de 0 à fond : le son se met à battre et s'élargit."),
        ("08_mode_LIQUID_punch", "LIQUID", "LIQUID", "AMOUNT = PUNCH monte : la chute de hauteur devient plus profonde et plus longue."),
        ("09_mode_METAL_fm", "METAL", "METAL", "AMOUNT = FM DEPTH monte : de propre à chaotique."),
    ]),
    ("Le son : chaque bouton de 0 à fond", "sweeps", [
        ("10_sweep_MORPH", "MORPH", "SKIN", "Sinus, puis triangle, scie, carré : de plus en plus brillant."),
        ("11_sweep_SPREAD", "SPREAD", "SKIN", "Harmonique, puis métallique, puis très dissonant."),
        ("12_sweep_HARMONICS", "HARMONICS", "SKIN", "D'un seul oscillateur grave aux six à plein volume."),
        ("13_sweep_NOISE", "NOISE", "LIQUID", "Le burst de bruit à l'attaque grossit et s'allonge."),
        ("14_sweep_TILT", "TILT", "SKIN", "À gauche, les aigus meurent d'abord ; à droite, ce sont les graves."),
        ("15_sweep_DRIFT_drone", "DRIFT", "METAL", "Drone : les six oscillateurs dérivent de plus en plus."),
    ]),
]

rings = {}
p = os.path.join(D, "rings.txt")
if os.path.exists(p):
    for line in open(p):
        k, v = line.split()
        rings[k] = [(int(r.split(":")[0]), r.split(":")[1] if ":" in r else "") for r in v.split("|")]


def ring_svg(rs):
    # three concentric circles, outer = HITS; a dot per step, filled where it hits
    out = ['<svg class="rings" viewBox="0 0 120 120" role="img" aria-label="Les 3 cercles">']
    for i, (n, bits) in enumerate(rs):
        r = 50 - i * 15
        cls = ["hits", "ghost", "acc"][i]
        out.append(f'<circle class="track" cx="60" cy="60" r="{r}"/>')
        for s in range(n):
            a = -math.pi / 2 + 2 * math.pi * s / n
            x, y = 60 + r * math.cos(a), 60 + r * math.sin(a)
            on = bits[s] == "1"
            out.append(f'<circle class="dot {cls}{" on" if on else ""}" cx="{x:.1f}" cy="{y:.1f}" r="{3.6 if on else 1.6}"/>')
    out.append("</svg>")
    return "".join(out)


sec = []
for title, sid, items in SECTIONS:
    rows = []
    for f, name, mode, desc in items:
        mp = os.path.join(D, "mp3", f + ".mp3")
        if not os.path.exists(mp):
            continue
        b = base64.b64encode(open(mp, "rb").read()).decode()
        svg = ring_svg(rings[f]) if f in rings else ""
        rows.append(
            f'<li class="clip{" has-rings" if svg else ""}" data-mode="{mode.lower()}">{svg}<div class="body">'
            f'<div class="meta"><span class="name">{html.escape(name)}</span><span class="mode">{mode}</span></div>'
            f'<p>{html.escape(desc)}</p><audio controls preload="none" id="a-{f}" src="data:audio/mpeg;base64,{b}"></audio></div></li>')
    sec.append(f'<section aria-labelledby="h-{sid}"><h2 id="h-{sid}">{title}</h2><ul>{"".join(rows)}</ul></section>')

page = f'''<title>ALTER, premiers sons</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wdth,wght@75..100,500..800&family=IBM+Plex+Sans:wght@400;500&family=IBM+Plex+Mono:wght@500&display=swap">
<style>
/* Layout: one listening column; sequencer clips carry their three circles, mode coded by a chip like a module LED */
:root {{
  --bg:#eef0ee; --panel:#ffffff; --fg:#17201d; --muted:#5b6762; --line:#d3d9d5;
  --skin:#1d7f6e; --liquid:#2a5fc9; --metal:#b4561f;
  --hits:#17201d; --ghost:#8a9a93; --acc:#d23b2a;
  --display:"Archivo",system-ui,sans-serif; --body:"IBM Plex Sans",system-ui,sans-serif; --mono:"IBM Plex Mono",ui-monospace,monospace;
}}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{
  --bg:#121715; --panel:#1a211e; --fg:#e4ebe7; --muted:#97a49e; --line:#2c3632;
  --skin:#49c2a9; --liquid:#7aa2f7; --metal:#f0915a; --hits:#e4ebe7; --ghost:#6d7c75; --acc:#ff6b55; color-scheme:dark }} }}
:root[data-theme="dark"] {{ --bg:#121715; --panel:#1a211e; --fg:#e4ebe7; --muted:#97a49e; --line:#2c3632;
  --skin:#49c2a9; --liquid:#7aa2f7; --metal:#f0915a; --hits:#e4ebe7; --ghost:#6d7c75; --acc:#ff6b55; color-scheme:dark }}
body {{ background:var(--bg); color:var(--fg); font:15px/1.55 var(--body);
  padding-left:max(16px,env(safe-area-inset-left,0px)); padding-right:max(16px,env(safe-area-inset-right,0px)); padding-block:40px 64px }}
.wrap {{ max-width:780px; margin:0 auto; display:grid; gap:40px }}
header {{ display:grid; gap:12px }}
h1 {{ font-family:var(--display); font-stretch:75%; font-weight:800; font-size:clamp(2.6rem,9vw,4.2rem); line-height:.95; letter-spacing:.01em; margin:0; text-wrap:balance }}
.lede {{ max-width:62ch; margin:0; color:var(--muted) }}
.spec {{ font-family:var(--mono); font-size:12px; letter-spacing:.06em; text-transform:uppercase; color:var(--muted); display:flex; flex-wrap:wrap; gap:6px 18px }}
.legend {{ display:flex; flex-wrap:wrap; gap:6px 18px; font-family:var(--mono); font-size:12px; letter-spacing:.06em; color:var(--muted) }}
.legend span::before {{ content:""; display:inline-block; width:9px; height:9px; border-radius:50%; margin-right:6px; vertical-align:-1px; background:var(--c) }}
h2 {{ font-family:var(--display); font-stretch:85%; font-weight:700; font-size:1.25rem; margin:0 0 12px; text-wrap:balance }}
ul {{ list-style:none; margin:0; padding:0; display:grid; gap:10px }}
.clip {{ background:var(--panel); border:1px solid var(--line); border-radius:6px; padding:14px 16px; display:grid; gap:14px; min-width:0 }}
.clip.has-rings {{ grid-template-columns:96px minmax(0,1fr); align-items:center }}
.body {{ display:grid; gap:6px; min-width:0 }}
.rings {{ width:96px; height:96px }}
.rings .track {{ fill:none; stroke:var(--line); stroke-width:1 }}
.rings .dot {{ fill:var(--line) }}
.rings .dot.hits.on {{ fill:var(--hits) }} .rings .dot.ghost.on {{ fill:var(--ghost) }} .rings .dot.acc.on {{ fill:var(--acc) }}
.meta {{ display:flex; align-items:center; gap:10px; flex-wrap:wrap }}
.name {{ font-family:var(--display); font-stretch:80%; font-weight:700; font-size:1.05rem; letter-spacing:.04em }}
.mode {{ font-family:var(--mono); font-size:11px; letter-spacing:.08em; padding:2px 8px; border-radius:99px; border:1px solid currentColor }}
.mode::before {{ content:""; display:inline-block; width:7px; height:7px; border-radius:50%; background:currentColor; margin-right:6px; vertical-align:1px }}
[data-mode="skin"] .mode {{ color:var(--skin) }} [data-mode="liquid"] .mode {{ color:var(--liquid) }} [data-mode="metal"] .mode {{ color:var(--metal) }}
.clip p {{ margin:0; color:var(--muted); max-width:62ch }}
audio {{ width:100%; height:36px; margin-top:4px }}
.note {{ border-top:1px solid var(--line); padding-top:16px; color:var(--muted); font-size:13px; max-width:65ch }}
@media (max-width:440px) {{ .clip.has-rings {{ grid-template-columns:1fr }} }}
</style>
<div class="wrap">
<header>
<div class="spec"><span>Moteur ALTER</span><span>6 oscillateurs</span><span>3 cercles euclidiens</span><span>132 BPM</span></div>
<h1>ALTER, premiers sons</h1>
<p class="lede">Rendus par le vrai code du moteur et du séquenceur, calculés en nombres entiers comme sur le FM-1. Son brut, sans les effets de Felucca (disto, fold, delay, reverb).</p>
<div class="legend"><span style="--c:var(--hits)">HITS</span><span style="--c:var(--ghost)">GHOST</span><span style="--c:var(--acc)">ACCENT</span><span>· les cercles montrent le motif à la fin de l'extrait</span></div>
</header>
{"".join(sec)}
<p class="note">L'enveloppe utilisée ici imite celle de Felucca sans être la vraie. Le séquenceur, lui, est le code qui ira dans le firmware.</p>
</div>'''
open(os.path.join(D, "alter_ecoute.html"), "w").write(page)
print(os.path.getsize(os.path.join(D, "alter_ecoute.html")) // 1024, "KB")
