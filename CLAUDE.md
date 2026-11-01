# CLAUDE.md — Firmware « ALTER » pour M-VAVE FM-1 (né d'un fork de Felucca)

## Utilisateur
- Romain, non-développeur, techno live indus. Explications simples, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Garder ce fichier court : le détail va dans
  `docs/alter/HISTORIQUE.md` (pourquoi et noms de fonctions de chaque étape).
- **N'a pas encore de FM-1** : cible = version navigateur (le vrai firmware en WebAssembly), qui doit rester compatible FM-1.

## Rôle de Claude
Ingénieur DSP + développeur C embarqué.

## Démarrer une session
1. `add_repo Forban5929/alter-fm1` (push), `git clone --depth 1`, `git submodule update --init --depth 1`.
2. **Travail en cours sur la branche `etape-3d`** (`git fetch --depth 1 origin etape-3d && git checkout -b etape-3d FETCH_HEAD`).
   `main` = étape 2 validée ; on fusionne une étape dans main quand Romain l'a validée à l'écoute.
3. Construire + tester (≈ 1 min) :
   - `sh tools/alter/wasm/build_emu.sh` → `build/emu/` (felucca.wasm + `alter.html`). Génère `build/gen` si absent
     (supprimer `build/gen` pour régénérer polices/tables).
   - `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` (tout ALTER : cercles, pages,
     fills, dés, CHAOS, scènes, flash…) · `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm` (FX + MASTER)
   - `mkdir -p build/alter && cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
   - `node web/emu/emu_test.mjs build/emu/felucca.wasm` (ancien test Felucca : **4 échecs VOULUS** « a key sounds »,
     « FX held », « SAVE, OCT+, OCT+ », « a fresh instance… » : clavier, FX, SAVE appartiennent à la vue ALTER).
   - Vérifier la **version FM-1** sans son compilateur (syntaxe + fonctions manquantes) :
     `clang --target=wasm32 -fsyntax-only -std=gnu99 -w -Werror=implicit-function-declaration -Werror=undefined-internal -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 firmware/src/felucca.c`
   - Son de référence : `node tools/alter/alter_render.mjs build/emu/felucca.wasm DOSSIER` (WAV des scènes et sons) ;
     comparer octet par octet avant/après un nettoyage. Captures : `node tools/alter/alter_shot.mjs build/emu/felucca.wasm DOSSIER "b:PLAY w:500 s:nom"`.
4. Publier la page jouable : republier `build/emu/alter.html` + files felucca.wasm, worklet.js,
   fonts/DotGothic16-subset.woff, fonts/OFL.txt sur https://claude.ai/artifact/JpKofrvF7kb17ZmrJZmx8X (lire l'artifact avant).
   Page d'écoute : https://claude.ai/artifact/BqNkjf2dYiPQFnkgco6zey (`tools/alter/make_listen_page.py build/alter`).
- Emscripten bloqué dans le cloud → clang système + mini-libc `tools/alter/wasm/`. Le compilateur JieLi (pi32v2) n'est
  PAS installé : rien n'est vérifié sur une vraie FM-1 (taille, CPU).
- `tools/alter/wasm/alter_page.py` : texte d'aide de la page entre apostrophes Python → écrire ’ (pas ').
- Le son du navigateur a ~0,3 s de retard sur les réglages : attendre ≥ 1 s avant de mesurer.

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
  **OCT− + OCT+ tenus + bouton = vraie page Felucca** (HOME pour sortir). **OCT− + OCT+ + DÉ n = page VERROUS du cercle n** (7 touches blanches = rythme,
  hauteur, glide, enveloppe, timbre, force, vélocité ; HOME pour sortir). Verrous PAR CERCLE. En-tête : pointe de
  couleur du cercle au lieu de son numéro.
- LED clavier : utilisables **faibles au repos, fortes si engagées** (mute, fill tenu, flash du dé) ; sans fonction = noires.
- **ALGORITHM = mode d'enveloppe du cercle choisi** (31/10) : FREE (ADSR du son) · PERC · STAB (coups ≤ 2 pas) · DRONE
  (attaque lente, note tenue jusqu'au coup suivant). L'ADSR du son l'affine d'1/4, le dé d'1/3. Affiché à côté du moteur.
- **3 MUTE ensemble (01/11)** = VELO, CHANCE, PITCH du cercle choisi tirés au hasard, valeurs fixes comme SHUFFLE ;
  les mutes déclenchés par le combo sont annulés (alter_ui.c alt_lanes_random, au.mute_tog/mute_combo).
- **CHAOS TOTAL (01/11) = SHIFT −1 + SHIFT +1 + REVERSE** : tout SANS limite sur les 3 cercles (moteur, son, 8 réglages,
  ADSR, mode + ses réglages, DIST/INSERT tout type, envois, 3 LFO toute DEST, MTX : 1 case 50 % / 2 cases 25 % / 3 cases
  5 %, rythme EUCLID/TURING, 6 couloirs, accord, note, gamme, delay/reverb partagés, dé DOSE 127). PAS tempo, swing,
  mutes, MASTER. Volume gardé : LEVEL du son, INSERT LEVEL ≤ 0 dB, FEEDBACK ≤ 100. **Même combo dans les 3 s = annulé**
  (photo exacte avant : alt_tcu, alt_tc_snap/alt_tc_undo ; le dé redevient propre). Les tours des SHIFT et le REVERSE
  du combo sont annulés/muets (au.sh_app, au.tc_combo). Tests : 2 contrôles dans alter_emu_test.mjs.
- **CHAOS = DÉ 1+2+3 ensemble** : moteur, preset, 8 réglages, effets + LFO, rythme (immédiat), dé DOSE 127, gamme et
  notes de base, sur les 3 cercles. PAS le tempo, ni les modes, ni les mutes. DÉFINITIF (pas d'annulation).
- Vue **DEEP** (EDIT) : tous les réglages. Effets Felucca (DIST, INSERT FOLD, delay, reverb), ADSR Felucca.

## Spécification moteur ALTER (`eng_alter.c`)
6 oscillateurs. E0 MORPH (sinus→tri→scie→carré) · E1 SPREAD (harmonique → cymbale 808 → barre libre) · E2 HARMONICS ·
E3 MODE SKIN/LIQUID/METAL · E4 AMOUNT (DETUNE / PUNCH / FM DEPTH selon MODE) · E5 NOISE (burst) · E6 TILT · E7 DRIFT.
3 voix (accords). Mesuré sur PC : ≈ 2 × ANALOG par voix (non vérifié sur FM-1).

## Base technique
- JieLi AC79, cœur **pi32v2**. **Virgule fixe uniquement** (int32, Q15/Q24), pas de float/malloc/math.h.
  44 100 Hz, blocs de 32. API `engine_t` (`firmware/engines/src/felucca_engine.h`).
- **Un seul fichier compilé** : `firmware/src/felucca.c` inclut tout dans l'ordre (le nom reste, il est interne) ;
  la version navigateur passe par `web/emu/felucca_web.c` + `tests/hostsim.c` (garder les deux listes d'inclusion en phase).
- Sous-modules (ne pas toucher) : `firmware/engines/` (Felucca-Engines, MIT : NOISE vient de là), `firmware/bsp/`
  (matériel, MPL-2.0). Licence GPL-3.0 : le code reste ouvert, la page **ABOUT** (menu) doit garder les mentions légales.
- **Moteurs : ALTER (14), PHYS (9), NOISE (11) seulement.** `NENGINES` = 15 fixe dans core.h : les autres numéros sont
  des cases vides muettes (`ENG_GONE`, engines.c) pour ne pas décaler les numéros. Partie 4 : silencieuse.
- **Mémoire flash** (`storage.c`) : objet 0 = réglages (`alter_persist.c`) · objets 2, 3, 4 = banques ALTER
  (`alter_store.c` : rythmes des scènes USER, scènes, patterns + enveloppes) · **objet 1 et objets 5.. LIBRES**
  (ex-projets, presets, banque FM6, autosave Felucca) → place pour les PLAGES du menu SEQ, plus de slots…
- **Mise à jour USB** (M-UPGRADE, `bsp/chip/ota.c`, `ota_hw.c`) : INTACTE. Ne pas changer l'identité `FM-1_900`
  (build.py PRODUCT) ni retirer « FM-1 » du nom USB (« ALTER FM-1 » : les outils de mise à jour cherchent /fm-1|felucca/).
  Le chargeur de mise à jour (bsp) s'annonce encore « Felucca Update » : on n'y touche pas (risque de brique).

## Carte du code ALTER (`firmware/src/alter_*`)
- `alter_seq.c` séquenceur pur (3 cercles EUCLID/TURING, dé, couloirs, plages, accords, gammes) · `alter_play.c` côté audio
  (inclus en tête de seq.c : cercle i → partie i, fills, modes PERC/STAB/DRONE, file UI→audio `alt_q`)
- `alter_ui.c` vue PLAY + pages rapides/menus (inclus dans ui_input.c) · `alter_look.c` style tarot · `alter_fxpage.c` page FX
- `alter_fx.c` presets FX + MASTER (inclus dans fx.c) · `alter_lfo.c` LFO + enveloppes par mode (inclus dans mod.c)
- `alter_sounds.c` 23 sons + 4 scènes d'usine · `alter_store.c` SAVE/LOAD · `alter_user_ambient.h` scène AMBIENT de Romain
- `alter_persist.c` réglages en flash · `eng_alter.c` le moteur · `alter_legacy.c` **restes** (voir ci-dessous)
- Outils : `tools/alter/` (tests, rendu, captures, page d'écoute, `alter_wav.c`).

## Restes de Felucca (à retirer à l'étape 3c)
Nettoyage du 01/11 fait (−59 000 lignes, wasm 1 036 → 690 Ko, son identique au bit près) : 12 moteurs, échantillons,
banque FM6, éditeur web, projets, presets utilisateur, autosave, tests Felucca ; écran d'allumage « ALTER 0.3 », nom USB
« ALTER FM-1 », page ABOUT « ALTER, built on Felucca ». **Reste, volontairement** (seule la trappe y mène) :
- l'**interface Felucca** (`ui*.c`, `menu_items.c`, `icons.c`…) via **OCT− + OCT+ + bouton** (GLIDE, MIDI, 2e cœur, USB…) ;
- son **séquenceur** (seq.c : seq_tick débranché, ALTER_FELUCCA_STEPS 0), `perform.c`, `motion.c`, `chord.c`,
  `song_chain.c` : tissés dans le transport/horloge MIDI que les cercles utilisent → retirer avec l'interface ;
- `alter_legacy.c` : noms et réponses vides que ces pages citent encore (lanes DRUM, presets/projets vides, chaîne).
La vue DEEP (3c) doit d'abord offrir en ALTER : GLIDE, canal/horloge MIDI, 2e cœur, audio USB, volume, ABOUT (licences).

## État d'avancement (détail : docs/alter/HISTORIQUE.md)
- [x] Étape 1 moteur ALTER · [x] Étape 2 séquenceur 3 cercles + dé (dans main)
- Étape 3 (vues sur l'écran 240×240, ALTER remplace Felucca), branche `etape-3d`, tout codé/testé/publié,
  **ATTEND L'ÉCOUTE de Romain** : 3a navigateur · 3b vue PLAY · 3d-1 3 voix ALTER/PHYS/NOISE · 3d-3 scènes + 23 sons ·
  3d-4 SAVE/LOAD scènes/patterns · 3e LFO (3 par cercle + GLOBAL + MTX) · 3f enveloppes par mode · 3g menu SEQ
  (EUCLID/TURING, couloirs VELO CHNC DLY REV PITCH RATCH, plages, SHUFFLE, FLAT) · 3h page FX + MASTER ·
  refonte graphique tarot (lune, sablier, lissage) · **nettoyage Felucca phase 1 (01/11)**.
  Retours connus : TECHNO « parfait » (ne pas toucher) ; NOISE METAL un peu doux dans le mélange.
- [ ] **3c** : vue DEEP (EDIT) + écran SYSTEM ALTER, puis **retirer l'interface et le séquenceur Felucca** (phase 2 du
  nettoyage). Reste aussi : touches blanches libres, DESTROY assignable, verrous du dé, DOSE, sauver les PLAGES (objet 1 libre).
- [ ] Étape 4 : tranche de console PAR PISTE, ordre fixe validé : son → disto/fold → EQ 3 bandes → compresseur → delay
  (echo, ping-pong stéréo à créer, dub) → reverb. Réutiliser alter_fx.c. Coût CPU à mesurer.

## GitHub
- Compte **Forban5929**, dépôt public **alter-fm1** (renommé Alter-FM1, liens redirigés). Copies de secours dans le Projet
  (`claude/code/`). Une branche par étape ; issues = retours d'écoute. La session cloud ne peut pas créer de dépôt :
  add_repo puis git push. Avant de pousser depuis un clone superficiel : `git fetch origin <branche>`.
