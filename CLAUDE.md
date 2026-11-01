# CLAUDE.md — Firmware « ALTER » (fork de Felucca) pour M-VAVE FM-1

## Utilisateur
- Romain, non-développeur, techno live indus. Explications simples, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Garder ce fichier court et à jour.
- **N'a pas encore de FM-1** : cible = version navigateur (émulateur Felucca), qui doit rester compatible FM-1.

## Rôle de Claude
Ingénieur DSP + développeur C embarqué.

## Vision (30/10, revue le 31/10)
**Un instrument indus / noise / texture**, joué par un séquenceur euclidien orienté live. En live : variations
subtiles, jamais de rupture. En sound design : hasard radical possible. Influences : Basimilus Iteritas Alter, Plaits,
Collision, Euclidean Circles v2, Sting 2. Drums externes : Analog Rytm (sync MIDI clock).
- **3 cercles = 3 voix indépendantes** (1 voix chacune), chacune SON moteur : **ALTER, PHYS, NOISE** seulement (les autres
  moteurs Felucca retirés). Départ : cercle 1 ALTER, 2 PHYS, 3 NOISE. Presets indus/noise/texture créés par Claude
  (~6 par moteur, Romain juge). Changer de son : **SELECT = cercle, PRESETS = tous les presets des 3 moteurs**.
- Par cercle : LEN, HITS, ROT, PROB (cercle = déclenchements seulement). Changement de rythme au **cycle suivant**.
- **Force par pas** (ghost ↔ accent) et **chance par pas** : dans l'onglet **SEQ** (vue LINÉAIRE classique, un cercle à la
  fois, pages : FORCE avec rotation des accents/ghosts indépendante des coups ; CHANCE ; d'autres pages plus tard).
  Édition : tenir une/des touches blanches (= pas 1..16) + tourner un KNOB.
- **DÉ (par cercle) = un clic applique TOUT DE SUITE un nouveau tirage, DRASTIQUE** (retour 31/10) : rythme (HITS/ROT,
  rafales, flams, coups sautés), hauteur (jusqu'à 3 oct. dans la gamme, gamme jamais changée), glide, longueur et
  enveloppe PAR COUP, timbre, force. Tenir 0,6 s = motif propre. Verrous par paramètre (plus tard, vue DEEP).
- **Toutes les touches du clavier agissent sur la GRILLE (pas suivant), jamais en fin de boucle.** DENSITÉ = APPUI ±1
  coup, gardé. REVERSE relâché = retombe sur la grille.
- **Fills = sur le cercle SÉLECTIONNÉ** (ROLL, RATCHET, DUB DELAY/REVERB, DESTROY, DENSITÉ, REVERSE).
- **SHIFT −1/+1 = tourne le cercle sélectionné d'un pas par appui, gardé.** Écran : cercle choisi = anneau plein ; dé actif =
  petit triangle à gauche du cercle, à sa couleur.
  **Centre (01/11)** : triangle coupé en 3 depuis son centre, une pièce par cercle (pâle au repos ; un coup l'allume et
  la pousse vers l'extérieur ~0,25 s ; muet = gris ; point = cercle choisi) : alter_ui.c alt_prism/alt_tri/alt_glow.
- **Boutons de page = UNE règle (01/11)** : **appui court = page rapide** (4 KNOBS sur les cercles ; 2e appui ou HOME =
  fermée) · **tenu 0,5 s = MENU** du bouton (reste ouvert ; même bouton tapé, HOME ou autre bouton = sortie). Menus :
  SEQ (couloirs), FX (page FX), LFO (LFO 1 2 3 GLOBAL MTX), **ENV = enveloppe dessinée** selon le mode (knobs = page
  rapide ENV), **SCL = liste des gammes + clavier** (knobs = page rapide SCL), **EDIT = RÉGLAGES** (THEME NIGHT/DAY,
  KNOB 1 ou ALGORITHM ; SELECT = rangée), GLO pas de menu (tenu = court). Rapides : ENV = ATK DEC SUS REL (selon mode) ·
  FX = DIST, INSERT, DELAY, REVERB · LFO = LFO 1 · EDIT = 4 autres réglages du moteur · SCL = GAMME NOTE ACCORD DOSE ·
  GLO = TEMPO VOLUME PAN SWING. Code : alter_ui.c alt_menu_cur/alt_menu_set (seq_open, afp.open, mat_open, au.mn),
  au.mhold, ALT_MENU_HOLD_MS ; écrans alt_env_scene, alt_scl_scene, alt_set_scene, pied commun alt_foot.
  **THÈME** : ALT_DAY = ux.light ; DAY = palette Felucca PAPER, NIGHT = GREY (palette_choose + settings_save : gardé).
  DAY = **papier crème texturé** (retour Romain : « pas un blanc aussi blanc ») : ALT_BG (234,224,204) + grain fixe alt_paper.
  Couleurs ALTER = macros ALT_BG/INK/DIM/GREY/DARK/GO + ALT_COLS[2][3] ; fondus en couleur alt_mixc (jamais RGB(0,0,0)).
  **OCT− + OCT+ tenus + bouton = vraie page Felucca** (HOME pour sortir). **OCT− + OCT+ + DÉ n = page VERROUS du cercle n** (7 touches blanches = rythme,
  hauteur, glide, enveloppe, timbre, force, vélocité ; HOME pour sortir). Verrous PAR CERCLE. En-tête : pointe de
  couleur du cercle au lieu de son numéro.
- LED clavier : utilisables **faibles au repos, fortes si engagées** (mute, fill tenu, flash du dé) ; sans fonction = noires.
- **ALGORITHM = mode d'enveloppe du cercle choisi** (31/10) : FREE (ADSR du son) · PERC · STAB (coups ≤ 2 pas) · DRONE
  (attaque lente, note tenue jusqu'au coup suivant). L'ADSR du son l'affine d'1/4, le dé d'1/3. Affiché à côté du moteur.
- **CHAOS = DÉ 1+2+3 ensemble** : moteur, preset, 8 réglages, effets + LFO, rythme (immédiat), dé DOSE 127, gamme et
  notes de base, sur les 3 cercles. PAS le tempo, ni les modes, ni les mutes. DÉFINITIF (pas d'annulation).
- Vue **DEEP** (EDIT) : tous les réglages. Effets Felucca (DIST, INSERT FOLD, delay, reverb), ADSR Felucca.

## Spécification moteur ALTER
6 oscillateurs à table d'onde.
| # | Bouton | Rôle |
|---|---|---|
| E0 | MORPH (HOME) | sinus→triangle→scie→carré, fondu linéaire |
| E1 | SPREAD (HOME) | 0 = harmoniques 1..6 ; monte vers ratios inharmoniques/métalliques fixes |
| E2 | HARMONICS (HOME) | bas = graves seuls ; haut = aigus forts et plus longs |
| E3 | MODE (HOME) | SKIN (additif) / LIQUID (glide descendant au trigger) / METAL (FM 6→5→…→1) |
| E4 | AMOUNT (libellé selon MODE) | SKIN = DETUNE, LIQUID = PUNCH, METAL = FM DEPTH |
| E5 | NOISE | burst de bruit (LCG) à l'attaque, 0 = aucun |
| E6 | TILT | decay des aigus vs graves, dans l'ADSR |
| E7 | DRIFT | instabilité aléatoire lente des fréquences |

## Base technique (vérifiée dans le code)
- Fork de https://github.com/hugelton/Felucca (GPL-3.0 → code du fork ouvert aussi). `--recurse-submodules`.
  Sous-modules : `firmware/engines/` (Felucca-Engines, MIT), `firmware/bsp/` (matériel, on n'y touche pas).
- SLOOP (https://github.com/isod89/sloop-fm1) prouve qu'un fork peut refaire séquenceur et interface.
- Moteurs : `firmware/src/eng_*.c`, liste `firmware/src/engines.c`, `NENGINES` dans `core.h`.
- JieLi AC79 cœur **pi32v2** (pas ARM). **Virgule fixe uniquement** (int32, Q15/Q24), pas de float/malloc/math.h.
- 44 100 Hz, blocs `CTL` = 32. API `engine_t` (`felucca_engine.h`) : `note_on`, `render` (AJOUTE au buffer),
  hooks `desc`, `block`, `amp`, `post`. État voix `ph[3]` + `s[8]` ; plus → mémoire de piste `mem` (≤ ~12,9 Ko).
- Tables : SINE[1025], pitch_inc(), SVF_G, TANH_Q15. Modèles physiques existants : `eng_phys.c`, `phys_dsp.c`.
- CPU repère : ANALOG 8 voix = 17 %. Plaits est en float → à réécrire en entiers, modèle par modèle.
- Émulateur navigateur = le VRAI firmware en WebAssembly (écran 240×240, panneau simulé). Build FM-1 : `./build.sh` (Linux x86-64).
  Emscripten est bloqué dans la session cloud → on compile avec le clang du système : `tools/alter/wasm/build_emu.sh`
  (mini-libc maison `tools/alter/wasm/`, génère build/gen si absent, sortie build/emu/ + `alter.html` pour l'artifact).
  Tests : `node web/emu/emu_test.mjs build/emu/felucca.wasm` (Felucca ; 4 échecs VOULUS : « a key sounds », « FX held »,
  « SAVE, OCT+, OCT+ » et « a fresh instance… » : clavier, FX et SAVE appartiennent à la vue PLAY ; la ligne
  de résumé « 4 FAILED » en fait un 5e « FAIL » quand on les compte) et `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav`
  (chaque cercle seul, SEQ appui/tenu, boutons court/tenu ENV SCL EDIT GLO, THEME DAY, SHIFT, DÉ via son triangle à l'écran, DENSITÉ, fills sur le cercle choisi, PRESETS,
  page ENV, OCT−+OCT+ + FX → Felucca puis HOME, VERROUS, modes ALGORITHM, CHAOS, SAVE/LOAD scènes + patterns, usine
  protégée, mémoire flash relue par une instance neuve, traînes d'effets qui s'éteignent). La page SAVE garde sa
  dernière ligne : le test va d'abord en haut (row(n)).
  Le test règle le tempo à 120 (web_alter 4) ; un pas = nombre ENTIER d'échantillons (5512 à 120 BPM), comme le firmware.
  Détecteur d'attaques = enveloppe (les traînes delay/reverb des presets ne retombent jamais à 0).
  Captures d'écran : `node tools/alter/alter_shot.mjs build/emu/felucca.wasm DOSSIER "b:PLAY w:500 K:10+ s:nom"` (.ppm).
  Clavier de la page : `-` = PLAY, `0` = SEQ, `1`..`=` = FX…REC, ↑↓ = SELECT, Z.. = touches blanches.
  `tools/alter/wasm/alter_page.py` : texte d'aide de la page entre apostrophes Python → écrire ’ (pas ') dans le texte,
  sinon build_emu.sh s'arrête APRÈS avoir fait le .wasm (alter.html pas mis à jour).

## Code — dépôt https://github.com/Forban5929/alter-fm1 (branche `main`)
Source de vérité = le dépôt GitHub (copies de secours dans le Projet, `claude/code/`).
Démarrer une session : attacher `Forban5929/alter-fm1` (add_repo, push), cloner, `git submodule update --init`.
(Dépôt renommé `Alter-FM1` sur GitHub le 31/10 : les anciens liens redirigent.)
- `firmware/src/eng_alter.c` : le moteur, inscrit en DERNIER dans `engines.c` (`ENGI_ALTER` = NENGINES-1 dans core.h,
  1er dans ENGINE_ORDER, partie 1 au démarrage = ALTER THUD ; ENG_VCOST 330 ; MIDI CUT=HARM RES=SPRD ; pas de favoris).
- `firmware/src/alter_play.c` (inclus en tête de seq.c, `alter_tick` après click_tick, côté AUDIO) : cercle i joue la
  PARTIE i (0..2 ; TRK_DEF dans engines.c = ALTER THUD, PHYS TOMS, NOISE METAL ; partie 4 DRUM muette). Note d'un pas,
  GLIDE seulement si le dé l'a tiré, offsets de timbre du dé sur 4 paramètres PAR MOTEUR (ALT_TIMB_*) annulés au pas
  suivant (alt_applied[3][10] : E1..E8, DEC, REL ; alt_slot/alt_offset). Par coup : longueur (alt_gate_left, un coup
  coupe le précédent), DEC/REL décalés, rafales/flam (alt_rat_*[3]). FILLS tenus (AF_*) sur UN cercle = alt_tgt.
  Échange UI→audio sans conflit : file alt_q (cercle, P_*, delta : tous les KNOBS, UI écrit / audio lit), compteurs
  écrits par l'UI seule (alt_shift_tot, alt_dens_tot, alt_roll_cnt, alt_clear_cnt, alt_preset_cnt), alt_sel, alt_post +
  alt_post_cfg (+ alt_post_now = tout de suite) ; octets écrits par l'UI : alt_seq.scale, alt_seq.c[i].locks,
  alt_emode[3]. SWING = song.g[G_SWING] (alt_len). Modes : AM_*, ALT_MODE_ENV, slots alt_applied ATK/SUS en plus.
- `firmware/src/alter_ui.c` (inclus dans ui_input.c) : vue PLAY = HOME (alter_view), rend la main à Felucca dès qu'un
  autre bouton est pressé (0,4 s après relâchement). KNOB 1-4 = engine_t.knob du moteur du cercle choisi ; SELECT =
  cercle + track_select (pages Felucca ENV/FX/EDIT suivent) ; PRESETS = presets ALTER+PHYS+NOISE à la suite
  (alt_preset_step, change de moteur si besoin) ; table ALT_KEY (touche k = demi-tons au-dessus de F3). Écran :
  alter_draw (2 moitiés 240×120, redessin si signature change). Pages rapides : table ALT_QP_K (QK_* = réglages
  spéciaux : gamme, note, dose, tempo, swing, rythme, moteur), alt_page/alt_turn/alt_kv ; boutons de page = ceux de
  la vue (alt_ours), OCT−+OCT+ + bouton → au.handed (Felucca). Page VERROUS : au.lockc, ALT_LK_KEY/BIT. CHAOS : alt_chaos.
  Comparer des temps en SIGNÉ (`(int32_t)(fm1_ms - t0)`) : bug corrigé le 31/10 (die_t0 = fm1_ms|1).
- **Felucca ne séquence plus rien** (bug « kick invisible », 31/10) : seq.c n'appelle plus seq_tick (ALTER_FELUCCA_STEPS 0)
  — un motif Felucca (sa vraie page SAVE, une vieille sauvegarde auto du navigateur) jouait sous les cercles sans être
  vu. REC et ARP appartiennent à la vue et ne font rien ; song.rec est désarmé (sinon décompte + clic de métronome).
  Test : web_alter 6 (phrase Felucca sur une partie) + REC + NEW SCENE = silence.
- `tools/alter/alter_wav.c` : programme PC, rend 15 extraits WAV + compare le coût à ANALOG.
  Recréer : `python3 firmware/engines/tools/gen_engine_tables.py build/alter/fe_tables.h`
  puis `cc -std=c99 -O2 -I build/alter -o build/alter/alter_wav tools/alter/alter_wav.c -lm && build/alter/alter_wav build/alter/wav`
  (son ADSR est une approximation, pas celui de `voice.c`).
- Choix techniques ALTER : état des 2 voix dans `ENG_MEM` (slot par voix), ratios SPRD harmonique → cymbale 808 →
  barre libre, oscillateurs tous en phase au coup, normalisation √(nb d'oscillateurs) + `soft_knee(20000)`,
  64 bits seulement 1×/bloc (incréments). Presets : THUD, BELL, CLANG, SNAP, STAB, RUST.
- Mesures (PC x86) : ALTER ≈ 1,7 à 2,3 × ANALOG par voix → estimé ~4-5 % CPU FM-1 par voix (≈10 % pour 2).
  NON vérifié sur pi32v2 (toolchain JieLi pas encore installée). Crêtes -2 à -5 dBFS, pas d'écrêtage, DC ≈ 0.
- `firmware/src/alter_seq.c` : séquenceur C pur, 1×/double-croche. 3 cercles indépendants (`as_cfg_t` : len hits
  rot prob frot dose mute root). Par pas : `force[]` (GHOST/NORMAL/ACCENT = vél. 40/96/124, rotation FROT propre) et
  `chance[]` (0..127). `as_set` = cycle suivant (mute, root tout de suite) ; `as_turn` = SHIFT immédiat gardé ;
  `as_roll` = dé IMMÉDIAT (dhits/drot, rat, flam, dchance, gate, env, force, hauteur, vél., glide, 4 timbres ;
  as_repat = motif joué) ; `as_clear` = motif propre ; `as_density` = ±1 HITS gardé ; `as_step(q, ev[3])`.
  DOSE par défaut AS_DOSE_DEF = 100 (drastique). Gamme partagée (OCT 5TH MPEN MIN PHRY DOR CHR), verrous PAR CERCLE
  (c->locks) AS_LK_PITCH/VEL/GLIDE/FORCE/TIMB/RHYTHM/ENV. Accord : cfg.chord (AS_CH_*), ev.chord[2].
  **ACCORDS (01/11, choix Romain)** : knob = OFF OCT 5TH TRIAD 7TH 9TH (AS_CH_ORDER, as_ch_step) ; 3 voix max :
  OCT +12, 5TH +7 +12 (fixe), TRIAD 3ce+5te, 7TH 3ce+7e (sans quinte), 9TH SANS FONDAMENTALE 3ce 7e 9e (ev.note = 3ce).
  TRIAD/7TH/9TH suivent la gamme (as_cnote : note de gamme la plus proche de l'intervalle, marche en pentatonique).
  Valeurs stockées : 2 (ex-SUS) = 5TH, 3 (ex-OPEN) = OCT → AMBIENT (USER 01, ex-OPEN) joue maintenant OCT. HOUSE WARM = 7TH.
- `tools/alter/alter_seq_test.c` : 35 contrôles (accords, cycle suivant, mute, force+FROT, chance, dé drastique, verrous,
  SHIFT, DENSITÉ, polyrythmie). `cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
- (`alter_seq_demo.c` supprimé : ancien modèle. Nouvelles démos à faire en 3d-3.)
- `tools/alter/make_listen_page.py build/alter` : page d'écoute HTML (MP3 via ffmpeg depuis wav/ dans mp3/).
- Page d'écoute (scènes + 19 sons, 3d-3) : https://claude.ai/artifact/BqNkjf2dYiPQFnkgco6zey

## État d'avancement
- [x] Dépôts vérifiés, vision et design validés
- [x] Étape 1 : moteur ALTER en C + extraits audio (validé : « bon début »)
- [x] Étape 2 : séquenceur 3 cercles + dé (validé par Romain, fusionné dans main)
- [ ] Étape 3 (choix validés 30/10 : vues SUR L'ÉCRAN FM-1 240×240, ALTER REMPLACE Felucca (comme SLOOP),
      cercles CONCENTRIQUES HITS dehors → ACCENT dedans, livraison par morceaux) :
  - [x] 3a : ALTER + séquenceur jouables dans le navigateur. Page (republiée à chaque morceau) :
        https://claude.ai/artifact/JpKofrvF7kb17ZmrJZmx8X — republier `build/emu/alter.html` + files felucca.wasm,
        worklet.js, fonts/DotGothic16-subset.woff, fonts/OFL.txt (lire felucca.wasm publié avant, sinon refus).
  - [x] 3b : vue PLAY (cercles concentriques). **Règle de Romain : une touche fait ce que dit son nom.**
        Boutons : HOME = PLAY ; EDIT = DEEP ; SEQ tenu = KNOB 1-4 → LEN HITS ROT PROB ; SELECT = cercle ; SCL = gamme +
        note de base ; ENV, LFO, FX, GLO (tempo/sync), SAVE ; PLAY = start/stop ; ARP, REC = rien pour l'instant.
        Clavier : NOIRES tenues (groupes 3-2-3-2-1) ROLL 1/8T 1/16 BUILD UP | RATCHET x2 x3 | DUB DELAY, DUB REVERB,
        DESTROY | DENSITÉ -, + (appui) | REVERSE. BLANCHES (blocs allumés séparés d'une éteinte) MUTE 1 2 3 · DÉ 1 2 3 ·
        SHIFT -1 +1 · le reste libre.
  - [x] **3d-1 (branche `etape-3d`, 31/10) : 3 voix ALTER/PHYS/NOISE** codé, testé, publié (même lien que 3a).
        Choix validés : 3 MUTE + 3 DÉ (un par cercle) ; KNOB 1-4 = son du cercle choisi ; note de base par cercle
        (C2, C3, C4). 1er retour de Romain (31/10) appliqué le soir même (dé drastique, grille, DENSITÉ appui, SEQ
        verrouillable, anneau du cercle choisi, triangle du dé), puis pages rapides + OCT−+OCT+ + VERROUS par cercle,
        puis modes PERC/STAB/DRONE (ALGORITHM) + CHAOS.
        ATTEND SA 2e ÉCOUTE.
        Constat : NOISE METAL nettement plus doux que ALTER/PHYS dans le mélange → équilibrer en 3d-3.
  - [x] **3d-2 → remplacé par 3g (menu SEQ, ci-dessous)**.
  - [x] **3d-3 (31/10, codé, testé, publié ; ATTEND L'ÉCOUTE)** : SCÈNES + presets. Choix validés : **SAVE (appui court) = page SCÈNES** (PRESETS
        choisit, SAVE charge, HOME annule ; les vraies pages SAVE de Felucca via OCT−+OCT+ + SAVE). Scènes de départ :
        TECHNO (texture, stab, perc), HOUSE (perc, accord, drone), AMBIENT (drone, drone, texture) ; une scène règle les 3
        cercles (moteur, preset, mode, rythme, note, accord) + gamme + TEMPO. **ACCORD par cercle** (3 notes de la gamme
        par coup, réglable dans SCL ; le dé bouge l'accord entier). **Presets indus/noise/texture ~6 par moteur** dans
        une table À NOUS (alter, pas les tables Felucca : NOISE est dans le sous-module), niveaux équilibrés. Page
        d'écoute : chaque preset + chaque scène, rendus par le VRAI firmware (émulateur).
        FAIT : `firmware/src/alter_sounds.c` = ALT_PRE (19 sons : ALTER THUD ANVIL STAB PIPES RUST GRIND, PHYS TOMS GONG
        BARS SCRAPE BUZZ SITAR, NOISE HAT SNARE METAL CRUSH RAIN WIND ARCADE ; moteur, E1..E8, ADSR, envois, LEVEL, mode
        conseillé) + ALT_SCENE (TECHNO HOUSE AMBIENT INDUS — INDUS ajoutée par Claude ; gain par cercle). Scène 1 chargée
        à l'allumage (TRK_DEF n'est plus qu'un point de départ). Une scène chargée retombe sur la grille depuis PLAY
        (alt_gstep). SCL KNOB 3 = ACCORD (OFF TRIAD SUS OPEN) à la place du GLIDE (glide encore réglable par la vraie page).
        ALTER passé à 3 voix (accords). Niveaux mesurés : sons perc ~0,10-0,15 de crête, sons longs −27..−31 dB RMS,
        scènes −30..−34 dB RMS. `tools/alter/alter_render.mjs build/emu/felucca.wasm build/alter` (WAV + volumes) puis
        `python3 tools/alter/make_listen_page.py build/alter` → build/alter/alter_ecoute.html (page d'écoute, même lien
        que l'ancienne : https://claude.ai/artifact/BqNkjf2dYiPQFnkgco6zey). Hook navigateur : web_alter(op,a,b)
        (felucca_web.c → alter_ui.c alt_web_cmd : 0 son, 1 scène, 2 mute, 3 mode, 4 tempo, 5 mode conseillé).
  - [x] **3d-4 (31/10, codé, testé, publié ; ATTEND L'ÉCOUTE) : SAVE / LOAD** (retour de Romain). Page SAVE à 2 ONGLETS SCÈNES / PATTERNS.
        Gestes VALIDÉS : SELECT = slot · ALGORITHM = onglet · appui SAVE = CHARGER · SAVE tenu 1 s = ENREGISTRER · HOME =
        sortir. Chaque onglet : NEW (vide, chargement seul, jamais écrasable) + 4 d'usine PROTÉGÉS + 16 slots USER.
        SCÈNE = tout (sons, réglages, effets, rythmes, modes, notes, accords, verrous, gamme, tempo, swing).
        PATTERN = rythme SEUL (par cercle LEN HITS ROT PROB FROT + force et chance par pas), le reste ne bouge pas.
        Mémoire durable : ALTER prend les slots PROJETS 3 et 4 de Felucca dans la flash (validé). HOUSE refait :
        4 temps au kick + stabs en triade façon house avec DUB DELAY.
        FAIT : `firmware/src/alter_store.c` (inclus par alter_ui.c) : listes NEW + 4 usine + 16 USER par onglet
        (ALT_NLIST), patterns d'usine ALT_FPAT (FOUR FLOOR, ACID, BROKEN, POLY), alt_scene_get/put (scène compacte :
        force 2 bits, chance 4 bits par pas), alt_rhy_get/put (pattern), alt_new_scene, alt_row_load/save, banques en
        RAM alt_bank_s/alt_bank_p + flash objets 3 et 4 (magie ATLS/ATLP), chargées au démarrage (alt_banks_load).
        Scènes d'usine : champ `dly` (envoi delay, 0 = celui du son). Écran : 2 onglets, 5 lignes, cadenas = usine.
  - [x] **3e LFO (31/10, 3e version après retours de Romain ; codé, testé, publié ; ATTEND L'ÉCOUTE)**. Validé :
        **3 LFO simples PAR CERCLE + 1 GLOBAL**, chacun K1 WAVE · K2 RATE · K3 DEPTH · K4 DEST (1 seule) ; formes SIN TRI
        SAW SQR S&H SLEW RAMP+ RAMP- ; RATE 1/32..8 BAR (GL_RATE_6TH, sixièmes de double-croche) ; DEST = OFF PITCH SHAPE
        FILTER LEVEL DIST DELAY REVERB PAN PROB (même liste partout). **Appui court LFO** = LFO 1 du cercle choisi (page
        rapide, « LFO 1 »). **LFO encore** = page LFO : onglets **LFO 1 · LFO 2 · LFO 3** (du cercle choisi, soulignés à sa
        couleur ; SELECT change de cercle) · **GLOBAL** · **| MTX** (cases Felucca du cercle choisi, SELECT = case, K1 SRC
        K2 DEST K3 AMT, phrase d'aide par source). Vue d'un LFO : **grille d'UNE mesure fixe** (pas fins, temps plus clairs)
        et la **vague qui défile** (présent au bord droit) ; RATE change le nombre de vagues visibles ; liste déroulante
        pour DEST. Tout est à nous (alter_lfo.c) : une horloge alt_gl_n (échantillons depuis PLAY, en pas entiers
        alt_lfo_p) → alt_lfo_at(forme, rate, graine, n) ; S&H/SLEW = hasard tiré du numéro de cycle (l'écran dessine
        exactement ce qui joue). alt_cl[3][3] + alt_cl_val, alt_gl (slot 0 seulement) + alt_gl_val. Le GLOBAL **prend le
        dessus** sur une DEST (alt_cl_on, P_LD_*, cases MTX à source LFO/S&H/SLEW). Dans MTX, LFO/S&H/SLEW = LFO 1 du
        cercle (alt_mlfo). Le LFO Felucca des parties n'est plus utilisé (P_LD_* à 0). mod.c : alt_lfo_k (k 0 GLOBAL,
        1..3 LFO du cercle) dans mod_global et mod_show. PROB : alt_seq.pmod[3] = alt_prob_of. Changer de son garde les
        LFO. Scènes : 3 octets par LFO (WAVE|RATE<<3, DEST, DEPTH) + GLOBAL 4 octets ; P_LRATE/LD_* sortis d'ALT_SP
        (banque 3780 o) → **slots USER d'avant effacés**. CHAOS tire le LFO 1 de chaque cercle. voice.c : FADE 0 = sans
        fondu. Écran cercles : une valeur bougée par un LFO = **zone fixe dans la couleur du cercle choisi** (plage min↔max, mod_show ext ±1) +
        barre blanche fixe — plus de trait qui saute ni de pulsation (alt_live / alt_live_bar). Page LFO : la
        vague dessinée **suit DEPTH** (taille, retournée si négatif) sur sa forme pleine en pointillé gris. Tests : LFO 1 SQR→PROB, SAW→LEVEL, le GLOBAL
        prend le dessus, RAMP+→LEVEL, LFO 2 SQR -64→PROB (autre moitié), MTX POS→AMP. Idée pas faite : DÉ qui tire les LFO.
  - [x] **3f ENVELOPPES PAR MODE (31/10, codé, testé, publié ; ATTEND L'ÉCOUTE)**. Choix de Romain : page ENV = 4 knobs
        selon le mode du cercle (ALGORITHM) : FREE = ATK DEC SUS REL (ADSR du son) · **PERC = ATTACK (courte) HOLD DECAY
        PUNCH** (AD + hold façon Rytm : SUS 127, alt_hold_smp lâche la note après HOLD 0..500 ms, DECAY = release ;
        PUNCH = la hauteur plonge, jusqu'à 2 oct., ~23 ms) · **STAB = LENGTH (decay = release, plafonné) SUSTAIN PLUCK
        (l'enveloppe ouvre le FILTER, terme par voix MS_ENV→MD_CUT) CHORD** (gate ≤ 2 pas) · **DRONE = SWELL (jamais
        court) BODY (sustain, decay fixe dedans) TAIL (min ~290 ms) OPEN** (l'enveloppe ouvre le FILTER, comme PLUCK ;
        BREATH retiré : doublon des LFO). **DRONE en couches** (retour Romain : chaque trigger rejoue) : chaque coup =
        nouvelle couche (alt_on_drone, alt_no_reuse → voice_alloc ne réutilise pas la voix de la même note), **2 couches
        max**, la 3e fait relâcher la plus ancienne (son TAIL) ; avec ACCORD (3 voix ALTER/PHYS) = fondu enchaîné
        (l'ancien relâché, ses voices reprises au besoin) ; eng_alter : une voix qui sonne encore reprise garde phases,
        pas de sweep ni de burst (sinon clic) ; alt_off relâche aussi les couches ; pas de rafales/flams du dé ; **PHYS MODAL/STRNG
        passe à l'archet** en DRONE (BOW = E7 → 80 si < 60, retiré hors DRONE ; MEMB/SYMP : E7 = BEND/BUZZ, pas
        d'archet). STAB LENGTH : 1er quart retiré (alt_map 48..86). Réglages 0..127 → temps Felucca (ms = 10000^(v/127)) via alt_map dans alt_env_apply (alter_play.c), à
        chaque coup. Données : alter_lfo.c AE_* / alt_envp[3][11] / AE_DEF, alt_ems (miroir des modes), alt_punch_lvl,
        alt_breath_val ; mod.c mod_global les applique (alt_env_mod). **Nouvelles DEST LFO : LENGTH ATTACK DECAY SUS/REL**
        (GD_ELEN.. ; lues à chaque coup, alt_env_lfo ; sens selon le mode, alt_env_gd). **Petite vague de couleur à côté
        du nom** d'un knob qu'un LFO bouge sans barre visible (PROB, réglages d'enveloppe : alt_knob_lfo) ; la barre ne
        pulse plus (zone de plage seule). Scènes USER : réglages d'enveloppe gardés dans l'objet 4 (alt_bank_p.envok/env :
        la banque de scènes est pleine) → **patterns USER d'avant effacés** ; scènes d'usine / NEW = AE_DEF.
        Tests : PERC HOLD, STAB LENGTH, LFO SQR → LENGTH en PERC, DRONE en couches sans attaque dure (sauts < ×1,5 par 50 ms).
  - [x] **3g MENU SEQ (31/10, 2e version après retour de Romain ; codé, testé, publié ; ATTEND L'ÉCOUTE)**.
        **SEQ tenu 0,5 s = menu SEQ** (reste ouvert ; SEQ tapé ou HOME = sortie ; appui court = page rapide des 4 réglages
        du rythme, nommée EUCLID / TURING sans « SEQ ») ; **SEQ tenu + ALGORITHM = moteur du cercle : EUCLID · TURING**
        (STEP retiré : « contre-productif » ; la vue linéaire sert seulement au détail par pas). Knobs : EUCLID LEN HITS ROT
        PROB · TURING LEN(1..16) LOCK (127 = figé) DENS RANGE. **Couloirs** sur 16 pas : ALGORITHM = VELO · CHANCE · DELAY ·
        REVERB · PITCH ; tenir des pas (touches blanches) + un KNOB = leur valeur (affichée en bas) ; pas de texte d'aide.
        **TURING en jeu** (cercle choisi en TURING) : SHIFT −1 = motif PRÉCÉDENT, SHIFT +1 = motif SUIVANT (nouveau, ou
        celui annulé par PREV), touche blanche 18 = **LOCK** (figé / rendu au knob LOCK ; LED allumée = figé, noire en
        EUCLID) ; 2 motifs gardés de chaque côté. ROLL etc. inchangés.
        Code : alter_seq.c AS_T_EUCLID/TURING, as_tnext/as_tprev (tback/tfwd, AS_THIST 2), tfreeze (écrit par l'UI),
        svel/sdly/srev/spitch, ev.dly/ev.rev ; alter_play.c : SHIFT (alt_shift_tot) = NEXT/PREV si TURING ; offsets P_DLY
        (+dly) P_REV (+rev) par coup ; alter_ui.c : AK_TLOCK, LN_*, alt_seq_input/scene/sig. Stockage alt_rhy_t (couloirs
        4 bits, objet 2 pour les scènes USER) → **slots USER d'avant effacés** (taille changée). P-lock : pas fait.
        **01/11** : couloir **RATCH** (×1..×4 par pas : alter_seq.c srat → ev.rat, le dé peut aller plus loin ; onglets
        courts ALT_LANE_TAB VELO CHNC DLY REV PITCH RATCH). Stocké À PART (2 bits/pas, alt_rat_get/put) : alt_bank_r.rat
        (scènes USER) et alt_bank_p.prat (patterns USER) ; anciennes tailles encore lues (ALT_BANK_R_OLD/_FX, ALT_BANK_P_OLD)
        → **slots USER gardés** ; le dump AMBIENT ne contient pas de RATCH (zéro). **Touches noires dans le menu SEQ** =
        raccourcis (ALT_SEQ_BK : 1 VELO, 3 CHNC, 5 DLY, 8 REV, 10 PITCH, 13 RATCH, 15 EUCLID/TURING, 20 cercle −, 22 cercle +).
        **LED** du menu SEQ : pas joué + pas tenus allumés (la lumière avance), coups à mi-niveau (nm), autres faibles ;
        couloir affiché allumé sur sa touche noire. **ROLLS** (choix Romain) : 1/8T · 1/16 · BUILD UP (4 croches, 8 doubles,
        puis triples croches tant qu'on tient, de plus en plus fort ; AF_ROLLT8/AF_ROLL16/AF_BUILD). CHAOS tire aussi RATCH.
        **PLAGES (01/11, choix Romain)** : pas tenus + **KNOB 1 = valeur** (efface la plage), **KNOB 2 = SHUFFLE** (ci-dessous), **KNOB 3 /
        KNOB 4 = bas / haut d'une PLAGE** : chaque passage tire une valeur au hasard dedans (alter_seq.c rmask[AS_NLR]/
        rlo/rhi, as_rng ; VELO avec FROT). Écran : bande pâle entre les deux points + « 41%..82% RANDOM ». CHAOS et un
        chargement effacent les plages. **Plages PAS ENCORE sauvegardées** dans les scènes/patterns (pas de place dans les
        objets 2-4 : 324 o/scène). Test : CHANCE 0..100 % sur le pas 1 → joué 6/10 mesures, KNOB 1 le rend.
        **SHUFFLE (01/11, remplace les conditions, retirées : « pas dans le workflow »)** : **DEUX pas tenus au moins +
        KNOB 2** (anti-erreur en live ; 1 pas tenu + KNOB 2 = rien, aucun pas = HITS) = chaque cran tire au hasard TOUT
        l'onglet affiché sur ses 16 pas (joués ou non), valeurs fixes (plage effacée), toute la hauteur sauf VELO/CHANCE
        ≥ 30 % (alter_ui.c alt_lane_shuffle, alt_cr). Test : 1 pas = écran inchangé, 2 pas = couloir changé.
        **FLAT (01/11)** : menu SEQ, **OCT− + OCT+ tenus + touche noire d'un onglet** = cet onglet remis à plat (VELO 96,
        CHANCE 127, DLY/REV/PITCH/RATCH 0, plage effacée ; alt_lane_flat). Test : après SHUFFLE, FLAT = écran d'avant.
        (Le test LOCKS tolère 1 attaque égarée : il bougeait avec le minutage du test, pas un bug du verrou.)
  - [x] **3h PAGE FX + MASTER (31/10, codé, testé, publié ; ATTEND L'ÉCOUTE)**. Choix de Romain : **FX tenu 0,5 s = page FX**
        (reste ouverte ; FX tapé ou HOME = sortie ; FX tapé court = page rapide d'avant) ; **delay et reverb PARTAGÉS** (1 seul
        de chaque dans Felucca) + envoi par cercle ; master **simple + presets** ; ducking = **temps OU cercle**. Le clavier
        joue pendant la page (mutes, dés, fills). Onglets (ALGORITHM) : **DIST** (cercle choisi : DRIVE TYPE AMOUNT MIX ;
        SOUND WARM CRUNCH FOLD FUZZ SCREAM BITS DESTROY, niveaux mesurés ±3 dB) · **DELAY** (TIME FEEDBK COLOR SEND ; ECHO SLAP
        DUB DUB LONG TAPE TRIPLET 16TH DOT SPACE) · **REVERB** (TYPE SIZE DAMP SEND ; HALL ROOM BIG ROOM METAL SPRING DARK
        SPRING BRIGHT HALL CAVE) · **MASTER** (SELECT = rangée : LOW MID HIGH COMP | DUCK SRC SHAPE CLIP ; FLAT LIVE GLUE LOUD
        PUMP 1/4 PUMP 1/8 DUCK C1 DARK ; vu-mètres COMP et DUCK). PRESETS = preset de l'onglet ; un knob tourné = nom avec *.
        Code : `alter_fx.c` (inclus dans fx.c : tables de presets, amx[8] écrit par l'UI, am_master = EQ isolateur
        Linkwitz-Riley 24 dB/oct 250 Hz / 2,5 kHz (64 = plat, 0 = coupé, 127 = +6 dB) → COMP 4:1 (gain toutes les 8
        éch., attaque immédiate, ~190 ms) → CLIP (tanh à mi-échelle, sous le limiteur) ; à FLAT rien ne tourne = son
        identique à avant ; duck = am_dg[3] sur LEVEL dans mix_part, SRC 1/4 1/8 1/2 (grille, baisse les 3 cercles, suit
        le kick du Rytm via MIDI clock) ou CIRC n (baisse les 2 autres), am_hit/am_clock appelés par alter_play.c).
        `alter_fxpage.c` (inclus dans alter_ui.c : afp_input/afp_scene/afp_sig, état afp et afx_sel/afx_edit dans alter_fx.c).
        Scènes USER : effets (delay, reverb, master, presets, INSERT B/C) dans l'objet 2 (alt_bank_r.fx, ancienne taille
        encore lue : **slots USER gardés**). Changer de son remet sa DIST à SOUND.
        **Retour Romain (31/10 soir)** : « effets trop présents, grooves perdus » → scènes d'usine = effets d'ORIGINE
        (dpre/rpre/mpre tous 0 = ECHO/HALL/FLAT ; Romain ajoute les effets lui-même). **CHAOS** : envois DLY/REV ≤ 33 %
        (0..42), DIST/INSERT A/MIX ≤ 10 % (0..13), type d'insert OFF..CRUSH, LFO 1 jamais vers DIST ni les envois.
        CHAOS tire aussi les **couloirs SEQ** (31/10) : chaque couloir (VELO CHANCE DELAY REVERB PITCH) a 40 % de chances
        d'être tiré, indépendamment, sur ~1 pas sur 2 (VELO 40..127, CHANCE 40..127, DELAY/REVERB 0..51 = 40 % max, PITCH
        −7..7) ; un couloir non tiré revient neutre (96, 127, 0, 0, 0).
        TECHNO = « parfait » (ne pas toucher) ; INDUS redevenue celle d'avant. **HOUSE refaite en DEEP** (122 BPM DOR swing 8 :
        THUD 4/16, WARM triade 3/16 rot 3, OPEN HAT 4/16 rot 2) ; **AMBIENT refaite en nappes douces** (70 BPM MIN : 2 × PAD
        en DRONE, 32 pas accord OPEN + 24 pas rot 12 une quinte plus haut, RAIN discrète). 3 sons ajoutés EN FIN de ALT_PRE
        (WARM, PAD, OPEN HAT : les numéros des sons des scènes USER ne bougent pas). alter_render.mjs accepte les noms à espace.
        **2e retour (31/10, 23 h)** : son **KICK** ajouté (fin de ALT_PRE : sinus, LIQUID 64, NOIS 10, DEC 85, DIST 25 ;
        +5 dB de graves < 120 Hz vs THUD) pour TECHNO et HOUSE (gain −6, PERC HOLD 45 DECAY 85 : champs de scène
        phold/pdec). TECHNO : le 4e kick (pas 13) part à +40 % dans le delay (champs lstep/ldly → couloir DELAY sdly).
        Tests : web_alter 9 (couloir DELAY d'un pas à 0) et 10 (réglages de mode par défaut) remettent l'ancien kick sec.
        **AMBIENT = scène USER 01 de Romain** (31/10) : bouton « Envoyer mes sauvegardes ALTER à Claude » de la page 3a
        (capability db, collection `saves`, docs {off, b64} = secteurs flash ATLS/ATLR/ATLP) → ArtifactData list out_dir →
        `node tools/alter/alter_user_scene.mjs build/emu/felucca.wasm DOSSIER 1 AMBIENT > firmware/src/alter_user_ambient.h`
        (web_alter_dump / alter_ui.c alt_dump_user : scène + 3 rythmes + fxok + fx[32] + envok + env). ALT_SCENE.user = ces
        octets → alt_load_scene charge la scène entière. _Static_assert sur la taille : si les structures changent,
        RE-EXTRAIRE (les secteurs ne sont pas gardés : redemander l'envoi). Vérifié : même son que USER 01 chargé.
        Test : `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm` (page, EQ par bande, COMP, CLIP, DUCK, scène).
        ⚠ Le son du navigateur a ~0,3 s de retard sur les réglages : attendre ≥ 1 s avant de mesurer.
        Police AF_L : « Z » ajouté (tools/gen_aa_font.py ; supprimer build/gen pour régénérer).
        Pas fait : delay ping-pong stéréo (le delay Felucca est mono), EQ/compresseur PAR piste (étape 4).
  - [ ] 3c : vue DEEP (EDIT ouvre encore la page EDIT de Felucca). Puis retirer l'interface Felucca. Reste : SCL → gamme
        des cercles, touches blanches libres, DESTROY assignable, verrous du dé, DOSE.
- [ ] Étape 4 (après le séquenceur) : tranche de console PAR PISTE, ordre FIXE validé :
      son → **disto/fold → EQ 3 bandes → compresseur** → delay (echo, ping-pong, dub) → reverb. (Master fait en 3h.)
      Existant Felucca (vérifié dans fx.c) : DIST, INSERT (SOFT/HARD/FOLD/FUZZ/CRUSH…), delay MONO à 1 ligne avec
      retour filtré (base possible pour le dub), reverb HALL/ROOM/SPRING, limiteur master.
      À créer : EQ 3 bandes et compresseur par piste (réutiliser alter_fx.c), delay ping-pong (stéréo). Coût CPU à mesurer.
## GitHub (créé le 30/10/2026)
- Compte : **Forban5929**. Dépôt : **fork PUBLIC de hugelton/Felucca nommé `alter-fm1`**.
- Branches : `main` = version ALTER qui marche ; une branche par étape (`etape-2-sequenceur`…) fusionnée
  dans main quand Romain a validé à l'écoute. Remote `upstream` = hugelton/Felucca, `origin` = le fork (récupérer ses correctifs).
- Nos fichiers préfixés `alter_` dans `firmware/src/` (eng_alter.c, alter_seq.c, alter_ui.c, alter_fx.c),
  outil PC dans `tools/alter/`, docs dans `docs/alter/`, `CLAUDE.md` à la racine. Sous-modules inchangés.
- Version navigateur : artifact claude.ai pour l'instant ; GitHub Pages plus tard (Romain devra activer Pages).
- Issues GitHub = retours d'écoute de Romain ; un jalon (milestone) par étape.
- La session cloud ne peut ni lister le compte ni créer de dépôt (proxy) : add_repo puis git push uniquement.
