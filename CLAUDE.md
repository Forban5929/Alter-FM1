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
  coup, gardé. REVERSE relâché = retombe sur la grille. SEQ : appui court = reste ouvert, tenu ≥ 2 s = se ferme au relâchement.
- **Fills = sur le cercle SÉLECTIONNÉ** (ROLL, RATCHET, DUB DELAY/REVERB, DESTROY, DENSITÉ, REVERSE).
- **SHIFT −1/+1 = tourne le cercle sélectionné d'un pas par appui, gardé.** Écran : cercle choisi = anneau plein ; dé actif =
  petit triangle à gauche du cercle, à sa couleur.
- **Boutons de page (31/10)** : APPUI COURT sur ENV/FX/LFO/EDIT/SCL/GLO/SEQ = reste sur les cercles, seuls les 4 KNOBS
  changent (comme SEQ : 2e appui ou autre bouton = retour, tenu ≥ 2 s = se ferme au relâchement). ENV = ATK DEC SUS REL ·
  FX = DIST, INSERT (fold), DELAY, REVERB · LFO = VITESSE, →HAUTEUR, →TIMBRE, →VOLUME · EDIT = les 4 autres réglages du
  moteur · SCL = GAMME, NOTE DE BASE, GLIDE, DOSE · GLO = TEMPO, VOLUME, PAN, SWING. **OCT− + OCT+ tenus + bouton = vraie
  page Felucca** (HOME pour sortir). **OCT− + OCT+ + DÉ n = page VERROUS du cercle n** (7 touches blanches = rythme,
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
  (chaque cercle seul, SEQ appui/tenu, SHIFT, DÉ via son triangle à l'écran, DENSITÉ, fills sur le cercle choisi, PRESETS,
  page ENV, OCT−+OCT+ + FX → Felucca puis HOME, VERROUS, modes ALGORITHM, CHAOS, SAVE/LOAD scènes + patterns, usine
  protégée, mémoire flash relue par une instance neuve, traînes d'effets qui s'éteignent). La page SAVE garde sa
  dernière ligne : le test va d'abord en haut (row(n)).
  Le test règle le tempo à 120 (web_alter 4) ; un pas = nombre ENTIER d'échantillons (5512 à 120 BPM), comme le firmware.
  Détecteur d'attaques = enveloppe (les traînes delay/reverb des presets ne retombent jamais à 0).
  Captures d'écran : `node tools/alter/alter_shot.mjs build/emu/felucca.wasm DOSSIER "b:PLAY w:500 K:10+ s:nom"` (.ppm).
  Clavier de la page : `-` = PLAY, `0` = SEQ, `1`..`=` = FX…REC, ↑↓ = SELECT, Z.. = touches blanches.

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
- `tools/alter/alter_seq_test.c` : 29 contrôles (motifs, cycle suivant, mute, force+FROT, chance, dé drastique, verrous,
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
        Clavier : NOIRES tenues (groupes 3-2-3-2-1) ROLL 1/16 1/32 triolet | RATCHET x2 x3 | DUB DELAY, DUB REVERB,
        DESTROY | DENSITÉ -, + (appui) | REVERSE. BLANCHES (blocs allumés séparés d'une éteinte) MUTE 1 2 3 · DÉ 1 2 3 ·
        SHIFT -1 +1 · le reste libre.
  - [x] **3d-1 (branche `etape-3d`, 31/10) : 3 voix ALTER/PHYS/NOISE** codé, testé, publié (même lien que 3a).
        Choix validés : 3 MUTE + 3 DÉ (un par cercle) ; KNOB 1-4 = son du cercle choisi ; note de base par cercle
        (C2, C3, C4). 1er retour de Romain (31/10) appliqué le soir même (dé drastique, grille, DENSITÉ appui, SEQ
        verrouillable, anneau du cercle choisi, triangle du dé), puis pages rapides + OCT−+OCT+ + VERROUS par cercle,
        puis modes PERC/STAB/DRONE (ALGORITHM) + CHAOS.
        ATTEND SA 2e ÉCOUTE.
        Constat : NOISE METAL nettement plus doux que ALTER/PHYS dans le mélange → équilibrer en 3d-3.
  - [ ] **3d-2 (PROCHAINE)** : édition par pas FORCE (rotation FROT, déjà dans alter_seq) et CHANCE (chance[] déjà là),
        tenir touche(s) blanche(s) = pas 1..16 + KNOB. SEQ est maintenant une page rapide (LEN HITS ROT PROB). → QUESTION
        À POSER AVANT DE CODER : comment ouvrir l'édition par pas sans casser les pages rapides (ex. OCT−+OCT+ + SEQ =
        Felucca aujourd'hui) ? Pas 17..32 quand LEN > 16 ?
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
  - [x] **3e LFO (31/10, codé, testé, publié ; ATTEND L'ÉCOUTE)**. Choix de Romain : 1 LFO par cercle (celui de Felucca) +
        **1 LFO GLOBAL** (toujours les 3 cercles ; formes SIN TRI SAW SQR S&H SLEW RAMP+ RAMP- ; vitesse 1/16..8 MESURES
        comptée depuis PLAY ; 4 cases destination + quantité : PITCH SHAPE FILT LEVEL DIST DELAY REVRB PAN PROB). Le
        GLOBAL **prend le dessus** : sur une destination qu'il vise, la route LFO DEST du cercle (P_LD_*) et ses cases
        de matrice à source LFO/S&H/SLEW sont ignorées. **LFO 2e appui = page MATRICE** (onglets LFO 1 2 3 · GLOBAL ;
        ALGORITHM onglet, SELECT ligne ; ligne 0 = WAVE RATE SYNC du LFO ; lignes 1-4 = cases : K1 source, K2
        destination avec liste déroulante 1,2 s, K3 quantité ; LFO, HOME ou autre bouton de page = sortie). Nouvelles
        sources de matrice **POS** (rampe sur le cycle du cercle) et **FORCE** (dernier coup : ghost 0, normal 0,5,
        accent 1). Scènes : + WAVE SYNC LD_SHP + 4 cases (2 octets chacune) par cercle + le GLOBAL (banque 3780/3840 o ;
        la taille a changé → les slots USER enregistrés avant sont effacés au 1er démarrage). Scènes d'usine / NEW =
        GLOBAL éteint. Code : `alter_lfo.c` (inclus par mod.c : GLOBAL alt_gl, alt_gl_val, alt_msrc[3][2] POS/FORCE,
        alt_gl_take = masque MD_*, alt_gl_prob) ; mod.c : MS_POS MS_FORCE, mod_global + mod_keep (MOD_NKEEP 12) ;
        alter_play.c : alt_gl_tick (par bloc, sur la grille), alt_pos_src APRÈS les pas (alter_tick → alter_tick_),
        FORCE au coup, alt_seq.pmod (PROB) ; alter_ui.c : alt_matrix_input / alt_matrix_scene, alt_qpush.
        Tests (alter_emu_test) : MATRICE s'ouvre, GLOBAL SQR → PROB, RAMP+ → LEVEL, POS → AMP, le GLOBAL prend le dessus.
        Idées pas faites : DÉ/CHAOS qui tirent le LFO, source GLOBAL dans les matrices des cercles.
        Retour 31/10 appliqué : page rapide LFO nommée « LFO n » (= LFO du CERCLE CHOISI) ; MATRICE : noms de sources
        plus clairs (ALT_SRC_NAME) + une ligne qui explique la source de la case choisie (ALT_SRC_ABOUT), toutes gardées ;
        **barre qui ondule** en bas de l'écran cercles : trait couleur du cercle = valeur en direct quand un LFO / la
        matrice / le GLOBAL la bouge (mod.c mod_show → alter_ui.c alt_live ; FILTRE = E3 des 3 moteurs, FORME = E1
        ALTER / E2 NOISE, AMP sur LEVEL).
  - [ ] 3c : vue DEEP (EDIT ouvre encore la page EDIT de Felucca). Puis retirer l'interface Felucca. Reste : SCL → gamme
        des cercles, touches blanches libres, DESTROY assignable, verrous du dé, DOSE.
- [ ] Étape 4 (après le séquenceur) : tranche de console dédiée, ordre FIXE validé :
      son → **disto/fold → EQ 3 bandes → compresseur** → delay (echo, ping-pong, dub) → reverb.
      Existant Felucca (vérifié dans fx.c) : DIST, INSERT (SOFT/HARD/FOLD/FUZZ/CRUSH…), delay MONO à 1 ligne avec
      retour filtré (base possible pour le dub), reverb HALL/ROOM/SPRING, limiteur master.
      À créer : EQ 3 bandes, compresseur, delay ping-pong (stéréo). Coût CPU à mesurer.
## GitHub (créé le 30/10/2026)
- Compte : **Forban5929**. Dépôt : **fork PUBLIC de hugelton/Felucca nommé `alter-fm1`**.
- Branches : `main` = version ALTER qui marche ; une branche par étape (`etape-2-sequenceur`…) fusionnée
  dans main quand Romain a validé à l'écoute. Remote `upstream` = hugelton/Felucca, `origin` = le fork (récupérer ses correctifs).
- Nos fichiers préfixés `alter_` dans `firmware/src/` (eng_alter.c, alter_seq.c, alter_ui.c, alter_fx.c),
  outil PC dans `tools/alter/`, docs dans `docs/alter/`, `CLAUDE.md` à la racine. Sous-modules inchangés.
- Version navigateur : artifact claude.ai pour l'instant ; GitHub Pages plus tard (Romain devra activer Pages).
- Issues GitHub = retours d'écoute de Romain ; un jalon (milestone) par étape.
- La session cloud ne peut ni lister le compte ni créer de dépôt (proxy) : add_repo puis git push uniquement.
