#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ALTER's listening page: the scenes and the sounds of firmware/src/alter_sounds.c, rendered by the real firmware
# (node tools/alter/alter_render.mjs build/emu/felucca.wasm build/alter -> build/alter/wav/), as one HTML file with
# the clips embedded as MP3 (ffmpeg):
#   python3 tools/alter/make_listen_page.py build/alter  ->  build/alter/alter_ecoute.html
import base64, html, os, subprocess, sys

D = sys.argv[1] if len(sys.argv) > 1 else "build/alter"

SCENES = [
    ("scene_1_TECHNO", "TECHNO", "132 BPM · phrygien", "PERC · STAB · TEXTURE",
     "KICK (plein, gras) en PERC sur les noires, le 4e envoyé à 40 % dans le delay pour le groove, STAB en accord (triade) sur 3 coups décalés, CRUSH en texture sur 11 pas sur 16 avec un peu de hasard."),
    ("scene_2_HOUSE", "HOUSE", "122 BPM · dorien · swing léger", "KICK · ACCORDS · OPEN HAT",
     "Deep house : KICK sur les 4 temps, WARM en accords mineurs chauds sur les contretemps, OPEN HAT entre les kicks."),
    ("scene_3_AMBIENT", "AMBIENT", "70 BPM · mineur", "NAPPE · NAPPE · PLUIE",
     "Deux nappes douces PAD (cercles de 32 et 24 pas, une quinte d'écart) qui se fondent, une pluie RAIN très discrète."),
    ("scene_4_INDUS", "INDUS", "128 BPM · phrygien", "MÉTAL · BRUIT · GONG",
     "ANVIL en PERC, METAL (registre à décalage) en PERC sur 7 pas, GONG qui sonne librement."),
]
SOUNDS = {
    "ALTER": [
        ("THUD", "Coup grave à chute de hauteur rapide : un kick métallique sec."),
        ("ANVIL", "Frappe d'enclume : 6 oscillateurs en modulation croisée, un peu de bruit."),
        ("STAB", "Carré court aux aigus qui meurent vite ; fait pour les accords."),
        ("PIPES", "Harmoniques pures et longues, presque un orgue frappé ; joue bien en accord."),
        ("RUST", "Drone métallique tenu qui dérive lentement."),
        ("GRIND", "Texture saturée : tout au maximum, modulation forte, dérive."),
        ("KICK", "Kick plein : sinus grave, chute de hauteur courte, corps long, un peu de disto pour le gras."),
        ("WARM", "Accord doux et chaud, légèrement désaccordé : les accords de la deep house."),
        ("PAD", "Nappe lente et sombre qui respire : pour les drones paisibles."),
    ],
    "PHYS": [
        ("TOMS", "Peau de tambour frappée, la hauteur chute juste après le coup."),
        ("GONG", "Barre métallique inharmonique qui sonne longtemps."),
        ("BARS", "Lames accordées (rapports harmoniques) : marimba de métal, idéal en accord."),
        ("SCRAPE", "Métal frotté à l'archet, avec le grain du frottement mélangé au son."),
        ("BUZZ", "Corde à chevalet courbe : sitar sale, bourdonnant."),
        ("SITAR", "Corde et 3 cordes sympathiques qui résonnent avec elle : un drone vivant."),
    ],
    "NOISE": [
        ("HAT", "Bruit blanc filtré dans l'aigu, très court."),
        ("SNARE", "Bruit plus grave et plus large, avec impulsions : une caisse claire de bruit."),
        ("METAL", "Une séquence de registre à décalage répétée à la hauteur de la note : un timbre métallique accordé."),
        ("CRUSH", "Bruit échantillonné-bloqué et réduit en bits."),
        ("RAIN", "Crépitements filtrés par une résonance : pluie, gouttes."),
        ("WIND", "Bruit coloré lent qui dérive : vent, souffle."),
        ("ARCADE", "Registre court cadencé au-dessus de la note : buzz de console 8 bits."),
        ("OPEN HAT", "Charleston ouverte : le bruit aigu qui sonne plus longtemps."),
    ],
}
ENG_ABOUT = {
    "ALTER": "6 oscillateurs frappés ensemble (MRPH SPRD HARM MODE).",
    "PHYS": "Modèles physiques : lames, cordes, peaux (MODEL STRC BRIT DAMP).",
    "NOISE": "Bruits analogiques et numériques à travers un filtre résonant.",
}
ENG_COL = {"ALTER": "c1", "PHYS": "c2", "NOISE": "c3"}


def mp3(name):
    w, m = f"{D}/wav/{name}.wav", f"{D}/mp3/{name}.mp3"
    os.makedirs(f"{D}/mp3", exist_ok=True)
    if not os.path.exists(m) or os.path.getmtime(m) < os.path.getmtime(w):
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", w, "-codec:a", "libmp3lame", "-b:a", "112k", m], check=True)
    return "data:audio/mpeg;base64," + base64.b64encode(open(m, "rb").read()).decode()


def files():
    return sorted(f[:-4] for f in os.listdir(f"{D}/wav") if f.startswith("sound_") and f.endswith(".wav"))


e = html.escape
parts = []
for f, name, meta, roles, about in SCENES:
    parts.append(f"""<article class="scene">
  <header><h3>{e(name)}</h3><p class="meta">{e(meta)}</p></header>
  <p class="roles"><span class="c1">{e(roles.split(' · ')[0])}</span><span class="c2">{e(roles.split(' · ')[1])}</span><span class="c3">{e(roles.split(' · ')[2])}</span></p>
  <p>{e(about)}</p>
  <audio controls preload="none" src="{mp3(f)}"></audio>
</article>""")
scenes_html = "\n".join(parts)

by_name = {f.split("_", 3)[3]: f for f in files()}
eng_html = []
for eng, lst in SOUNDS.items():
    rows = []
    for name, about in lst:
        f = by_name.get(name)
        if not f:
            continue
        rows.append(f"""<li><div class="txt"><h4>{e(name)}</h4><p>{e(about)}</p></div><audio controls preload="none" src="{mp3(f)}"></audio></li>""")
    eng_html.append(f"""<section class="eng {ENG_COL[eng]}">
  <h3>{e(eng)}</h3><p class="lede">{e(ENG_ABOUT[eng])}</p>
  <ul>{''.join(rows)}</ul>
</section>""")

page = f"""<title>ALTER Écoute</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;600&family=IBM+Plex+Sans:wght@400;500&family=IBM+Plex+Mono:wght@400&display=swap">
<style>
/* layout: one narrow column like the FM-1's screen; three accents = the three circles' colours */
:root {{
  --bg: #f1f0ee; --panel: #ffffff; --fg: #18181a; --dim: #5d5c61; --rule: #d6d4d1;
  --c1: #c9562f; --c2: #1f8a6a; --c3: #a8740f;
  --display: "Chakra Petch", "Arial Narrow", sans-serif;
  --body: "IBM Plex Sans", system-ui, sans-serif;
  --mono: "IBM Plex Mono", ui-monospace, monospace;
}}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{
  --bg: #0f0f11; --panel: #18181b; --fg: #ebebec; --dim: #9a99a0; --rule: #2c2c31;
  --c1: #f0997b; --c2: #5dcaa5; --c3: #fac775; color-scheme: dark; }} }}
:root[data-theme="dark"] {{
  --bg: #0f0f11; --panel: #18181b; --fg: #ebebec; --dim: #9a99a0; --rule: #2c2c31;
  --c1: #f0997b; --c2: #5dcaa5; --c3: #fac775; color-scheme: dark; }}
body {{ background: var(--bg); color: var(--fg); font: 15px/1.55 var(--body); }}
main {{ max-width: 760px; margin: 0 auto; padding-block: 32px 48px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px));
  display: grid; gap: 40px; }}
h1, h2, h3, h4 {{ font-family: var(--display); font-weight: 600; margin: 0; text-wrap: balance; letter-spacing: .02em; }}
h1 {{ font-size: 2.1rem; }}
h2 {{ font-size: .8rem; text-transform: uppercase; letter-spacing: .14em; color: var(--dim); font-weight: 500;
  border-bottom: 1px solid var(--rule); padding-bottom: 8px; }}
p {{ margin: 0; max-width: 65ch; }}
.intro {{ display: grid; gap: 10px; }}
.intro .rings {{ display: flex; gap: 6px; align-items: center; }}
.intro .rings i {{ width: 10px; height: 10px; border-radius: 50%; display: block; }}
.dim {{ color: var(--dim); }}
.block {{ display: grid; gap: 16px; }}
.scenes {{ display: grid; gap: 14px; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }}
.scene {{ background: var(--panel); border: 1px solid var(--rule); border-radius: 6px; padding: 16px; display: grid; gap: 10px; min-width: 0; }}
.scene header {{ display: flex; justify-content: space-between; align-items: baseline; gap: 8px; flex-wrap: wrap; }}
.scene h3 {{ font-size: 1.3rem; }}
.meta {{ font: 12px var(--mono); color: var(--dim); font-variant-numeric: tabular-nums; }}
.roles {{ display: flex; gap: 6px; flex-wrap: wrap; font: 500 11px var(--mono); letter-spacing: .06em; }}
.roles span {{ border: 1px solid currentColor; border-radius: 99px; padding: 1px 8px; }}
.c1 {{ color: var(--c1); }} .c2 {{ color: var(--c2); }} .c3 {{ color: var(--c3); }}
.scene p:not(.meta):not(.roles) {{ color: var(--fg); }}
audio {{ width: 100%; height: 36px; }}
.eng {{ display: grid; gap: 8px; }}
.eng h3 {{ font-size: 1.15rem; display: flex; gap: 8px; align-items: center; }}
.eng h3::before {{ content: ""; width: 9px; height: 9px; border-radius: 50%; background: currentColor; }}
.eng .lede {{ color: var(--dim); }}
.eng ul {{ list-style: none; margin: 0; padding: 0; display: grid; }}
.eng li {{ display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 300px); gap: 14px; align-items: center;
  padding: 10px 0; border-top: 1px solid var(--rule); }}
.eng li h4 {{ font-size: .95rem; color: var(--fg); letter-spacing: .06em; }}
.eng li p {{ color: var(--dim); font-size: 14px; }}
.txt {{ min-width: 0; }}
@media (max-width: 560px) {{ .eng li {{ grid-template-columns: 1fr; gap: 6px; }} }}
footer {{ color: var(--dim); font-size: 13px; }}
</style>
<main>
  <section class="intro">
    <div class="rings"><i style="background:var(--c1)"></i><i style="background:var(--c2)"></i><i style="background:var(--c3)"></i></div>
    <h1>ALTER, écoute</h1>
    <p>Les scènes de départ et les sons d'ALTER, joués par le vrai firmware (la version navigateur), sans retouche.
    Sur l'instrument : <b>SAVE</b> ouvre les scènes et les patterns (SELECT choisit, SAVE charge, SAVE tenu enregistre), <b>PRESETS</b> fait défiler les sons du cercle choisi.</p>
    <p class="dim">Chaque son est joué seul sur 4 mesures (4 coups sur 16, 120 BPM), dans le mode d'enveloppe pour lequel il est fait. Les niveaux sont égalisés à la mesure.</p>
  </section>
  <section class="block"><h2>Scènes</h2><div class="scenes">{scenes_html}</div></section>
  <section class="block"><h2>Sons</h2>{''.join(eng_html)}</section>
  <footer>ALTER, fork de Felucca pour le M-VAVE FM-1 · GPL-3.0</footer>
</main>
"""
out = f"{D}/alter_ecoute.html"
open(out, "w", encoding="utf-8").write(page)
print(out, os.path.getsize(out), "bytes")
