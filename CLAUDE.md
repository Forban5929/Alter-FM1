# CLAUDE.md — Firmware « ALTER » pour M-VAVE FM-1 (fork de Felucca)
Court par choix. Le détail (pourquoi, noms de fonctions, historique des étapes) : `docs/alter/HISTORIQUE.md`
(l'ancien CLAUDE.md complet du 04/11 y est archivé en bas). Mettre à jour CE fichier à chaque session, en le gardant court.

## Utilisateur
- Romain, non-développeur, techno live indus (Analog Rytm pour les drums). Français simple, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Retours de jam = issues/listes : résumer, clarifier, puis coder.
- A sa FM-1 (puce flash UC25HQ80 0xB36014), PC Windows. Installe par https://forban5929.github.io/Alter-FM1/install/
  (secours : OCT− + OCT+ tenus 5 s = UBOOT + kit /secours/ ; n'installer que des firmwares qui connaissent 0xB36014).

## Démarrer une session (≈ 2 min)
1. `add_repo Forban5929/alter-fm1` (push) · `git clone --depth 1` · `git fetch --depth 40 origin etape-3d:etape-3d` ·
   `git checkout etape-3d` · `git submodule update --init --depth 1`. **Travail sur `etape-3d`** (main = étape 2).
2. Construire + tester :
   - `sh tools/alter/wasm/build_emu.sh` (→ `build/emu/`, ~30 s ; `mkdir -p build/alter build/host` avant les cc)
   - `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` · `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm`
   - `cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
   - `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm` · `node tools/alter/alter_learn_test.mjs build/emu/felucca.wasm`
     · `node tools/alter/alter_smooth_test.mjs build/emu/felucca.wasm` (bandes = redessin complet, coût ON/OFF)
     · `node web/emu/emu_test.mjs …` (**4 échecs VOULUS**)
   - 2 cœurs (GitHub le lance) : `cc -std=gnu99 -O1 -w -fsanitize=thread -DFELUCCA_CPU2=1 -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Itests -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 tests/dualcore_test.c -o build/host/dualcore_test -lpthread -lm`
   - Syntaxe FM-1 : `clang --target=wasm32 -fsyntax-only -std=gnu99 -w -Werror=implicit-function-declaration -Werror=undefined-internal -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 firmware/src/felucca.c`
3. Outils : `alter_render.mjs wasm DIR` (WAV + niveaux de tous les sons), `alter_shot.mjs wasm DIR "w:500 B:GLO+ w:700 B:GLO- s:nom"`
   (captures), `alter_knob_probe.mjs`. Ops de test `web_alter` : liste en tête de `alt_web_cmd` (alter_ui.c) ;
   **17** = un réglage de piste (a = cercle | P_id<<8, 255<<8 = RUMBLE), **18** = RUMBLE coupé (les tests comptent les coups à l'audio), **19** = une PLAGE.
   `web_alter_peek` : 11 CONSOLE, 12 mute/hits/sel/playing/REC, 100+P un param, 400+i un lien MIDI LEARN ; `web_midi(st,d1,d2)` = MIDI USB.
4. Pousser sur `etape-3d` → GitHub Actions construit le vrai firmware + publie https://forban5929.github.io/Alter-FM1/ (version
   dans /version.txt). Le cloud ne télécharge pas les artifacts ni le compilateur JieLi : **rien n'est mesuré sur la FM-1 depuis le cloud.**

## Règles techniques (vérifiées)
- JieLi AC79 pi32v2 : **entiers seulement** (Q15/Q24), pas de float/malloc/math.h ; 44,1 kHz, blocs de 32.
- Un seul fichier compilé `firmware/src/felucca.c` (inclut tout) ; navigateur : `web/emu/felucca_web.c` + `tests/hostsim.c`.
- RAM ~16 Ko libres (gros tampons → `__attribute__((section(".pool")))`, ~19 Ko libres ; CI exige 4 Ko RAM / 8 Ko pool). Sous-modules `firmware/engines`, `firmware/bsp` : ne pas toucher.
- Mise à jour USB : garder l'identité `FM-1_900` et « FM-1 » dans le nom USB. GPL-3.0 : garder la page ABOUT.
- 2e cœur : une partie ne touche que l'état de SON cercle (tout état par cercle, jamais partagé dans mix_part).
- Page compagnon (`web/alter/`, protocole `docs/alter/LINK.md`, **v4**) + guide (`web/alter/guide.html`) : **jamais prompt/alert/confirm**.

## Carte du code ALTER (`firmware/src/alter_*`)
`alter_seq.c` séquenceur pur (cercles, dés `as_draw_w`, couloirs) · `alter_play.c` côté audio (fills, modes, file UI→audio) ·
`alter_ui.c` vue PLAY + menus · **`alter_learn.c` MIDI LEARN** (+ `midi_control.c alm_midi` côté audio) · `alter_fxpage.c` page FX · `alter_fx.c` presets FX, MASTER, **RUMBLE** · **`alter_console.c`
CONSOLE** (EQ + COMP par cercle, appelée dans fx.c mix_part/mix_spread après DIST+INSERT) · `alter_filter.c`
MASTER FILTER · `alter_lfo.c` · `alter_sounds.c` sons + scènes d'usine · `alter_store.c` SAVE/LOAD · `alter_persist.c`
réglages flash · `eng_alter.c` / `eng_reso.c` moteurs · `fx.c` (Felucca + nos ajouts : delay MODE/TAPE, glisse des envois,
gain MASTER + vumètre). Moteurs (numéro, index ALT_ENGS) : ALTER 14 (0), PHYS 9 (1), NOISE 11 (2), RESO 10 (3),
**PLATE 12 (4)** `eng_plate.c`, **CORDE 2 (5)** `eng_corde.c`, **RING 3 (6)** `eng_ring.c` (05/11 ; NENGINES 15 fixe, libres :
0 1 4-8 13 — 13 = alt_sys en flash, ne pas prendre ; 1 = DIGITAL réservé). Knobs 1-4 des nouveaux = MATR SPRD RES DIRT ;
RES en durée (même longueur à toutes les notes, via reso_exp2). Ajouter un moteur : core.h ENGI_x + NENG_SHOWN, engines.c
(include + table + ENGINE_ORDER), alter_ui.c ALT_ENGS/ALT_NENG, alter_play.c ALT_TIMB_x, sons à la FIN d'ALT_PRE,
alter_render.mjs ENG, alter_link_test (nombre), tests/dualcore_test.c + hostsim.c (liste).

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
- **USER : 32 scènes + 32 patterns (05/11)**, page SAVE à 4 onglets (ALGORITHM) : SCENES A, SCENES B, PATT. A, PATT. B
  (A = NEW + 4 usine + A01-A16 ; B = B01-B16). **SAVE tenu sur une ligne USER = page NAME** (3 caractères A-Z 0-9 -
  espace, KNOB 1 2 3 ; départ = nom existant, sinon 3 lettres du son du cercle 1 / HITS des 3 cercles ; SAVE = sauver,
  HOME = annuler) ; liste « A05  KCK ». Noms : scènes `alt_ub.s.nm` (fin d'objet ; objets sans nm encore lus), patterns
  `p.rsv[3]` ; RAM `alt_nm_s/p`. Une scène = un enregistrement `alt_us_t` (tout : son, rythmes, FX, filtre,
  pas ajoutés, fills, REC, PLAGES) lu en flash au chargement : objets 14-29 (2 scènes/objet), patterns 30-31 (16/objet),
  0xA8000-0xCBFFF. En RAM : seulement les drapeaux `alt_used_s/p`. Anciennes banques (objets 1-7, 9 ; 16 places) = lues
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

## État d'avancement
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
- [ ] **PROCHAINE SESSION** : retour de Romain (fluidité + « nnF » FM-1, page branchée). Si besoin : alidade à ~30 images/s,
  SPI (bsp : pas sans accord). Aussi MIDI LEARN avec le BCR32, CONSOLE de la page branchée, TRS (autre câble).
- [ ] Idées : un contrôle → plusieurs réglages (macro) ; LEARN dans la page compagnon ; DENSITY/ROT continus.
