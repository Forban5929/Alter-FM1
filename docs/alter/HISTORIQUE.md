# ALTER — historique détaillé des étapes (sorti de CLAUDE.md le 01/11/2026)

Détails de conception et de code de chaque étape, dans l'état où ils ont été écrits. CLAUDE.md garde le résumé ;
ici, le « pourquoi » et les noms de fonctions quand on retravaille une partie. Les mentions de fichiers retirés lors du
nettoyage du 01/11 (project.c, upreset.c, moteurs Felucca…) décrivent l'état d'AVANT ce nettoyage.

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
  - [x] **Refonte graphique « A · ALCHIMIE » (01/11, CODÉE, testée, publiée ; ATTEND L'AVIS)**. Maquettes :
        https://claude.ai/artifact/ATmBQMPtQ33hEiNbvVJkCD. Chaque écran = une CARTE DE TAROT (`firmware/src/alter_look.c`,
        inclus dans alter_ui.c) : alt_card (cadre double, coins en éventail, bandeau titre, chiffre romain), alt_chip /
        alt_chip_txt (cercle choisi en haut à droite), alt_tabs (onglets gravés), alt_dialk (knob = cadran doré à aiguille,
        plage LFO = arc couleur), alt_diamond, alt_star, alt_arc, alt_pt. Arcanes : PLAY XIV (ou celui de la page rapide),
        SEQ X, ENV XIII, FX XVI, LFO XVIII, EDIT I, SCL XVII, GLO XXI, SAVE XX, LOCKS XI, SETTINGS I.
        PLAY : rayons gravés, anneaux dorés (choisi = sa couleur), coups reliés en SIGIL (carré, triangle, étoile {n/k} dès
        5 coups), marques : losange (cercle 1), soleil (2), point (3), étoile sur le pas joué, aiguille dorée du cercle choisi,
        triangle serti d'or dans un cercle. SCL = roue des 12 notes (gamme = polygone, accord en pointillé). ENV = modes en
        onglets + aire hachurée. Polices : `assets/fonts/alter/` TeX Gyre Pagella Bold (GUST) à la place de Cinzel (pas de
        réseau pour la télécharger) + IBM Plex Mono (OFL) → build/gen/alter_fonts.h (tools/build.py ; ALT_FS 11 px, ALT_FM
        16, ALT_FL 26, ALT_FV 12, ALT_FN 16 ; +63 Ko de flash, NON vérifié sur FM-1). Couleurs : ALT_BG/INK/DIM/GREY/DARK/
        GOLD/FRAME/RIB/RIBTX ; grain sur les 2 thèmes (alt_paper). **Lissage (01/11, retour Romain « pixelisé, pas moderne »)** : lignes/anneaux/disques ANTI-CRÉNELÉS (alter_look.c alt_plot, alt_aaline16 Wu en 1/16 px, alt_aaring16 ; alt_disc par couverture) ; libellés en Inter (AF_S/AF_L), serif Pagella seulement dans le bandeau titre ; page navigateur en image-rendering auto (agrandissement lissé). **Knobs = PHASES DE LUNE (01/11, choix Romain)** : alt_dialk dessine une lune (nouvelle à 0, pleine au max ; LFO = arc couleur sur l'orbite) ; TEMPO = SABLIER (alt_hourglass) ; un MOT (FOLD, PHRY, C2) reste écrit, un NOMBRE n'apparaît que 1 s quand on tourne (nom en doré ; alt_dial_live dans la signature) ; fill -1 (LEN HITS ROT PROB, WAVE RATE…) = texte comme avant. **Repère d'instrument** dans les sous-menus : nom du son en haut à droite (alt_chip), moteur à côté du chiffre romain (alt_eng_tag ; SEQ : EUCLID/TURING). Maquette : planche Knobs de la page design. Pas fait : noms d'arcanes sous le chiffre.
  - [ ] 3c : vue DEEP (EDIT ouvre encore la page EDIT de Felucca). Puis retirer l'interface Felucca. Reste : SCL → gamme
        des cercles, touches blanches libres, DESTROY assignable, verrous du dé, DOSE.
- [ ] Étape 4 (après le séquenceur) : tranche de console PAR PISTE, ordre FIXE validé :
      son → **disto/fold → EQ 3 bandes → compresseur** → delay (echo, ping-pong, dub) → reverb. (Master fait en 3h.)
      Existant Felucca (vérifié dans fx.c) : DIST, INSERT (SOFT/HARD/FOLD/FUZZ/CRUSH…), delay MONO à 1 ligne avec
      retour filtré (base possible pour le dub), reverb HALL/ROOM/SPRING, limiteur master.
      À créer : EQ 3 bandes et compresseur par piste (réutiliser alter_fx.c), delay ping-pong (stéréo). Coût CPU à mesurer.

## 01/11 — Bug « annulation du CHAOS TOTAL » corrigé
- Cause 1 : les rotations faisaient `(rot + d + 4*AS_MAXLEN) % len` ; 128 n'est multiple que de 1/2/4/8/16/32, donc sur
  un cercle de 3, 5, 6, 7, 9… pas, SHIFT −1, le knob ROT tourné à gauche et la correction du combo tombaient faux
  (ex. LEN 6 : SHIFT −1 donnait ROT 1 au lieu de 5). Remplacé partout par `as_wrap(v, len)` (alter_seq.c).
- Cause 2 : `au.sh_app` (le tour d'un SHIFT, à défaire si c'est le début du combo) n'était jamais effacé : un SHIFT
  tapé longtemps avant était « défait » au combo suivant. Effacé désormais au relâchement de la touche (alter_ui.c).
- Test ajouté : alter_seq_test « SHIFT -1 then +1 on every LEN 1..32 ».

## 01/11 — REC : les knobs enregistrés sur les pas (choix de Romain)
- Choix : overdub tant que REC est allumé ; knobs de la page ouverte ; une valeur par pas, en douceur ; effacé par REC
  tenu 1 s (cercle), OCT− + OCT+ + REC (tout), DÉ / CHAOS. Pas de sauvegarde dans la scène (pas demandé, à faire).
- Pas réutilisé motion.c (Felucca, lié à son séquenceur qu'on retire en 3c) : nouveau `alter_rec.c`, inclus par
  alter_play.c après alt_move. ar_moved (dans alt_take, après chaque mouvement de la file) écrit la base au pas ar_now ;
  ar_step (dans alter_step, le pas joué de chaque cercle, kk[c] lu avant as_step) lance une rampe vers la valeur du pas
  suivant (ar_value : interpolation entre pas écrits autour du cercle) ; ar_tick (chaque bloc) avance les rampes et
  traite les effacements (ar_clr_cnt). ar_clear sur alt_roll_cnt / alt_preset_cnt.
- UI (alter_ui.c) : au.rec_t0 / rec_done, ALT_REC_CLEAR_MS ; LED REC dans alter_leds ; point rouge ALT_RED à côté du
  triangle PLAY (alt_scene) ; sig. Export test web_alter_rec / web_alter_knobp. Son des 27 scènes/sons identique au bit près.

## Clavier live réorganisé (01/11)
Demande de Romain : un clavier logique où l'on ne se perd pas. Gauche = quel cercle (MUTE blanches, CERCLE noires
juste à côté : une colonne par cercle), milieu = rythme (DÉ blanches, DENSITÉ noires au-dessus), droite = effets tenus
(toutes en noires). Choix de Romain : CHAOS TOTAL = les 3 CERCLE ensemble (comme 3 MUTE, 3 DÉ), dernière noire = RATCHET ×2,
5 dernières blanches libres pour plus tard. RATCHET ×3, REVERSE, BUILD UP retirés du clavier (code audio gardé).
- alter_ui.c : ALT_KEY (AK_SEL), ALT_TC_KEYS = noires 1 3 5, au.sel_was (le combo remet le cercle d'avant), LED de la
  touche CERCLE du cercle choisi allumée ; au.sh_app supprimé (plus besoin d'annuler les tours SHIFT).
- Tests alter_emu_test.mjs mis à jour (fills, DENSITÉ, CERCLE 2, CHAOS TOTAL) ; le contrôle « REC tenu 1 s » mesurait
  le mouvement du LFO de la scène avec 2 points de marge : il dépendait du moment (72 ↔ 77 selon le décalage) →
  mesure sur 4 mesures et marge de 6. Aide de la page (alter_page.py) mise à jour.

## Page web simplifiée (01/11)
Demande de Romain : trop de texte. alter_page.py : GUIDE = « Démarrer », le SCHÉMA DU CLAVIER (SVG : couleurs
quel cercle / rythme / effet tenu, lettre de l’ordinateur sur chaque touche), 3 lignes BOUTONS, 2 lignes COMBOS ;
l’ancien texte complet (DETAIL) est replié sous « Tout le détail ». Si le clavier change : régénérer la constante SVG.

## MASTER FILTER (01/11)
Demande de Romain : un filtre master façon Rodec Sherman Restyler / Ministyler, pour le **mastering** (couleur tonale,
saturation), pas pour jouer en continu, jamais randomisé. Choix validés : sur tout le mix (échos compris), vraie
auto-oscillation saturée, onglet FILTER sans touche dédiée, pas de REC, réglages A/B + MORPH gardés, drive doux puis sale,
sauvé dans la scène seulement s'il est allumé.
- `alter_filter.c` (inclus par alter_fx.c) : `afl_block` appelé dans fx.c mix_block juste avant `am_master` (le wet est
  plié dedans). DRIVE (tanh + biais asymétrique, au-delà de 64 écrêtage dur mélangé ; `AFL_DRV` gain, `AFL_MK` volume
  gardé : -0,85 × le gain, mesuré sur TECHNO, ±2,5 dB) → anti-continu → 2 étages SVF TPT (Simper) : 1er Butterworth
  (mélangé par `w24` : 12/24 dB sans clic), 2e résonant (`k` = 2 → −0,4) dont l'état bande passe dans un tanh à
  plafond `afl_lim` qui baisse avec RES (auto-oscillation ≈ −28 dB RMS, comme le mix). TYPE = fondu LP/BP/HP, BP ×2 k.
  Fréquence : table `AFL_G` (tan, 257 points, 20 Hz–18 kHz), recalculée tous les 8 échantillons (MOD, FM, DETUNE).
  Suiveur 5/100 ms sur le mix, un cercle (`afl_take` depuis ac_measure) ou le temps (`afl_beat`). MIX sin/cos, rampe 10 ms ;
  MIX 0 = `afs.run` 0 = rien ne tourne (rendu des 27 WAV identique au bit près).
- Page : alter_fxpage.c onglet AFP_FLT, `afl_kv` (valeurs), `afl_pic` (courbe calculée en entiers : `afl_resp`, `afl_l2`),
  `afl_ab` (barre A–B). OCT− / OCT+ seuls = édite A / B (afp.ab) et MORPH saute à ce côté. afp_input reçoit `pressed`.
- Scènes : alter_store.c `alt_bank_f` (objet 1, magic "ATLF") ; `afl_off` dans alt_load_scene / alt_new_scene.
- Non vérifié sur FM-1 : coût CPU (2 × 2 SVF + 4 divisions 64 bits tous les 8 échantillons).

## Nouveaux moteurs — RESO (01/11)
Romain veut plus de moteurs, tous indus, sous une logique commune à 4 knobs. Choix (AskUserQuestion) : RESO, PLATE,
RING, CHAOS + « corde froissée » (Karplus brutalisé) ; grammaire MATIÈRE · ÉCART · RÉSONANCE · SALETÉ pour les nouveaux
seulement ; drones dans une voix à part (partie 4) ; RESO frappable par 4 touches libres.
- `eng_reso.c` (moteur 10, ENGI_RESO) : 4 SVF TPT, g = tan via sine_i (sin/cos), 4 divisions par bloc ; excitation
  impulsion / clic plié (sinus ×3 plié) / bruit, chute DCAY ; RES → k (≥ 112 : k < 0, souffle de bruit pour démarrer) ;
  tanh sur l'état bande à plafond `lim` (DIRT plus bas et plus poussé) ; sortie 8√k × bande (k ≥ 0,03), NOTCH = Σ(x − k·v1).
  PING : reso_note_on choisit le masque (ALL, CYCL reso_cyc, RAND, 1-4) ou `reso_force` posé par une touche.
- Touches PING : alter_ui.c AK_PING (19 21 23 24) → alt_ping_tot ; alter_play.c `alt_pads` (fin d'alter_step) joue un
  coup sur la grille (note = ROOT du cercle, vél. 112, au moins un pas).
- Liste des moteurs : ALT_ENGS/ALT_NENG (4), stockage scène `o->eng` 0..3, alter_render.mjs lit les moteurs 0-9.
- 6 sons à la fin d'ALT_PRE (23..28), niveaux réglés par alter_render (−32,8..−34,7 dBFS). Les 23 anciens sons et 4 scènes :
  identiques au bit près. Test : 1 contrôle « RESO » dans alter_emu_test.mjs.

## RESO v2 (01/11)
Retour de Romain sur la v1 : « seul HOLLOW est intéressant et a du volume, les knobs ne changent pas grand-chose, toujours
le même genre de résultat ». Diagnostic : 4 filtres passe-bande frappés = 4 sinus qui s'éteignent (toujours « cloche ») ;
MATIÈRE ne changeait que l'attaque ; et un BUG : `m->cutoff` traité comme centré sur 64<<8 alors que c'est un décalage
autour de 0 → filtres 2 octaves SOUS la note (sourds). Choix de Romain : refaire en profondeur ; usages : percus indus,
hurlements/larsen, textures bruitées ; de HOLLOW il aime le volume/l'impact.
- Filtres en anneau : entrée = excitation (×4) + fb·tanh(0,6·soi + 0,4·précédent)/drv (DIRT : fb 0..3,2, drive 1..4 ;
  chante quand 0,6·fb > 1, DIRT ≈ 92) ; filtres +2 octaves ; SPRD 4 jeux (unisson, harmonique, 808, dissonant) ;
  MATR > 84 = frottement tenu pendant la note ; OUT LOW (passe-bas) ; SWEP remplace FINE (chute à la frappe sur DCAY).
- Mesuré avec `tools/alter/alter_knob_probe.mjs` (nouveau, op 14 web_alter) : sur ZAP, RES fait 0,15 → 1,7 s et
  600 → 2150 Hz ; DIRT 0,2 → 1,4 s ; MATR 460 → 1310 Hz. Sur les sons tenus (LARSEN, FRICTION) MATR agit peu : le
  feedback domine (assumé). 6 sons (indices 23..28) remplacés : ZAP CLANK HOLLOW LARSEN FRICTION GROWL.

## TRIGGER à la place de RATCHET ×2 ; PING 1-4 retirés (03/11)
Retour de Romain : RESO v2 « beaucoup mieux et utile » ; les PING « font souvent le même son ». Demande : à la place de
RATCHET ×2, une touche TRIGGER qui joue un coup sur n'importe quel son, et qui avec REC l'ajoute au cercle, calé sur un
des 32 pas. Choix (AskUserQuestion) : PING 1-4 retirés ; sans REC = tout de suite (pad) ; retaper un pas ajouté = rien ;
sauvé dans la scène.
- alter_seq.c : `as_circ_t.add` (32 bits) OR dans `as_repat` (EUCLID et TURING), `as_add`, tourné par `as_turn`.
- alter_play.c : `alt_trig_now(n)` chaque bloc (après alt_take) : alt_on(dernière note) ; si REC (UI : alt_trig_add_tot) et
  transport : pas le plus proche via alt_pos / alt_len ; si c'est le suivant, `alt_trig_skip` saute son coup une fois
  (dans alter_step). Arrêté : la note est lâchée après une double-croche (alt_trig_left).
- alter_ui.c AK_TRIG (touche 25), `alt_add_set` ; REC tenu 1 s / OCT−+OCT++REC effacent aussi les ajouts.
- alter_store.c : `alt_bank_f.add[16][3]` (objet 1, ancienne taille encore lue : ALT_BANK_F_OLD) ; scènes d'usine, NEW,
  patterns = ajouts à 0. eng_reso.c : `reso_force` retiré. Tests : RATCHET X2 retiré des contrôles de fills ; TRIGGER
  (son en 3 ms, ajout, sauvé/rechargé, effacé). Sons et scènes identiques au bit près.

## REC + fills, ROLL 1/32 (03/11)
Demande de Romain : en REC, ROLL 1/16 doit écrire ses coups comme des TRIGGER ; remplacer ROLL 1/8T par un 1/32 ;
et en REC les effets tenus (DUB DELAY etc.) sont enregistrés aussi.
- AF_ROLLT8 → AF_ROLL32 (alt_roll_period p/2), touche 22 « ROLL 1/32 ».
- alter_play.c : `alt_fxr[3][32]` (AFR_R32, AFR_DLY, AFR_REV, AFR_DST). Écriture : boucle des rolls (chaque coup de roll,
  ar_on → as_add du pas en cours ; 1/32 → AFR_R32 aussi) et chaque bloc pour les dubs tenus (pas en cours du cercle cible).
  Lecture : alter_step pose `alt_fxr_now[c]` pour le pas joué (R32 → rep = 1, le coup doublé) ; alt_take calcule les dubs
  PAR CERCLE (tenu sur la cible | enregistré) et applique les changements par alt_send(c, …) avec alt_fx_save[c][6].
- Effacés/chargés/sauvés avec les ajouts TRIGGER (alt_fxr_put ; alt_bank_f.fx[16][3][32], ALT_BANK_F_ADD = ancienne taille).
- Tests : ROLL 1/32 = 2 × les notes de 1/16 (compteur web_alter_hits) ; REC + ROLL 1/16 / 1/32 / DUB DELAY écrivent et
  rejouent (web_alter_fxr, web_alter_send) ; sauvé dans USER 04 ; REC tenu 1 s efface. Sons identiques au bit près.


## 07/11 — CONSOLE dans la page compagnon + MIDI LEARN ALTER
Retours de Romain : sons refaits et CONSOLE OK à l'écoute ; EQ/COMP **pas** dans REC (choix) ; TRS MIDI en pause (sans doute
son câble). Choix (AskUserQuestion) : learn par une **page LEARN dans SYSTEM** (pas de geste rapide), un knob lié = **un cercle
fixe**, **knobs + boutons** (actions), **carte CC fixe gardée** pour les CC non appris.
- LINK 4 : groupe CONSOLE (8 réglages par cercle, ajoutés en fin de `LK_V`) ; page : colonnes CONSOLE en haut d'EFFECTS.
- MIDI LEARN : `alter_learn.c` (UI, inclus dans la partie SYSTEM d'alter_ui.c) + `midi_control.c alm_midi` (côté audio,
  appelé en tête de `midi_event`, AVANT le filtre MIDI IN). Table : favorites.factory[5..8] (128 o ; [0] = 0xA1, effacée une
  fois si absent car Felucca y gardait des favoris), 42 liens × 3 octets (numéro | 0x80 note, canal | 0x80 utilisé, code).
  Codes : cercle c réglage t = c*32+t (`ALM_CT`, 30), GLOBAL 96+g (`ALM_GT`, 12), ACTION 128+a (`ALM_AT`, 30) — à ne
  jamais réordonner (la flash garde les codes). Côté audio : seulement la dernière valeur + un compteur par lien
  (`alm_io_s.v/seq`) ; `alm_poll` (1re chose d'alter_ui_input) applique : réglage du son = file `alt_qpush` (REC l'écrit),
  CONSOLE / RUMBLE / GLOBAL = direct ; action = touches/boutons virtuels `alm_keys` / `alm_btns` OR-és partout où
  alter_ui.c lisait `fm1_in.notes` / `fm1_in.buttons` (macros `ALT_NOTES` / `ALT_BTNS`) → mêmes combos que le clavier
  (CHAOS = DÉ 1+2+3, CHAOS TOTAL = CERCLE 1+2+3). Un « on puis off » dans la même passe = un tap (tenu une passe).
  Un contrôle = une chose (réapprendre déplace). Une note est refusée sur un réglage. CC jamais appris : 0 6 32 38 96-101 120-127.
  Ancien learn Felucca (`midi_learned`, factory[14]) retiré ; midi_learn.c réduit à `ml_count` (= 0).
- Tests : `tools/alter/alter_learn_test.mjs` (17, ajouté à pages.yml), peek 12 (mute/hits/sel/playing/REC), 100+P (param),
  400+i (lien i). Captures vérifiées (lignes, WAITING, CC21 CH16). **Pas sur la FM-1.**

## Archive : CLAUDE.md complet au 04/11/2026 (avant le rangement v4)
Gardé tel quel : le détail des étapes FLASH, page compagnon, hasard, REC, MASTER FILTER, RESO, réverbs…

### CLAUDE.md — Firmware « ALTER » pour M-VAVE FM-1 (né d'un fork de Felucca)

#### Utilisateur
- Romain, non-développeur, techno live indus. Explications simples, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Garder ce fichier court : le détail va dans
  `docs/alter/HISTORIQUE.md` (pourquoi et noms de fonctions de chaque étape).
- **A reçu sa FM-1 (03/11)**, PC Windows. Sait flasher ; utilise https://thefirmwarehub.com/ (sauvegarde, « Back to official
  V15 », secours = tenir OCT− à l'allumage, Rescue box). Le Hub n'installe pas un firmware perso non testé → notre page.

#### Étape FLASH (03/11)
- **Le vrai firmware se construit** : `.github/workflows/firmware.yml` (chaque push main/etape-*, artifact `felucca.fwsc` +
  journal ; les artifacts ne se téléchargent PAS depuis le cloud) et `pages.yml` publie la **page d'installation**
  https://forban5929.github.io/Alter-FM1/install/ (`tools/alter/make_install.py` = installateur Felucca `web/index_pkg.html`
  + fm1pkg.js + fm1ota.js, protocole USB intact, retour V15/secours → The Firmware Hub). Le cloud ne peut pas télécharger le
  compilateur JieLi : tout passe par GitHub. Suivre : `gh run list -R Forban5929/alter-fm1` / `gh run view ID --log`.
- Mesuré (04/11, après page v2) : image 384 Ko, RAM 69,3 / 73,7 Ko (≈ 4 Ko libres), pool 318 / 369 Ko.
- Corrigé pour la vraie puce : `alter_divdi.c` (__udivdi3/__divdi3/__umoddi3/__moddi3 : pas de bibliothèque du compilateur ;
  testé exact sur 5 M valeurs ; ~64 tours/appel) · `tools/size_fns.py` ignore les fichiers retirés · contrôle ROM de
  `tools/build.py` : `ROM_CONST_OK` autorise la limite −(1<<22) d'afl_block (comparaison, pas une adresse).
  Leçon : `inst` est une macro de tests/hostsim.c (ne pas nommer une variable ainsi) ; `0x02000000u` en dur = alerte « adresse ».
- **VERSION TEST** : PLAY en haut à gauche « CPU moy/pic % » (pic = max sur ~1 s, `alt_cpu_pk_q8` audio.c) + « CUT n » rouge
  = voix coupées par la protection Felucca > 85 % (`shed_count`) + trous dans le son (`alt_cpu_late`). `ALTER_CPU_SHOW`
  (alter_ui.c, 0 dans le navigateur). Doré si pic ≥ 75 %.
- **Essai sur la FM-1 (03/11, Romain)** : démarre, son OK, « smooth ». Charge lourde (3 cercles + LFO + MTX + MASTER FILTER
  + GLUE + delay/reverb) : médiane 60-70 %, pics 80 %, CUT apparaît mais **rien d'audible**.
- **2e CŒUR (choix de Romain, 03/11)** : OCT− + OCT+ + EDIT (page Felucca), **lâcher**, puis HOME tenu = MENU ; ALGORITHM
  = onglet SYSTEM (le dernier), PRESETS = 2ND CORE, OCT+ = ON (BETA), éteindre/rallumer. (HOME tenu avec les OCT = NEW SCENE.) Felucca y envoie des
  parts entières (mix_part : c1_parts/mix_split, fx.c). Vérifié : le code ALTER de mix_part (ac_measure, afl_take, AC_G,
  AM_DG) n'écrit que l'état de SON cercle ; le séquenceur ALTER tourne avant le partage (events_block). `ENG_VCOST` : RESO
  = 360 (était 0). `tests/dualcore_test.c` adapté (ALTER PHYS NOISE RESO + scènes avec alignement + source MASTER FILTER) :
  **10/10 identiques au bit près, 0 alerte ThreadSanitizer** (dans firmware.yml). Pile du 2e cœur (2816 o) :
  `tools/alter/stack_c1.py` → **1488 o (52 %)**, pire chemin phys_render (Felucca) ; estimation (appels indirects = moteurs).
  Écran : « 2C » après le % quand le 2e cœur prend des cercles. **Romain (03/11) : 2e cœur ON « tient bon »** (chiffres pas notés).
- **LE HASARD (03/11, retours de Romain)** — DÉ, 3 MUTE, CHAOS, CHAOS TOTAL : **jamais RESO** (`alt_rand_pre` ; RESO
  reste à la main dans PRESETS : « pas dans le style », brouillon au hasard), **jamais d'accord** (chord = AS_CH_OFF ; le
  DÉ tapé coupe l'accord du cercle), **BUDGET D'EFFETS** (`alt_fx_budget`, CHAOS et CHAOS TOTAL seulement, knobs à la
  main libres) : 6 parts 0..256 = DIST, INSERT (IA x IMIX), DELAY (envoi + couloir le plus fort, x FEEDBACK), REVERB
  (idem), LFO (|depth|/20), MTX (|AMT|/25) ; total ≤ ALT_FXB 384 : la plus forte reste, les autres baissent ensemble
  (4 passes). Test : `web_alter_peek` (alter_ui.c alt_peek) + contrôle « CHAOS / CHAOS TOTAL x24 ».
- **Vague des LFO** : l'écran se dessine en 2 moitiés ; l'horloge relue entre les deux décalait la vague (ligne au milieu)
  → `alt_draw_gl_n` / `alt_draw_lfo_p` figés une fois par image. (Non reproductible au navigateur : à confirmer sur FM-1.)
  Leçon : tout ce qui bouge (horloge, pas joué) doit être lu une fois par image, pas dans le dessin.
- Test REC : `web_boot` ne recharge pas la scène → le bloc charge la scène 1 (sinon il hérite du CHAOS TOTAL d'avant).
- **PANNE TROUVÉE (03/11)** : la FM-1 de Romain a la flash **UC25HQ80 (JEDEC 0xB36014)** des FM-1 récentes ; le firmware
  n'acceptait que 0x856014 → flash_ok = 0 : SAVE en RAM seulement et mise à jour USB refusée EN SILENCE (« stopped after
  0 requests »). Corrigé dans `alter_persist.c` (comme Felucca 7435517, issue #320). Mais l'ALTER déjà installé ne peut
  PAS recevoir la correction par USB-MIDI → chemin : OCT− + OCT+ tenus 5 s = UBOOT (mode ROM, marche sous ALTER) →
  logiciel M-VAVE officiel (Windows) → V15 → puis notre page /install/ depuis V15. (OCT− à l'allumage : ne marche pas.)
  `fm1_rescue.py` (bsp) : Mac seulement et refuse 0xB36014. Toujours vérifier une nouvelle puce : fl_jedec_ram.
  M-UPGRADE (M-VAVE) : même voie MIDI → « unable to upgrade » sous ALTER, et ne voit pas le mode UBOOT.
  **Kit de secours Windows** (choix de Romain : étape 1 = lecture seule d'abord) : `tools/alter/rescue/` = fm1_recover de
  Melodee (GPL) qui accepte aussi 0xB36014 ; `pages.yml` job `rescue` (windows) → .exe PyInstaller + 1-VERIFIER /
  2-SAUVEGARDER (rien n'est écrit) / 3-ECRIRE-ALTER (taper OUI) + LISEZMOI → https://forban5929.github.io/Alter-FM1/secours/alter-fm1-secours-windows.zip
  Le kit contient ALTER.fwsc (corrigé, contrôlé par l'outil) ; 3-ECRIRE-ALTER écrit ALTER (pas V15 : V16 = FM-1_016, flash.bin 0xA9000 > 0x93000, refusé par l'outil). Pilote WinUSB par Zadig pour 4C4A:8057 seulement. Jamais testé sur
  Windows + UC25HQ80 : faire 1 et 2, lire le texte, puis seulement 3.
  **FAIT le 03/11 (Romain)** : étape 2 OK (JEDEC 0xb36014, clé OK), étape 3 « RESTORE VERIFIED » (92 secteurs, ALTER corrigé
  8765e894…). **Confirmé par Romain (03/11) : tout marche, SAVE gardé, mise à jour depuis /install/ OK sous ALTER.**
  OCT− seul à l'allumage = fonction de Melodee (son propre chargeur), PAS de Felucca/ALTER (info du Hub relayée à tort).
  Filets de secours d'ALTER : OCT− + OCT+ 5 s = UBOOT (puce) + kit /secours/.
  Felucca publié (sans le correctif #320) installé puis coincé : sorti par UBOOT + kit (RESTORE VERIFIED, 129 secteurs).
  → Sur cette FM-1 : n'installer que des firmwares qui connaissent 0xB36014 (ALTER, M-VAVE, Felucca après #320).
- **PAGE COMPAGNON (03/11, choix de Romain)** : https://forban5929.github.io/Alter-FM1/editor/ (`?demo` = simulateur dans la
  page, sans son). Liaison **directe USB** (Web MIDI SysEx, Chrome/Edge), **3 colonnes = réglages des 3 sons** (son, 8 réglages
  moteur, ADSR, DIST CHOR DLY REV, INSERT TYPE/A/MIX, LEVEL PAN GLIDE), patchs **fichier .alter.json** + **scène USER dans
  la FM-1** (« Ranger ici » = SAVE tenu). Protocole **ALTER LINK** : `docs/alter/LINK.md`, `alter_link.c`, `web/alter/`,
  test `tools/alter/alter_link_test.mjs` (20/20, aussi dans pages.yml). **1er essai FM-1 (04/11, Romain)** : liaison qui
  « coupe » en changeant de pattern + colonnes sans réglages → corrigé (trames avalées par ota_service, voir LINK.md) : À REVÉRIFIER.
  2e essai (04/11) : coupure en enregistrant un patch + chargement raté → corrigé : prompt()/confirm() du navigateur
  GÈLENT la page (plus de PING) → fenêtres <dialog> dans la page ; SOUND répond par STATE (la page attendait SOUND).
  **Jamais prompt/alert/confirm dans cette page.** Test navigateur : `tools/alter/editor_browser_test.mjs` (10/10, voir en-tête).
  **Essai FM-1 (04/11, Romain) de la version ratchet/DRONE/FREE/réverbs + page : OK** ; « CUT » à 6 en 30 min, rien d'audible
  (laissé tel quel ; si audible un jour : chercher quel son/effet fait les pics). CPU CLOUD pas noté.
  **v2 (04/11, choix de Romain)** : en haut 3 colonnes égales = cercle dessiné en direct (PLAY poussé) + boutons (MUTE TRIGGER
  CHOISIR, DENSITÉ −/+, DÉCALER, DÉ PROPRE RYTHME, hasard par couloir + TOUTES), CHAOS / CHAOS TOTAL ; onglets SONS | RYTHMES
  (+ éditeur de couloirs glissé, Au hasard / À plat) | MODULATION | EFFETS. Protocole : LINK.md « v2 » (`LK_V`, `lk_act`).
  Tests : alter_link_test 39/39, editor_browser_test 15/15. **Pas encore essayé sur la FM-1.**
  **Look (04/11, Romain : « rester sur le style du synthé »)** : page = écran FM-1 (cartes de tarot alter_look.c, cadran PLAY : sigil,
  losange/anneau/point, étoile, triangle en 3 pièces ; points des pas = vélocité), thèmes NIGHT/DAY (bouton, localStorage), polices
  `web/alter/fonts/` (copiées par pages.yml), **tout en anglais** (page + noms LK_GRP/LK_V), ALGORITHM = ligne TYPE dans ENVELOPE.
  (Pistes brutalistes rejetées : canvas https://claude.ai/artifact/UKDYy7GAuyAv2KxmceStfV.)
  MODULATION (04/11) : sous-onglets LFO 1/2/3 et SLOT 1..4 (◆ = utilisé), LFO GLOBAL en bas sur toute la largeur ; COURBE de chaque LFO dessinée par la page
  (forme d'après WAVE/DEPTH, point qui court à TEMPO×RATE, PAS calé sur la phase du synthé, S&H = tirage fixe ; ~30 img/s
  seulement onglet ouvert + PLAY ; rien de plus par USB : `modColumn`, `lfoCurve`, `curveLoop`).
  **Relié à l'ÉMULATEUR web (04/11, choix de Romain)** : bouton « Connect the emulator » ; l'émulateur (racine du site) ouvert et
  allumé dans un autre onglet → BroadcastChannel « alter-link » (même site seulement : pas dans les artifacts) → index.html relaie
  → worklet.js message « link » (web_link_in, web_link_sync toutes les 20 ms tant que la page parle). Son + écran + clavier.
  **Écran unique (04/11, Romain)** : la page tient dans la fenêtre (en-tête, cercles, onglets, panneau qui défile DEDANS si trop
  petit) ; SIZE AUTO = plus grand zoom qui tient (`fit`, `applyZoom`) ou S/M/L/XL ; rangées de 30 px, 2 colonnes internes
  (`.gcols`, container queries) ; **PLAY STOP REC au milieu** (actions LA_PLAY/STOP/REC 26-28, PLAY poussé : bit 1 = REC) ;
  EFFECTS : DELAY REVERB | FILTER A/B (2 col.) | MORPH, MASTER en bas toute la largeur ; dessins `drawFx` (échos du delay,
  décroissance de la reverb ≈ RT approximatif, courbe du filtre d'après alter_filter.c) = l'idée, pas une mesure du son.
  Plus tard : enveloppes des modes PERC/STAB dans la page, patch fichier = scène entière ?, assistant d'installation façon baudgirl.
  (L'ancien éditeur Felucca, protocole « FL », est dans l'historique git, commit 8b8f548^ : web/EDITOR_PROTOCOL.md.)

#### Rôle de Claude
Ingénieur DSP + développeur C embarqué.

#### Démarrer une session
1. `add_repo Forban5929/alter-fm1` (push), `git clone --depth 1`, `git submodule update --init --depth 1`.
2. **Travail en cours sur la branche `etape-3d`** (`git fetch --depth 1 origin etape-3d && git checkout -b etape-3d FETCH_HEAD`).
   `main` = étape 2 validée ; on fusionne une étape dans main quand Romain l'a validée à l'écoute.
3. Construire + tester (≈ 1 min) :
   - `sh tools/alter/wasm/build_emu.sh` → `build/emu/` (felucca.wasm + `alter.html`). Génère `build/gen` si absent
     (supprimer `build/gen` pour régénérer polices/tables).
   - `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` (tout ALTER : cercles, pages,
     fills, dés, CHAOS, scènes, flash…) · `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm` (FX + MASTER)
   - `mkdir -p build/alter && cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
   - `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm` (page compagnon)
   - `node web/emu/emu_test.mjs build/emu/felucca.wasm` (ancien test Felucca : **4 échecs VOULUS** « a key sounds »,
     « FX held », « SAVE, OCT+, OCT+ », « a fresh instance… » : clavier, FX, SAVE appartiennent à la vue ALTER).
   - Vérifier la **version FM-1** sans son compilateur (syntaxe + fonctions manquantes) :
     `clang --target=wasm32 -fsyntax-only -std=gnu99 -w -Werror=implicit-function-declaration -Werror=undefined-internal -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 firmware/src/felucca.c`
   - Son de référence : `node tools/alter/alter_render.mjs build/emu/felucca.wasm DOSSIER` (WAV des scènes et sons) ;
     comparer octet par octet avant/après un nettoyage. Captures : `node tools/alter/alter_shot.mjs build/emu/felucca.wasm DOSSIER "b:PLAY w:500 s:nom"`.
4. Publier la page jouable : republier `build/emu/alter.html` + files felucca.wasm, worklet.js,
   fonts/DotGothic16-subset.woff, fonts/OFL.txt sur https://claude.ai/artifact/JpKofrvF7kb17ZmrJZmx8X (lire l'artifact avant).
   Page d'écoute : https://claude.ai/artifact/BqNkjf2dYiPQFnkgco6zey (`tools/alter/make_listen_page.py build/alter`).
   **Lien public pour testeurs (01/11)** : https://forban5929.github.io/Alter-FM1/ (MAJUSCULES : adresse sensible à la casse) — automatique (`.github/workflows/pages.yml`) :
   chaque push sur main ou `etape-*` construit + teste + publie ; le dernier push gagne. Version visible dans `/version.txt`.
   Romain a choisi : UN lien toujours à jour. Activé (Pages = GitHub Actions ; environnement github-pages autorise etape-*).
   Le cloud ne peut ni ouvrir github.io ni changer les réglages ; suivi : gh api .../actions/runs.
- Emscripten bloqué dans le cloud → clang système + mini-libc `tools/alter/wasm/`. Le compilateur JieLi (pi32v2) n'est
  PAS installé : rien n'est vérifié sur une vraie FM-1 (taille, CPU).
- `tools/alter/wasm/alter_page.py` : texte d'aide de la page entre apostrophes Python → écrire ’ (pas ').
- Le son du navigateur a ~0,3 s de retard sur les réglages : attendre ≥ 1 s avant de mesurer.

#### Vision (30/10, revue le 31/10)
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
- **CLAVIER LIVE (01/11, alter_ui.c ALT_KEY)** — gauche = quel cercle, milieu = son rythme, droite = effets tenus :
  blanches F G A = MUTE 1 2 3 · (B noire) · C D E = DÉ 1 2 3 · (F noire) · G A = SHIFT −1 +1 · B = LOCK (TURING) ·
  C..G hautes = **libres** (PING 1-4 retirés le 03/11). Noires : F# G# A# = **CERCLE 1 2 3** (= SELECT ; les 3 ensemble = CHAOS TOTAL) ·
  C# D# = DENSITÉ − + · F# G# A# = DUB DELAY, DUB REVERB, DESTROY (tenus) · C# D# = ROLL 1/16, **ROLL 1/32** (ex-1/8T, 03/11) · F# = **TRIGGER**.
  Retirés du clavier : RATCHET ×2 et ×3, REVERSE, BUILD UP (le code audio AF_* reste dans alter_play.c). Fills = cercle choisi.
- **TRIGGER (03/11)** = un coup du cercle choisi **tout de suite** (pad, dernière note, vél. 112, même muet) ; avec **REC tenu ou
  allumé** (transport en marche) le coup est AJOUTÉ au cercle sur le **pas le plus proche** (1re moitié du pas = le pas joué,
  sinon le suivant, dont le coup est sauté une fois : pas de flam). Ajouts = `as_circ_t.add` (alter_seq.c as_add, OR sur le
  motif EUCLID/TURING, tournent avec SHIFT). Retaper = rien. Effacés : REC tenu 1 s (cercle), OCT−+OCT++REC (tout), scène
  ou pattern chargés. **Sauvés dans la scène USER** (alt_bank_f.add, objet 1). Code : alter_play.c alt_trig_now (par bloc),
  alter_ui.c AK_TRIG, alt_add_set. Test : 2 contrôles « TRIGGER » dans alter_emu_test.mjs.
- **REC + effets tenus (03/11)** : REC allumé (ar_on), ce que le cercle cible joue sous un fill s'écrit sur le pas en cours
  et rejoue à chaque tour : ROLL 1/16 = coups ajoutés (as_add) · ROLL 1/32 = coups ajoutés + doublés · DUB DELAY / DUB
  REVERB / DESTROY = appliqués pendant ces pas. `alt_fxr[c][32]` (bits AFR_R32 DLY REV DST, alter_play.c) ; les dubs sont
  calculés PAR CERCLE chaque bloc (alt_dub_on, alt_fx_save[c] : tenu OU enregistré). Effacés et sauvés comme les coups
  TRIGGER (alt_fxr_put ; alt_bank_f.fx, objet 1, 3 tailles lues). Pas affichés à l'écran. 3 contrôles dans alter_emu_test.mjs.
- **SHIFT −1/+1 = tourne le cercle sélectionné d'un pas par appui, gardé.** Écran : cercle choisi = anneau plein ; dé actif =
  petit triangle à gauche du cercle, à sa couleur.
  **Centre (01/11)** : triangle coupé en 3 depuis son centre, une pièce par cercle (petit, ~22 px ; pâle au repos ; un coup l'éclaire un peu
  et la pousse de 2 px max, ~0,25 s ; « plus petit, plus discret » ; muet = gris ; point = cercle choisi) : alter_ui.c alt_prism/alt_tri/alt_glow.
- **Boutons de page = UNE règle (01/11)** : **appui court = page rapide** (4 KNOBS sur les cercles ; 2e appui ou HOME =
  fermée) · **tenu 0,5 s = MENU** du bouton (reste ouvert ; même bouton tapé, HOME ou autre bouton = sortie). Menus :
  SEQ (couloirs), FX (page FX), LFO (LFO 1 2 3 GLOBAL MTX), **ENV = enveloppe dessinée** selon le mode (knobs = page
  rapide ENV), **SCL = liste des gammes + clavier** (knobs = page rapide SCL), **EDIT = RÉGLAGES** (THEME NIGHT/DAY,
  KNOB 1 ou ALGORITHM ; SELECT = rangée), GLO pas de menu (tenu = court). Rapides : ENV = ATK DEC SUS REL (selon mode) ·
  FX = DIST, INSERT, DELAY, REVERB · LFO = LFO 1 · EDIT = 4 autres réglages du moteur · SCL = GAMME NOTE ACCORD DOSE ·
  GLO = TEMPO VOLUME PAN SWING. Code : alter_ui.c alt_menu_cur/alt_menu_set (seq_open, afp.open, mat_open, au.mn),
  au.mhold, ALT_MENU_HOLD_MS ; écrans alt_env_scene, alt_scl_scene, alt_set_scene, pied commun alt_foot.
  **THÈME** : ALT_DAY = ux.light ; DAY = palette Felucca PAPER, NIGHT = GREY (palette_choose + settings_save : gardé).
  Style TAROT (voir Refonte graphique) ; fondus en couleur alt_mixc (jamais RGB(0,0,0)).
  **OCT− + OCT+ tenus + HOME = NEW SCENE tout de suite** (01/11, alt_row_load(0,0), pages fermées, pas d'annulation).
  **OCT− + OCT+ tenus + bouton = vraie page Felucca** (HOME pour sortir). **OCT− + OCT+ + DÉ n = page VERROUS du cercle n** (7 touches blanches = rythme,
  hauteur, glide, enveloppe, timbre, force, vélocité ; HOME pour sortir). Verrous PAR CERCLE. En-tête : pointe de
  couleur du cercle au lieu de son numéro.
- Knobs : le chiffre n'apparaît que si KNOB n a tourné < 0,4 s avant (panel.c panel_knob_ms) : charger une scène, un son
  ou un dé ne fait plus clignoter de chiffres.
- LED clavier : utilisables **faibles au repos, fortes si engagées** (mute, fill tenu, flash du dé) ; sans fonction = noires.
- **ALGORITHM = mode d'enveloppe du cercle choisi** (31/10) : FREE (ADSR du son) · PERC · STAB (coups ≤ 2 pas). L'ADSR du
  son l'affine d'1/4, le dé d'1/3. Affiché à côté du moteur.
  **04/11 (Romain)** : **DRONE retiré** (cercles courts, triggers, rolls : ne colle pas ; « un autre firmware un jour ») :
  numéro 3 gardé pour les scènes, `am_ok()` le joue en FREE, `AM_NSEL` 3 (code DRONE resté, inatteignable) ·
  **FREE ≤ 4 pas** (2× STAB) et ≤ LEN−1 : tenue ≤ 3 pas, puis fondu rapide (`alt_free_left`, REL 34) · **cercles ≤ 16 pas**
  (`AS_PLAYLEN`, as_clean ; tableaux à 32 gardés ; scène AMBIENT : PADS 32/24 → 16, FREE) · **RATCHET au hasard** : 0 la
  norme, UN pas max par cercle, ×3 1 fois sur 16 (`alt_rat_draw` : CHAOS 20 %, CHAOS TOTAL 30 %, SHUFFLE RATCH 100 % ;
  dé : `as_draw`, ≤ 25 % à DOSE 127). Tests : alter_seq_test (2000 tirages), emu « FREE 4 pas », « LEN 32 → 16 ».
- **3 MUTE ensemble (01/11)** = cercle choisi : LEN 4..16 + HITS 1..LEN tout de suite, et VELO, CHANCE, PITCH tirés
  (valeurs fixes comme SHUFFLE) ;
  les mutes déclenchés par le combo sont annulés (alter_ui.c alt_lanes_random, au.mute_tog/mute_combo).
- **CHAOS TOTAL (01/11) = CERCLE 1 + 2 + 3 (3 premières noires)** : tout SANS limite sur les 3 cercles (moteur, son, 8 réglages,
  ADSR, mode + ses réglages, DIST/INSERT tout type, envois, **LFO 1 SEUL (DEPTH −20..20 sur 64, toute DEST sauf PITCH),
  MTX : 1 case max (70 %), AMT ±4..25, jamais PITCH/VIB** (adouci 01/11 : « pas musical »), rythme EUCLID/TURING, 6 couloirs, accord, note, gamme, delay/reverb partagés, dé DOSE 127). PAS tempo, swing,
  mutes, MASTER. Volume gardé : LEVEL du son, INSERT LEVEL ≤ 0 dB, FEEDBACK ≤ 100. **Même combo dans les 3 s = annulé**
  (photo exacte avant : alt_tcu, alt_tc_snap/alt_tc_undo ; le dé redevient propre). Le combo remet le cercle choisi
  d'avant (au.sel_was, au.tc_combo). Tests : 2 contrôles dans alter_emu_test.mjs.
  Corrigé 01/11 : rotation fausse sur LEN ≠ 1/2/4/8/16/32 (SHIFT −1, knob ROT, annulation) → `as_wrap` (alter_seq.c).
- **REC (01/11) = enregistrement des knobs sur les pas** (alter_rec.c, côté audio, branché sur la file alt_q dans alt_take) :
  REC tapé = overdub sur le cercle choisi (LED allumée, point rouge à l'écran) : tout knob de la page ouverte (son 8, ADSR,
  DIST, INSERT, envois DLY/REV ; pas LFO/MTX/ENV des modes/GLO) s'écrit au pas qui joue (ar_moved, ar_live = les pas
  traversés en tournant) ; REC retapé = rejoue en boucle, lissé (rampe d'un pas vers la valeur du pas suivant, ligne droite
  entre pas écrits ; ENUM = saut). Valeurs = base (sans le dé). 64 pas écrits / 6 paramètres par cercle. REC tenu 1 s =
  efface le cercle ; OCT− + OCT+ + REC = tout ; DÉ tapé, CHAOS, son/scène chargé = efface (ar_clear). LED faible = cercle
  avec mouvements. **PAS encore sauvé dans les scènes.** Test : 3 contrôles « REC » dans alter_emu_test.mjs.
- **ALIGNEMENT DES NIVEAUX (01/11)** après un hasard (DÉ tapé ou tenu, 3 MUTE, CHAOS, CHAOS TOTAL) : le cercle mesure
  son vrai niveau (après DIST/INSERT, à son LEVEL ; RMS par fenêtres de 23 ms, moyenne des 4 plus fortes) pendant ≥ 4 s
  et 8 fenêtres sonores (8 s max), puis un GAIN À PART (pas LEVEL : le VOLUME reste à Romain) le ramène à AC_TARGET
  (5600 = médiane des 23 sons, `tools/alter/alter_level.mjs`, `… chaos` = 10 tirages avec/sans) et le FIGE ; −18..+18 dB ;
  trop fort pendant la mesure (×1,5) = baisse immédiate ; ensuite GARDE-FOU : au-delà de +6 dB il baisse, jamais ne
  remonte. Son/scène choisis à la main = 0 dB (sons et scènes identiques au bit près). Sauver une scène intègre le gain
  dans son LEVEL. Annuler le CHAOS TOTAL remet les gains d'avant. Code : alter_fx.c ac_* (ac_poll dans mix_block,
  ac_measure dans mix_part/mix_spread, AC_G dans le niveau), alter_ui.c alt_cal/alt_cal_to (alt_cal_cmd/cnt).
  Mesuré : écart entre cercles 15-24 dB → 1-5 dB sur 8 tirages sur 10 ; restent faibles : sons à la limite +18 dB
  ou dont les coups se raréfient après la mesure.
- **CHAOS = DÉ 1+2+3 ensemble** : moteur, preset, 8 réglages, effets + LFO, rythme (immédiat), dé DOSE 127, gamme et
  notes de base, sur les 3 cercles. PAS le tempo, ni les modes, ni les mutes. DÉFINITIF (pas d'annulation).
  **Adouci (01/11, « pas musical »)** : LFO 1 seul, DEPTH −16..16 (sur 64), LFO 2/3 et GLOBAL coupés ; MTX : 1 case
  une fois sur 2 (SRC LFO ENV VEL RAND SLEW FORCE → CUT SHP PAN E1 E2, jamais la hauteur), AMT ±6..25.
- **MASTER FILTER (01/11, alter_filter.c)** — façon Sherman/Ministyler, outil de **mastering** (réglé puis laissé),
  sur TOUT le mix (échos compris), avant EQ/COMP/CLIP/limiteur. **5e onglet FILTER** de la page FX (pas de touche, pas de
  REC, **jamais touché par DÉ/CHAOS/CHAOS TOTAL**). 3 rangées (SELECT) : FREQ RES TYPE DRIVE / MOD SRC FM MIX /
  SLOPE DETUNE MODTYP MORPH. DRIVE 0..30 dB doux→sale, volume gardé (AFL_MK mesuré) ; RES > ~106 = vraie auto-oscillation,
  plafond qui baisse avec RES (≈ niveau du mix) ; SRC MIX/CIRC 1-3/1/4/1/8 (suiveur 5/100 ms ; FM = l'audio de la source).
  **Réglages A et B + MORPH** : OCT− = édite A (MORPH à A), OCT+ = B. 9 presets. MIX 0 = éteint = son identique au bit
  près. **Sauvé dans la scène USER seulement s'il est allumé** (objet flash 1) ; scènes d'usine/NEW = éteint.
  Tests : 7 contrôles « FILTER » dans alter_fx_test.mjs ; web_alter op 12/13, web_alter_flt.
- **NOUVEAUX MOTEURS (01/11, choix de Romain)** — tous indus. **Grammaire commune des 4 knobs, pour les NOUVEAUX
  seulement** (ALTER/PHYS/NOISE gardent les leurs) : E0 **MATIÈRE** · E1 **ÉCART** (harmonique→inharmonique) ·
  E2 **RÉSONANCE** · E3 **SALETÉ** ; E4..E7 sur EDIT. Feuille de route, dans l'ordre :
  [x] **RESO v2** (eng_reso.c, moteur 10 ; v1 rejetée : « toujours pareil, knobs sans effet ») : 4 SVF TPT EN ANNEAU
  (chacun nourri par lui-même 0,6 + le précédent 0,4, via tanh), 2 oct. au-dessus de la note ; MATR impact → bruit →
  frottement tenu (grésillement → bruit) ; SPRD unisson → harmonique → métal → dissonant ; RES ; DIRT = rétroaction
  (chante au-delà de ~92 : larsen) ; EDIT OUT MIX/NTCH/LOW, DCAY, PING, SWEP (chute de 4 oct. à la frappe).
  Romain veut : percus indus, hurlements, textures bruitées, du volume. 6 sons ZAP CLANK HOLLOW LARSEN FRICTION GROWL
  (−29..−32 dBFS). Vérifier qu'un knob agit : `node tools/alter/alter_knob_probe.mjs build/emu/felucca.wasm 23,24`.
  Leçon : m->cutoff est un DÉCALAGE autour de 0 (m->shape, lui, centré sur 64<<8). Romain : « beaucoup mieux et utile » (03/11).
  Les touches PING 1-4 (03/11) : retirées, « font souvent le même son » → remplacées par TRIGGER ; le réglage PING de RESO reste.
  [ ] **PLATE** (Crucible : 4-6 lignes de retard inharmoniques + filtre, cymbale/fêlée/courbe ; mémoire OK : 12,9 Ko/cercle ;
  entrée = un autre cercle, la FM-1 n'a pas d'entrée audio) · [ ] **CORDE FROISSÉE** (Karplus brutalisé : burst de bruit,
  distorsion + passe-haut + fold dans la boucle, allpass = inharmonique) · [ ] **RING** (2 carrés désaccordés → ring mod →
  disto lampe, interférence radio) **dans une VOIX DRONE à part = partie 4** (muette aujourd'hui, hors cercles ; détails à
  demander) · [ ] **CHAOS** = source de mouvement (forme d'onde LFO + source MTX), pas une voix.
  Ajouter un moteur : core.h ENGI_x + NENG_SHOWN, engines.c (include, case libre 0-8/12/13, ENGINE_ORDER, eng_ok),
  alter_ui.c ALT_ENGS/ALT_NENG, sons à la FIN d'ALT_PRE (numéros gardés), alter_render.mjs ENG.
- Vue **DEEP** (EDIT) : tous les réglages. Effets Felucca (DIST, INSERT FOLD, delay, reverb), ADSR Felucca.

#### Spécification moteur ALTER (`eng_alter.c`)
6 oscillateurs. E0 MORPH (sinus→tri→scie→carré) · E1 SPREAD (harmonique → cymbale 808 → barre libre) · E2 HARMONICS ·
E3 MODE SKIN/LIQUID/METAL · E4 AMOUNT (DETUNE / PUNCH / FM DEPTH selon MODE) · E5 NOISE (burst) · E6 TILT · E7 DRIFT.
3 voix (accords). Mesuré sur PC : ≈ 2 × ANALOG par voix (non vérifié sur FM-1).

#### Base technique
- JieLi AC79, cœur **pi32v2**. **Virgule fixe uniquement** (int32, Q15/Q24), pas de float/malloc/math.h.
  44 100 Hz, blocs de 32. API `engine_t` (`firmware/engines/src/felucca_engine.h`).
- **Un seul fichier compilé** : `firmware/src/felucca.c` inclut tout dans l'ordre (le nom reste, il est interne) ;
  la version navigateur passe par `web/emu/felucca_web.c` + `tests/hostsim.c` (garder les deux listes d'inclusion en phase).
- Sous-modules (ne pas toucher) : `firmware/engines/` (Felucca-Engines, MIT : NOISE vient de là), `firmware/bsp/`
  (matériel, MPL-2.0). Licence GPL-3.0 : le code reste ouvert, la page **ABOUT** (menu) doit garder les mentions légales.
- **Moteurs : ALTER (14), PHYS (9), NOISE (11), RESO (10).** `NENGINES` = 15 fixe dans core.h : les autres numéros sont
  des cases vides muettes (`ENG_GONE`, engines.c) pour ne pas décaler les numéros. Partie 4 : silencieuse.
- **Mémoire flash** (`storage.c`) : objet 0 = réglages (`alter_persist.c`) · objets 2, 3, 4 = banques ALTER
  (`alter_store.c` : rythmes des scènes USER, scènes, patterns + enveloppes) · objet 1 = MASTER FILTER + coups TRIGGER des scènes USER ·
  **objets 5.. LIBRES** (ex-presets, banque FM6, autosave Felucca) → place pour les PLAGES, les REC, plus de slots…
- **Mise à jour USB** (M-UPGRADE, `bsp/chip/ota.c`, `ota_hw.c`) : INTACTE. Ne pas changer l'identité `FM-1_900`
  (build.py PRODUCT) ni retirer « FM-1 » du nom USB (« ALTER FM-1 » : les outils de mise à jour cherchent /fm-1|felucca/).
  Le chargeur de mise à jour (bsp) s'annonce encore « Felucca Update » : on n'y touche pas (risque de brique).

#### Carte du code ALTER (`firmware/src/alter_*`)
- `alter_seq.c` séquenceur pur (3 cercles EUCLID/TURING, dé, couloirs, plages, accords, gammes) · `alter_play.c` côté audio
  (inclus en tête de seq.c : cercle i → partie i, fills, modes PERC/STAB/DRONE, file UI→audio `alt_q`)
- `alter_ui.c` vue PLAY + pages rapides/menus (inclus dans ui_input.c) · `alter_look.c` style tarot · `alter_fxpage.c` page FX
- `alter_fx.c` presets FX + MASTER (inclus dans fx.c ; inclut `alter_filter.c` = MASTER FILTER) · `alter_lfo.c` LFO + enveloppes par mode (inclus dans mod.c)
- `alter_sounds.c` 23 sons + 4 scènes d'usine · `alter_store.c` SAVE/LOAD · `alter_user_ambient.h` scène AMBIENT de Romain
- `alter_persist.c` réglages en flash · `eng_alter.c` / `eng_reso.c` les moteurs · `alter_legacy.c` **restes** (voir ci-dessous)
- Outils : `tools/alter/` (tests, rendu, captures, page d'écoute, `alter_wav.c`).

#### Restes de Felucca (à retirer à l'étape 3c)
Nettoyage du 01/11 fait (−59 000 lignes, wasm 1 036 → 690 Ko, son identique au bit près) : 12 moteurs, échantillons,
banque FM6, éditeur web, projets, presets utilisateur, autosave, tests Felucca ; écran d'allumage « ALTER 0.3 », nom USB
« ALTER FM-1 », page ABOUT « ALTER, built on Felucca ». **Reste, volontairement** (seule la trappe y mène) :
- l'**interface Felucca** (`ui*.c`, `menu_items.c`, `icons.c`…) via **OCT− + OCT+ + bouton** (GLIDE, MIDI, 2e cœur, USB…) ;
- son **séquenceur** (seq.c : seq_tick débranché, ALTER_FELUCCA_STEPS 0), `perform.c`, `motion.c`, `chord.c`,
  `song_chain.c` : tissés dans le transport/horloge MIDI que les cercles utilisent → retirer avec l'interface ;
- `alter_legacy.c` : noms et réponses vides que ces pages citent encore (lanes DRUM, presets/projets vides, chaîne).
La vue DEEP (3c) doit d'abord offrir en ALTER : GLIDE, canal/horloge MIDI, 2e cœur, audio USB, volume, ABOUT (licences).

#### État d'avancement (détail : docs/alter/HISTORIQUE.md)
- [x] Étape 1 moteur ALTER · [x] Étape 2 séquenceur 3 cercles + dé (dans main)
- Étape 3 (vues sur l'écran 240×240, ALTER remplace Felucca), branche `etape-3d`, tout codé/testé/publié,
  **ATTEND L'ÉCOUTE de Romain** : 3a navigateur · 3b vue PLAY · 3d-1 3 voix ALTER/PHYS/NOISE · 3d-3 scènes + 23 sons ·
  3d-4 SAVE/LOAD scènes/patterns · 3e LFO (3 par cercle + GLOBAL + MTX) · 3f enveloppes par mode · 3g menu SEQ
  (EUCLID/TURING, couloirs VELO CHNC DLY REV PITCH RATCH, plages, SHUFFLE, FLAT) · 3h page FX + MASTER ·
  refonte graphique tarot (lune, sablier, lissage) · **nettoyage Felucca phase 1 (01/11)** · **MASTER FILTER (01/11)**.
  Retours connus : TECHNO « parfait » (ne pas toucher) ; NOISE METAL un peu doux dans le mélange.
- [ ] **3c** : vue DEEP (EDIT) + écran SYSTEM ALTER, puis **retirer l'interface et le séquenceur Felucca** (phase 2 du
  nettoyage). Reste aussi : sauver les mouvements REC dans la scène (objets 5..), touches blanches libres, DESTROY assignable, verrous du dé, DOSE, sauver les PLAGES (objets 5..).
- [x] **RÉVERBS (04/11, choix de Romain)** : TYPE 0 **SHORT** (ex-ROOM) · 1 **GATED** (ex-SPRING : lignes HALL à decay
  long + porte ouverte par le send, SIZE = 60..500 ms ouverte) · 2 **HALL** (inchangée) · 3 **CLOUD** (2× les lignes,
  4 diffuseurs, dérive lente ±14, SIZE 127 ≈ 1 min). UN seul réseau (fx.c rev_hall + table `RV[]`), ROOM/SPRING de
  Felucca supprimés. Pool +10,6 Ko (rev_comb 9766, rev_ap 1518). Mesuré PC : CPU ≈ HALL, 60 s à send 127 stable.
  8 presets FX (HALL, BRIGHT HALL, CLOUD, ENDLESS, SHORT, BOX, GATED, LONG GATE). Outil test : web_alter op 16 (TYPE/SIZE/DAMP).
  **À écouter par Romain + CPU FM-1 avec CLOUD.**
- [ ] Étape 4 : tranche de console PAR PISTE, ordre fixe validé : son → disto/fold → EQ 3 bandes → compresseur → delay
  (echo, ping-pong stéréo à créer, dub) → reverb. Réutiliser alter_fx.c. Coût CPU à mesurer.

#### GitHub
- Compte **Forban5929**, dépôt public **alter-fm1** (renommé Alter-FM1, liens redirigés). Copies de secours dans le Projet
  (`claude/code/`). Une branche par étape ; issues = retours d'écoute. La session cloud ne peut pas créer de dépôt :
  add_repo puis git push. Avant de pousser depuis un clone superficiel : `git fetch origin <branche>`.

## Archive : CLAUDE.md au 10/11/2026 soir (v0.46), commandes + journal détaillé 04/11 → 10/11

## Les commandes (état au 05/11)
- Clavier : F G A = MUTE 1 2 3 · C D E = DÉ 1 2 3 · G A = SHIFT −/+ · B = LOCK (TURING) · noires F# G# A# = CERCLE 1 2 3
  (les 3 = CHAOS TOTAL, re-tapé < 3 s = annulé) · C# D# = DENSITÉ −/+ · F# G# A# hautes = DUB DELAY, DUB REVERB, DESTROY
  (tenus) · C# D# hautes = ROLL 1/16, **ROLL 1/8** (ex-1/32) · F# haute = TRIGGER. **C D E hautes = SONG A B C** ; F G hautes blanches : libres.
- **Dés (04/11)** : DÉ = pattern seul (HITS, ROT, flams, 1 ratchet) · **OCT− + DÉ** = vélo + chance seules · **OCT+ + DÉ** =
  hauteur + jets delay/reverb · DÉ tenu 0,6 s = propre · OCT− + OCT+ + DÉ = verrous · DÉ 1+2+3 = CHAOS (tout).
  Le timbre/glide/longueur par coup ne sont plus tirés que par CHAOS / CHAOS TOTAL.
- MUTE 1+2+3 = trois mutes (SHUFFLE retiré le 05/11 ; le tirage reste sur le bouton « Au hasard » de la page compagnon).
- **Vélo/chance au hasard** (dés, CHAOS, « Au hasard », SHUFFLE du menu SEQ) : vélo 100 % 70 %, 75 % 20 %, 50 % 10 % ; chance 100 % sauf
  une part qui monte avec le nombre de coups (aucune si ≤ 2 coups, max 45 %) : 75/50/25 % (plancher 25 %). `as_vpct`, `as_cpct`.
- Boutons de page : tap = page rapide, tenu 0,5 s = menu. Menus : SEQ, FX, LFO, ENV, SCL,
  **EDIT = DEEP** (liste de tous les réglages du son du cercle choisi : onglets SOUND / ENV / FX / MIX, SELECT = ligne,
  KNOB 1 = valeur via la file alt_q donc REC l'enregistre, touches noires = cercle ; `alt_deep_*`),
  **GLO = MIXER**, onglets (ALGORITHM) **MIX / I / II / III** : MIX = knob 1-3 LEVEL des cercles, knob 4 MASTER 100 = 0 dB,
  vumètre blanc/doré = limiteur/rouge = clip · **I II III = CONSOLE du cercle** (étape 4, 05/11 ; suit les touches noires) :
  2 rangées, SELECT = rangée, KNOB 1-4 : DRIVE (P_DIST) LOW MID HIGH / COMP DELAY REVERB LEVEL (P_DLY P_REV P_LEVEL via
  alt_qpush → REC ok). LOW MID HIGH COMP = `acs_v[c][4]` (64 64 64 0 = à plat = code sauté, bit pour bit ; 0 = KILL, 32 = −12,
  127 = +12 dB ; coupures 250 Hz / 2,5 kHz) ; COMP 1 knob : seuil −4..−22 dB, ratio 2..8, rattrapage auto (crêtes remises
  à leur niveau, max +12 dB), barre dorée = réduction. **Pas dans REC** (choix de Romain 07/11), gardé dans les scènes USER (`alt_ub.s.cs`, après
  `nm` ; objets plus courts = à plat), usine/NEW = à plat. **Page compagnon (LINK 4)** : onglet EFFECTS, colonnes CONSOLE. Delay/reverb = envois vers les bus
  communs (pas de delay par cercle : RAM). Tests : op `web_alter` **20** (a = cercle | valeur<<8, b), peek **11**.
  **HOME tenu = SYSTEM** (batterie en haut ; onglets SETUP, **LEARN** (voir MIDI LEARN), AUDIO, RANDOM, ABOUT ; SETUP : THEME, **SMOOTH** ON/OFF (alt_sys[8], 0 = ON), CLOCK INT/USB/TRS, MIDI IN, **TRS SEEN / USB SEEN**
  (lecture seule : messages MIDI reçus par prise depuis l'allumage, « CLK » si des F8 arrivent ; `alt_midi_n`, compté dans seq.c) · AUDIO : lignes Felucca
  CLICK (OFF/ON), CLICK LEVEL, SPEAKER EQ, USB LEVEL, TUNE, 2ND CORE · RANDOM : les 7 moteurs et modes FREE PERC
  STAB permis au hasard (la liste défile) · ABOUT : document GPL de Felucca, défile). Gardé en flash : `alt_sys` = favorites.factory[13]
  ([3] CLOCK, [4] MIDI IN, [5] MASTER, [6] TUNE ; le reste dans les réglages Felucca).
- **MIDI LEARN (07/11, `alter_learn.c`)** : SYSTEM > LEARN. Ligne GROUP (KNOB 1) = CIRCLE I/II/III (réglage d'UN cercle fixe :
  8 du son, ADSR, CONSOLE, PAN, INSERT, CHORUS, RUMBLE, GLIDE, HITS, DOSE ; CC seulement) / GLOBAL (MASTER, TEMPO, SWING, MASTER
  FILTER, DELAY, REVERB ; dernière ligne CLEAR ALL) / ACTIONS (touches + PLAY REC OCT± ARP ; CC ≥ 64 ou note). SELECT = ligne,
  KNOB 1 → = WAITING puis le 1er CC/note = lié ; KNOB 1 ← = effacé. Lien = canal + numéro exacts, avant le filtre MIDI IN ;
  un contrôle = une chose. Valeur absolue 0..127 (encodeurs en ABSOLU). Réglage du son par la file (REC l'écrit), CONSOLE et
  GLOBAL en direct. Actions = touches/boutons virtuels (`ALT_NOTES`/`ALT_BTNS`). Carte CC fixe Felucca gardée pour les CC non
  liés. Flash : favorites.factory[5..8] (magic 0xA1, 42 liens × 3 o ; **codes à ne jamais réordonner**, détail HISTORIQUE).
- OCT− + OCT+ + HOME = NEW SCENE · REC tapé/tenu 1 s/OCT−+OCT++REC. **OCT− + OCT+ tenus 5 s = mode UPDATE** (main.c,
  à garder absolument) · OCT− + OCT+ tenus à l'allumage = calibration des boutons (main.c panel_setup).
- **USER : 48 scènes (10/11 ; patterns RETIRÉS, choix de Romain : les variations = SONG A B C)**, page SAVE à 3 onglets
  (ALGORITHM) : SCENES A B C (A = NEW + 4 usine + A01-A16 ; B = B01-B16 ; C = C01-C16 : objets 32-39, 0xCC000-0xDBFFF, `alt_us_obj`). **SAVE tenu sur une ligne USER = page NAME** (3 caractères A-Z 0-9 -
  espace, KNOB 1 2 3 ; départ = nom existant, sinon 3 lettres du son du cercle 1 / HITS des 3 cercles ; SAVE = sauver,
  HOME = annuler) ; liste « A05  KCK ». Noms : `alt_ub.s.nm` (fin d'objet ; objets sans nm encore lus) ; RAM `alt_nm_s`. Une scène = un enregistrement `alt_us_t` (tout : son, rythmes, FX, filtre,
  pas ajoutés, fills, REC, PLAGES) lu en flash au chargement : objets 14-29 (2 scènes/objet), objets 30-31 = anciens patterns,
  plus lus (30 = marqueur de la migration, à garder), 0xA8000-0xCBFFF. En RAM : seulement les drapeaux `alt_used_s`. Anciennes banques (objets 1-7, 9 ; 16 places) = lues
  UNE fois au 1er démarrage et copiées en banque A (`alt_us_migrate`, marqueur = objet 30), puis plus touchées. Objet 8 = mêmes
  secteurs que 7 : NE PAS UTILISER. Mémoire (build CI 07/11) : code 329/568 Ko, **RAM 58/72 Ko
  (≈ 15 Ko libres)**, pool 351/360 Ko (≈ 17,5 Ko libres ; anciennes banques 11 Ko dedans, récupérables).
- Page FX DELAY : TIME (1/16 1/8 1/8D 1/4 seulement) FEEDBK **TAPE** SEND. Presets : ECHO, DUB, DUB LONG (1/4 au lieu de
  1/4D), PING-PONG, GHOST, TAPE (SLAP, SPACE, TRIPLET, 16TH DOT retirés). MODE/TAPE dans G_DRLVL/G_DRREV (inertes chez
  Felucca) et octets 28-29 de l'enregistrement FX des scènes.
- **SONG (05/11, simplifié le jour même ; `alter_song.c` côté audio + `alt_sg_*` dans alter_ui.c)** : C D E hautes = slots
  A B C (les 3 blanches avant les 2 dernières à droite). Tapée = ce slot joue à la mesure suivante (la chaîne s'arrête) ·
  **SAVE tenu + A/B/C** = sauver ce qui joue (la page SAVE se referme) · **tenir A + taper C puis B** = chaîne A>C>B
  (4 mesures par maillon), elle boucle · bord droit de l'écran PLAY, vertical, **seulement si une chaîne de ≥ 2 tourne** :
  ses maillons (celui qui joue plein) · ARP tapé = chaîne ON/OFF (repart du maillon 1) · ARP tenu = page SONG (liste
  seule + 4 knobs nommés en bas : SLOT, BARS 1/2/4/8, LINKS max 16, CHAIN ON/OFF ; SELECT = maillon).
  **Modifs live gardées dans le slot qui joue** (`sg_keep` avant chaque changement, RAM ; `sg_detach` quand une scène /
  un pattern / NEW SCENE est chargé : plus de slot). Slot = rythmes seuls
  (as_circ_t entier : pas, couloirs, PLAGES, tirage du dé, pas ajoutés, fills REC, mouvements REC, mutes ; PAS les sons ni
  root/chord/dose). Changement à la barre (alt_gstep % 16), chaque cercle repart du pas 1. Flash : objets 10-12 (slots) + 13
  (chaîne) à 0xA0000 (ex-samples Felucca, inutilisés). Indépendant des scènes. Chaîne OFF à l'allumage. Test : `web_alter_song`.
- **TURING tiré au hasard** (CHAOS TOTAL, « Au hasard ») : `alt_tur_draw` = LEN 5-16 (max de 2 tirages : plus près de 16),
  DENS 40-60 %, LOCK 50-75 %, RANGE 1-4.
- Écran PLAY : cercle choisi = anneau épais + halo de sa couleur ; lignes des triggers et points avec un contour couleur du fond.
- **LEDs du clavier (vue PLAY, 05/11)** : toute touche avec une fonction = pleine ; un état actif (mute, cercle, slot,
  TURING figé) bat : creux au niveau `nm` (mid ~12 %) 170 ms par seconde ; touche momentanée appuyée = creux ; sans
  fonction = éteinte (les LEDs n'ont pas de niveau entre ~60 % et plein, bsp).
- **Niveleur (alter_fx.c ac_*)** : après un tirage, le gain suit dès la 1re fenêtre sonore (plus 4-8 s), plage −18..+24 dB.
  Écart des 3 cercles dans les 2 premières s après CHAOS : 15,5 → 8,1 dB (moyenne, émulateur).
- **Horloge externe** : seq.c convertit seq_n (unités MIDI_CLOCK_GRID) en échantillons pour ALTER (`midi_alt_grid`) — avant,
  les cercles tournaient à GRID/beat × le tempo. Test emu : `web_midi_rt`, `web_clock_mode`, `web_gstep`.
- **Écran plus rapide (05/11, « laggy »)** : grain de papier précalculé (tuile 128×64 en .pool, ~5× moins cher) ; vue des
  cercles : si seuls les pas joués / flashs bougent, seules les **lignes touchées** sont redessinées et envoyées (étoiles
  ±11 px, alidade ; `alter_draw`), vérifié identique pixel à pixel à un redessin complet (émulateur). La page compagnon
  reçoit PLAY ≤ 30/s depuis la boucle principale : un écran plus rapide l'aide aussi. FPS réel FM-1 **non mesuré**.
- Sons cachés : `alt_pre_t.hide` (WIRE, = SITAR) hors PRESETS et hasard, gardé pour les scènes qui l'utilisent.
- **KICK** : knob 4 = RUMBLE (réverb sombre distordue, ducker par le kick, suit le niveau du kick ; 70 par défaut ; sauvé dans
  la scène, octet `rsv`). Seuls les sons avec `rumble` ≠ 0 dans ALT_PRE l'ont.

### État d'avancement
- [x] Étapes 1, 2 (main) · étape 3 complète sur `etape-3d` (vues, scènes, LFO, FX, MASTER FILTER, page compagnon, réverbs).
- [x] **v4 (04/11, retours jam v3)** : clics des dubs (envois qui glissent ~3 ms, TIME en fondu 46 ms), delays revus + TAPE,
  ROLL 1/8, MIXER, SYSTEM, dés séparés, vélo/chance, KICK gras + RUMBLE, BARS/BUZZ/HAT/ARCADE refaits (réglés aux
  mesures, **pas écoutés** : Romain juge). Tests : emu + fx + seq (3 nouveaux) + link OK. **À essayer sur la FM-1.**
- [x] 05/11 : SHUFFLE (3 MUTE) retiré ; SYSTEM complet (AUDIO + ABOUT). Le « pop » à l'extinction existe aussi sur le firmware
  officiel : matériel, on laisse. BCR32 : TRS MIDI IN
  type A (câble DIN → mini-jack type A) + carte CC fixe de Felucca (CC7 LEVEL, 10 PAN, 73/75/72 ATK/DEC/REL, 74/71 =
  timbre du moteur, 91 REV, 94 DLY, 5 GLIDE ; canaux 1-3 = cercles 1-3) ; un MIDI LEARN ALTER = à faire si besoin.
- [x] 3c (05/11) : REC + PLAGES sauvés · vue DEEP · **interface Felucca retirée** (trappe OCT−+OCT++bouton supprimée, ui_input
  ne fait plus qu'appeler ALTER, ui_draw_page = UBOOT ou ALTER ; −8 300 lignes de code mort, wasm 767 → 500 Ko). Gardé exprès :
  seq.c (transport + horloge MIDI des cercles), menu_items.c (lignes de SYSTEM), ui_menu.c menu_document (ABOUT), main.c
  (UPDATE, calibration), midi_*.c. MIDI LEARN de Felucca parti avec ses pages (un learn ALTER = à faire pour le BCR32).
  Méthode : `/tmp`-script « prune » = compiler avec -Wunused-function et supprimer ce qui est signalé, en boucle, HORS sous-modules.
- [x] 05/11 : PLATE, CORDE, RING + 4 sons chacun (GONG TOLE SCRAPE CRACKED · WIRE SITAR BOW BROKEN · RADIO RING BELL LASER
  SIREN, niveaux ≈ −31 dBFS sauf BROKEN −36) ; SYSTEM RANDOM : 7 moteurs (défilent), nouveaux ON (alt_sys[7]). CPU navigateur ≈ ALTER/RESO.
  **Réglés aux mesures, pas écoutés** : Romain juge.
- [x] 05/11 : **MODE SONG** (voir plus haut). Testé dans l'émulateur (8 tests), **pas sur la FM-1**. Test RUMBLE rendu
  insensible à la phase (il échouait selon le moment où il démarrait).
- [x] 05/11 : **32 + 32 USER** (voir plus haut). Migration testée avec l'ancien firmware dans l'émulateur (A01 + PLAGES,
  A16, pattern A03 retrouvés) ; banque B testée après extinction. **Pas sur la FM-1** : 1er démarrage = migration.
- [x] 05/11 (jam) : TURING au hasard borné, sélection du cercle plus visible + contours, NOMS des USER. Testé émulateur
  (+ compatibilité avec les scènes de la version 32+32 sans noms), **pas sur la FM-1**.
- [x] 05/11 : **page compagnon v3** (LINK 3) : NOMS des scènes, patterns USER, onglet SONG ; **guide des touches**
  `web/alter/guide.html` (`/editor/guide.html`, dessins dans le style du synthé, en anglais comme la page). Testé : link_test +
  navigateur (mode démo). **Garder le guide à jour** à chaque changement de commande.
- [x] **Étape 4 (05/11) : CONSOLE par cercle** (voir MIXER). Testé émulateur : 6 tests fx (LOW/HIGH KILL, MID +12, COMP
  crest ↓ sans saut de volume, retour à plat, scène USER), 2 cœurs (scène « + CONSOLE »), syntaxe FM-1. CPU navigateur :
  pas de différence mesurable. **Pas sur la FM-1, pas écouté** : Romain juge (surtout le COMP). (Faite dans une autre
  session en parallèle ; fusionnée.)
- [x] 05/11 soir, retours tests FM-1 (1-19 OK sauf) : SONG modifs live gardées, page SONG 4 knobs, chaîne à droite,
  LEDs, niveleur plus rapide, **bug horloge externe corrigé**, PLATE −1,8 dB, CORDE MATR plafonné (×90/127) + BROKEN refait
  (−37,7 dBFS), RING RDIO ×70/127 + RADIO RDIO 13 + SIREN refait. Pas écoutés.
- [x] 05/11 nuit : écran plus rapide (voir plus haut) ; WIRE caché ; WIND +13,5 dB, PAD +13,5, LARSEN −4, CRACKED −6.
- [x] 05/11 : Romain « en TRS rien n'est détecté » → lignes TRS SEEN / USB SEEN dans SYSTEM pour voir si un octet arrive.
  La prise TRS de la FM-1 est **type A** (matrixsynth, 10/2026). Firmware : entrée TRS compilée (FELUCCA_UART=1), testée émulateur.
  Retour Romain : TRS NOTHING avec du MIDI branché (USB NOTHING = normal : la page compagnon = SysEx du lien, pas compté).
  Ajout : « RAW n » si des octets arrivent (um.bytes) sans message valide. Pistes : câble type B / Ableton Sync ; essayer
  l'horloge par USB (CLOCK USB, Ableton → port FM-1).
- [x] 07/11 : Romain : sons refaits + CONSOLE OK à l'écoute ; TRS MIDI **en pause** (sans doute son câble). **CONSOLE dans la
  page compagnon (LINK 4)** + **MIDI LEARN** (page LEARN de SYSTEM). Testés émulateur (link 5 tests, learn 17 tests, captures,
  page en mode démo dans Chrome), syntaxe FM-1, 2 cœurs. **Pas sur la FM-1.**
- [x] 08/11 : **RUMBLE dans la page compagnon** (groupe 16 `LV_RMB` + drapeau par son dans INFO, LINK.md « v4 + RUMBLE ») :
  ligne RUMBLE en bas du groupe Engine, seulement pour les sons qui en ont un (KICK). **Page plus fluide** (editor.html
  `follow`/`animLoop`) : étoile + aiguille avancent en continu entre deux PLAY, vitesse = temps moyen entre pas mesuré,
  jamais plus d'1 pas d'avance (attend le pas suivant), saut = direct ; coup = anneau qui grandit et s'efface (`pulse`) ;
  rien de tout ça si « réduire les animations » est demandé par le système. Testé : link (3 tests RUMBLE), Chrome démo. **Pas avec la FM-1.**
- [x] 08/11 : **écran FM-1 fluide (VERSION TEST)**, SYSTEM > SETUP > SMOOTH (ON par défaut). 1er essai (étoiles qui
  glissent) : **6F** sur la FM-1 + Romain « étoiles trop présentes ». **Refait** : pas d'étoile ; l'alidade (cercle choisi)
  tourne en continu depuis `alt_msrc[c][0]` (POS : pas + temps dedans, horloge de l'audio) ; un trigger qui joue grossit
  de 2 px plus clair puis revient en 130 ms (`alt_hit_step`, `alt_sm_g`). Dessin : **petits rectangles** (`alt_sm_frame`,
  `alt_rc_add` : alidade en 6 boîtes ancienne + nouvelle place, trigger ±11, triangle du centre quand il brille ; fusion
  seulement si ça n'agrandit pas) ; gfx.c **`cv_ox`** (décalage x) ; la scène saute ce qui est hors du rectangle
  (`alt_miss` dans alt_disc/alt_tri/alt_aaline16, test d'anneau dans alt_aaring16, tête/carte et knobs sautés hors
  zone) ; heure de l'image `alt_now_ms`. Le triangle du centre (`alt_glow`) n'est plus dans le sig : avant, **chaque coup
  = image complète** (aussi en OFF : 25 → 11 écrans/s). Émulateur : identique pixel à pixel (ON/OFF, 300 images) ;
  ON = 4,5 écrans de pixels/s, coût ≈ 25 % d'une image complète. **« nnF »** = images/s sur la ligne CPU. Page compagnon
  au même style (aiguille sur chaque cercle, trigger qui gonfle `pop`). **FPS FM-1 de cette version non mesuré.**
- [x] 08/11 (3e retour : « plus smooth », triggers OK, mais ligne qui saute/accélère, peu visible) : **FM-1** : position de
  l'alidade = `alt_disp[c]` (alter_play.c alt_pos_src) à **vitesse constante sur chaque paire de doubles-croches** (le
  POS `alt_msrc` attendait sur les pas longs du SWING puis sautait ; mesuré SWING 60 : avant 0..852/image, maintenant
  237..286) ; tracée au 1/16 px (`alt_alid16`), 2 px, or 95 % ; **ligne CPU et knobs du bas hors du sig** en SMOOTH
  (`alt_sm_head`/`alt_sm_foot` → leurs rectangles) : avant, 2 images complètes/s (CPU) + chaque mouvement de LFO sur un
  knob = alidade figée puis rattrapage. **Page** : vitesse mesurée sur 16 pas (pas pas à pas), écart rattrapé sur 2 pas
  (`A.err`), plus de butée « 1 pas d'avance » ; mesuré en démo : avant 0..5,9 rad/s, maintenant 3,1..3,7. **Pas sur la FM-1.**
- [x] 08/11 soir : **Romain : « smooth sur les deux, même en swing »**. Mesuré FM-1 (« nnF ») : 10-15 images/s avec 3
  cercles qui jouent, 15-20 avec 2, 30 avec 1 (chaque coup = rectangle du trigger + rectangle du triangle du centre
  62×58 pendant 240 ms : le plus cher). Piste si besoin : triangle du centre moins cher / sans lueur en SMOOTH.
- [x] 08/11 nuit (Romain : « 30 images stables ») : en SMOOTH, triangle du centre = **flash 80 ms sans décalage**
  (`alt_glow` 3 ou 0, `push` 0) et **rectangle du seul morceau** qui change (`PB`) ; **SMOOTH limité à ~30 images/s**
  (`alt_sm_cap` 28 ms : 1 image toutes les 2 boucles de 15 ms ; 0 dans les tests d'identité, `web_alter_smcap`).
  Émulateur : pixels envoyés 4,9 → 2,0 écrans/s, identique pixel à pixel. **Pas mesuré sur la FM-1.**
- [x] 08/11 nuit : Romain « la ligne est coupée par endroits » : l'alidade était tracée SOUS les anneaux et les lignes
  des motifs (et leurs contours couleur du fond). alt_scene en 3 passes (`pass`) : 0 anneaux + motifs (alidade OFF ici,
  comme avant), 1 alidade SMOOTH, 2 triggers par-dessus. Vérifié en capture émulateur.
- [x] 09/11 : Romain « 10F en jouant, CPU > 60 % ». **JIANT** (juanjiant-bit/JIANT, fork Felucca) comparé : même LCD (SPI
  12 MHz DMA, écran entier ≈ 77 ms), même CPU (360 MHz ; leur option 320 = plus lent) ; eux = dessin plat pas cher redessiné
  entier + **cellules 8×4 hashées** (gfx.c cv_blit_cells : seules les cellules changées partent). Nous = dessin cher (AA),
  envoi déjà minimal → optimisé le **calcul** (pixel pour pixel identique) : `alt_plot` mélange par décalages (plus de
  divisions), `alt_aaring16` ne parcourt que les cordes de l'arc vu (`alt_turnof` = atan par table ; ordre des cordes gardé),
  rayons et places des pas précalculés (`alt_step_xy`), `alt_disc` couverture en table (.pool 2,3 Ko), knobs du bas un par
  un (`alt_sm_footk`). Image fluide navigateur 0,11 → 0,083 ms. **Ligne CPU : « nnF xxMS »** = temps de dessin moyen d'une
  image (`alt_draw_ms`) : beaucoup de ms = dessin lent ; peu de ms mais peu de F = boucle tenue ailleurs (audio).
- [x] 09/11 midi : Romain **« 18F 60MS 2C »** → c'est le dessin qui freine (60 ms > 33). Le coût = surtout le **nombre de
  rectangles** (chaque rectangle reparcourt toute la scène pour savoir quoi sauter). Alidade en **2 boîtes** (`alt_sm_seg`,
  mesuré : 6 → 73 µs, 3 → 61, 2 → 60, 1 → 68) ; **bandes de distance au centre** (`alt_band_set`/`alt_band` : rayons
  44-100 px, anneau ± halo r±10, pas r-12..r+22, triangle 0-32 sautés d'un bloc). Image fluide navigateur 0,083 → 0,06 ms,
  identique pixel à pixel (ON/OFF, avec et sans ligne CPU). Stats tests : `web_alter_smstat`. **Pas mesuré sur la FM-1.**
- [x] 09/11 13h : Romain **« 21F 46MS »** ; choix : dessin un peu simplifié en SMOOTH. **Halo du cercle choisi : 6 anneaux au
  lieu de 10** (un sur deux, +6 % d'opacité ; `alt_sm_simple_on`, 0 = comme avant via `web_alter_smstat(4,0)`) : −10 %.
  **`alt_aaline16` ne parcourt que le bout de ligne au-dessus du canevas** (axe principal + axe secondaire ; chaque pixel
  calculé seul → mêmes pixels) : −20 %. Image fluide navigateur 59 → 43 µs (≈ −27 % ; estimation FM-1 ~34 ms ≈ 28-30F,
  **pas mesuré**). Identique pixel à pixel (le halo allégé : comparé ON contre ON).
- [x] 09/11 13h40 : Romain **« 28F 24MS »** ✔. Puis essais de look l'après-midi (3 anneaux sur la ligne e927cc5, sextant
  31c6660/b148407, pendule, engrenage de montre 49bd767 : tout dans l'historique git) → **Romain 09/11 soir : retour à la
  version 28F (eae3f5c : ligne simple, triggers qui grossissent de 2 px) + triangle du centre FIXE** (plus de `alt_glow` :
  ni flash ni décalage au coup, rien du centre dans le sig ni dans les rectangles ; OFF 7 → 5 %). Page : `.piece` fixe.
  Gardé de l'après-midi : alter_look.c `alt_aaring16` mémorise l'arc par centre+canevas. Coût navigateur SMOOTH ON
  10-16 % d'une image complète (bruit). **Look validé par Romain : ne plus y toucher sans demande.**
  Puis (Romain, même soir) **une ligne par cercle** en SMOOTH : couleur du cercle (gris en MUTE), du centre à son cercle + 6 px
  (`alt_hand_r`, `alt_alid16c`) ; cercle choisi 2 px par-dessus, autres 1 px ; `alt_sm_dang[3]`, `alt_rc_alid(c, turn)`
  (2 boîtes si > 50 px, sinon 1). Lignes confondues si même LEN (normal). Coût navigateur ≈ +1-2 points (13 → 14-15 %).
  Page : `.needle` = couleur du cadran (`--c`). Test : op `web_alter` **21** = LEN d'un cercle ; `alter_shot.mjs` « A:op,a,b ».
  Si la FM-1 perd des images : Romain veut d'**autres idées graphiques** (pas juste couper).
- [x] 10/11 (jam) : **LFO menu** : le défilement redessine seulement la bande 42..166 (`alt_mat_scroll`), pages hors cercles
  sans pos/flash dans le sig (avant ~67 écrans/s). **LEN au hasard** 5-16 (max de 2 tirages, `alt_len_draw`/`alt_len_fit`) dans
  CHAOS TOTAL, TURING et **OCT− + DÉ** (sauf verrou RHYTHM). **Tirages sauvés dans la scène** : `alt_ub.s.dr` (après cs ; dé,
  dhits/drot, graines, treg TURING, prog + 11 o/pas × 16 ; `alt_dr_pack/unpack` alter_play.c, compteur `alt_dr_cnt`) → un
  CHAOS sauvé revient pareil, TURING **figé** au rappel ; objets sans `dr` encore lus. **PROG** (knob SCL, ex-DOSE ;
  `as_seq_t.prog/pbar`, `AS_PG` 10 progressions en degrés de la gamme, 1 mesure/accord, cercle réglé seul ; as_circ_t/as_cfg_t
  NON changés : slots SONG compatibles) ; **DOSE → DEEP** onglet MIX « DICE DOSE ». DEEP PAN affichait −64 % : corrigé.
  **CPU** (profil navigateur) : chorus/delay sautés quand vides (bit pour bit), division 64 bits rapide (`alter_divdi.c`, exacte :
  le plus gros gain attendu sur FM-1, filtres + COMP), MASTER FILTER 1er étage sauté si inutile, EQ MASTER à plat = sautée
  (fondu 256 éch. ; même niveau, phase différente). am_master 8 → 3 %. Reste cher : moteurs ~28 %, MASTER FILTER 15 %,
  rev_hall 7 %. **Rien mesuré sur la FM-1.** Le CI avait refusé ce build (constante −(1<<22) prise pour une adresse ROM
  dans am_master : ajoutée à `ROM_CONST_OK` de tools/build.py, comme afl_block) → publié en **v0.41**.
- [x] 10/11 **v0.42** (Romain : « LFO pas smooth ») : page LFO refaite (choix de Romain) : **plus de grille ni losanges ni
  forme pâle** ; barre « maintenant » à GAUCHE (x 12) + **point** (r 4) qui monte/descend avec le LFO ; la vague de la mesure
  À VENIR arrive de la droite (`alt_lw_calc`/`alt_lw_draw`) ; ligne pointillée du milieu gardée. Envoi : seules les colonnes
  qui bougent, par morceaux de 4 px, de la ligne la plus haute à la plus basse touchée (`alt_lw_frame`, `alt_lw_span`) ;
  liste DEST ouverte = bande entière comme avant. Émulateur : identique pixel à pixel ; SPI estimé 3-13 ms/image (avant :
  bande 30 k px ≈ 40 ms), ~33 images/s (cap 28 ms). Aussi : sur la page LFO, ligne CPU et knobs du bas **hors du sig**
  (ils n'y sont pas dessinés ; sur FM-1 = 2 écrans entiers/s pour rien). `alt_mat_scroll`/`alt_mat_seen` supprimés.
  **Pas mesuré sur la FM-1.** Page compagnon : LFO pas changé.
- [x] 10/11 **v0.43** (Romain : LFO OK « ça défile bien ») : **patterns retirés**, **banque C** (48 scènes) ; **fichier patch
  de la page = scène ENTIÈRE** (LINK cmd 23 BLOB, `alt_blob_t`/`alt_us_snap`/`alt_us_apply` alter_store.c, `lk_blob` ; patch v2 =
  sons + `scene` base64 ; ancien fichier/firmware = sons seuls) ; à l'ouverture la liste USER va sur la **1re case vide** (+ NOM
  du fichier) ; **onglet MIXER** de la page (CONSOLE + MUTE + fader LEVEL par cercle, fader MASTER = nouveau réglage partagé
  groupe 17 `LV_VOL` alt_mgain) ; la CONSOLE quitte EFFECTS. Détail : LINK.md « 10/11 ». Testé : link (BLOB aller-retour après
  NEW, somme fausse refusée, banque C, PAT muet), C16 après extinction (émulateur), page en démo dans **Chrome headless**
  (`/tmp/cdp.mjs` : google-chrome --remote-debugging-port, site servi par `python3 -m http.server` ; patch ouvert = même scène,
  MUTE des 2 côtés). CI : un littéral 0x10000u refusé (« register/window address », tools/build.py MMIO_LIT) → éviter
  les nombres 0x1xxxx/0x4xxxx/0x5xxxx/0x20xxxxx dans firmware/src. **Pas sur la FM-1.** Romain veut aussi (réponse du 10/11) : liste de ce qui manque pour une version
  publique (README ALTER, Release v1.0 sur main, manuel, tests FM-1, puces flash des autres FM-1, retour au firmware officiel).
- [x] 10/11 **v0.44** : **son USB dans la page compagnon** (la FM-1 = carte son UAC, `FELUCCA_UAC`) : groupe « Sound (USB) »
  (choix de l'entrée, « FM-1 » trouvée par son nom ; Listen ; Rec WAV 16 bits stéréo par AudioWorklet `AUD_WORK` + `wavFile` ;
  vumètre L/R) ; SCOPE retiré en v0.45 (Romain). getUserMedia sans traitement voix. Couper le haut-parleur : impossible par
  le firmware (pas de ligne d'ampli connue, menu_items.c) → SYSTEM > AUDIO > USB LEVEL FIXED + volume au minimum (l'USB reste
  à fond). Testé : Chrome headless (faux micro par oscillateur, enregistreur + WAV en OfflineAudioContext : 1 s, 44,1 kHz,
  crête juste) ; le headless n'a pas de sortie son (AudioContext suspendu) → écoute/scope **pas vus en vrai**, **pas avec la FM-1**.
- [x] 10/11 soir **v0.45** (Romain : son USB « top ») : onglet MIXER façon **table de mixage** (choix de Romain : knobs AU-DESSUS du
  fader) : par cercle DRIVE | HIGH MID LOW | COMP | DLY REV (knobs SVG `knob()` : arc 270° couleur du cercle, EQ part du
  milieu, glisser haut/bas, molette, flèches, double-clic = milieu/minimum), MUTE, fader LEVEL ; tranche MASTER ; oscilloscope
  retiré. Testé Chrome headless démo (clavier → VSET, double-clic EQ à plat). **Pas avec la FM-1.**
- [x] 10/11 soir **v0.46** (page) : COMP à côté de DRIVE en haut de la tranche ; **SIZE AUTO = taille choisie d'après la
  fenêtre seule** (min(largeur/1440, hauteur/940), 0,6..1,25, recalculée seulement si la fenêtre change ; plus d'après l'onglet :
  Romain, les changements de taille à chaque onglet cassaient la fluidité) → les onglets longs défilent ; **barres de défilement
  aux couleurs de la page** (::-webkit-scrollbar, Firefox scrollbar-color). Testé Chrome headless (zoom 0,95 sur tous les onglets).
- [ ] **PROCHAINE SESSION** : « nnF xxMS » de cette version (but : ~30F, < 33 ms ; 28F 24MS avant le triangle fixe). Si encore lent : dessin sur le 2e cœur ?
  alléger le son ? SPI (bsp : pas sans accord). Aussi MIDI LEARN avec le BCR32, CONSOLE de la page branchée, TRS (autre câble).
- [ ] Idées : un contrôle → plusieurs réglages (macro) ; LEARN dans la page compagnon ; DENSITY/ROT continus.

## 10/11 soir — v0.47 : puces flash inconnues
- Vérifié : Felucca amont (commit a4f007c, 04/11) n'accepte que Puya 0x856014 et UC25HQ80 0xB36014 (issues #319/#320) ; ALTER pareil.
- Choix de Romain : prévenir clairement, sans accepter d'autres puces (risque d'écriture ratée). Puce inconnue : écran 6 s au
  démarrage (main.c), ABOUT remplace la ligne de sous-titre (ui_menu.c), message « USB UPDATE OFF: UBOOT » à la demande M-UPGRADE,
  fm1_recover.py dit l'ID et « rien n'a été écrit ». ABOUT vérifié dans l'émulateur avec un faux ID 0x123456 ; l'écran de
  démarrage (firmware seulement) non vu.

## 10/11 nuit — v0.48 : retours des premiers tests FM-1
- Mesures Romain : 67/85 % CPU 27F 31MS (tous effets) ; 3 LFO + envois FX par pas : 18F 50MS, CUT 393 → chantier CPU.
- LOCKS refaits sur les dés actuels (RHYTHM LENGTH VELOCITY CHANCE PITCH THROWS SOUND) ; GLIDE/FORCE/TIMBRE/ENV regroupés
  dans SOUND. Les anciens bits sauvés changent de sens (4→LEN, 8→CHANCE, 64→THROWS, 16→SOUND), sans effet si non utilisés.
- Mode UPDATE : plus jamais pendant un combo OCT− + OCT+ + autre touche (la page LOCKS tenue 5 s arrêtait le jeu).
