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
   - `sh tools/alter/wasm/build_emu.sh` (→ `build/emu/`, ~30 s)
   - `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` · `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm`
   - `cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
   - `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm` · `node web/emu/emu_test.mjs …` (**4 échecs VOULUS**)
   - 2 cœurs (GitHub le lance) : `cc -std=gnu99 -O1 -w -fsanitize=thread -DFELUCCA_CPU2=1 -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Itests -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 tests/dualcore_test.c -o build/host/dualcore_test -lpthread -lm`
   - Syntaxe FM-1 : `clang --target=wasm32 -fsyntax-only -std=gnu99 -w -Werror=implicit-function-declaration -Werror=undefined-internal -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 firmware/src/felucca.c`
3. Outils : `alter_render.mjs wasm DIR` (WAV + niveaux de tous les sons), `alter_shot.mjs wasm DIR "w:500 B:GLO+ w:700 B:GLO- s:nom"`
   (captures), `alter_knob_probe.mjs`. Ops de test `web_alter` : liste en tête de `alt_web_cmd` (alter_ui.c) ;
   **17** = un réglage de piste (a = cercle | P_id<<8, 255<<8 = RUMBLE), **18** = RUMBLE coupé (les tests comptent les coups à l'audio), **19** = une PLAGE.
4. Pousser sur `etape-3d` → GitHub Actions construit le vrai firmware + publie https://forban5929.github.io/Alter-FM1/ (version
   dans /version.txt). Le cloud ne télécharge pas les artifacts ni le compilateur JieLi : **rien n'est mesuré sur la FM-1 depuis le cloud.**

## Règles techniques (vérifiées)
- JieLi AC79 pi32v2 : **entiers seulement** (Q15/Q24), pas de float/malloc/math.h ; 44,1 kHz, blocs de 32.
- Un seul fichier compilé `firmware/src/felucca.c` (inclut tout) ; navigateur : `web/emu/felucca_web.c` + `tests/hostsim.c`.
- RAM ~4 Ko libres (gros tampons → `__attribute__((section(".pool")))`, ~40 Ko libres). Sous-modules `firmware/engines`, `firmware/bsp` : ne pas toucher.
- Mise à jour USB : garder l'identité `FM-1_900` et « FM-1 » dans le nom USB. GPL-3.0 : garder la page ABOUT.
- 2e cœur : une partie ne touche que l'état de SON cercle (tout état par cercle, jamais partagé dans mix_part).
- Page compagnon (`web/alter/`, protocole `docs/alter/LINK.md`) : **jamais prompt/alert/confirm**.

## Carte du code ALTER (`firmware/src/alter_*`)
`alter_seq.c` séquenceur pur (cercles, dés `as_draw_w`, couloirs) · `alter_play.c` côté audio (fills, modes, file UI→audio) ·
`alter_ui.c` vue PLAY + menus · `alter_fxpage.c` page FX · `alter_fx.c` presets FX, MASTER, **RUMBLE** · `alter_filter.c`
MASTER FILTER · `alter_lfo.c` · `alter_sounds.c` sons + scènes d'usine · `alter_store.c` SAVE/LOAD · `alter_persist.c`
réglages flash · `eng_alter.c` / `eng_reso.c` moteurs · `fx.c` (Felucca + nos ajouts : delay MODE/TAPE, glisse des envois,
gain MASTER + vumètre). Moteurs : ALTER 14, PHYS 9, NOISE 11, RESO 10 (NENGINES 15 fixe).

## Les commandes (état au 05/11)
- Clavier : F G A = MUTE 1 2 3 · C D E = DÉ 1 2 3 · G A = SHIFT −/+ · B = LOCK (TURING) · noires F# G# A# = CERCLE 1 2 3
  (les 3 = CHAOS TOTAL, re-tapé < 3 s = annulé) · C# D# = DENSITÉ −/+ · F# G# A# hautes = DUB DELAY, DUB REVERB, DESTROY
  (tenus) · C# D# hautes = ROLL 1/16, **ROLL 1/8** (ex-1/32) · F# haute = TRIGGER. C..G hautes blanches : libres.
- **Dés (04/11)** : DÉ = pattern seul (HITS, ROT, flams, 1 ratchet) · **OCT− + DÉ** = vélo + chance seules · **OCT+ + DÉ** =
  hauteur + jets delay/reverb · DÉ tenu 0,6 s = propre · OCT− + OCT+ + DÉ = verrous · DÉ 1+2+3 = CHAOS (tout).
  Le timbre/glide/longueur par coup ne sont plus tirés que par CHAOS / CHAOS TOTAL.
- MUTE 1+2+3 = trois mutes (SHUFFLE retiré le 05/11 ; le tirage reste sur le bouton « Au hasard » de la page compagnon).
- **Vélo/chance au hasard** (dés, CHAOS, « Au hasard », SHUFFLE du menu SEQ) : vélo 100 % 70 %, 75 % 20 %, 50 % 10 % ; chance 100 % sauf
  une part qui monte avec le nombre de coups (aucune si ≤ 2 coups, max 45 %) : 75/50/25 % (plancher 25 %). `as_vpct`, `as_cpct`.
- Boutons de page : tap = page rapide, tenu 0,5 s = menu. Menus : SEQ, FX, LFO, ENV, SCL,
  **EDIT = DEEP** (liste de tous les réglages du son du cercle choisi : onglets SOUND / ENV / FX / MIX, SELECT = ligne,
  KNOB 1 = valeur via la file alt_q donc REC l'enregistre, touches noires = cercle ; `alt_deep_*`),
  **GLO = MIXER** (knob 1-3 = LEVEL des cercles, knob 4 = MASTER 100 = 0 dB, vumètre blanc/doré = limiteur/rouge = clip),
  **HOME tenu = SYSTEM** (batterie en haut ; onglets SETUP : THEME, CLOCK INT/USB/TRS, MIDI IN · AUDIO : lignes Felucca
  CLICK (OFF/ON), CLICK LEVEL, SPEAKER EQ, USB LEVEL, TUNE, 2ND CORE · RANDOM : moteurs ALTER PHYS NOISE RESO et modes FREE PERC
  STAB permis au hasard · ABOUT : document GPL de Felucca, défile). Gardé en flash : `alt_sys` = favorites.factory[13]
  ([3] CLOCK, [4] MIDI IN, [5] MASTER, [6] TUNE ; le reste dans les réglages Felucca).
- OCT− + OCT+ + HOME = NEW SCENE · REC tapé/tenu 1 s/OCT−+OCT++REC. **OCT− + OCT+ tenus 5 s = mode UPDATE** (main.c,
  à garder absolument) · OCT− + OCT+ tenus à l'allumage = calibration des boutons (main.c panel_setup).
- Scènes USER : tout + **REC (mouvements) + PLAGES** (05/11) dans les objets flash 5 6 7 9 (5 scènes par objet, lus/écrits à la
  demande via `alt_xb`, `alt_xsave`/`alt_xput` dans alter_store.c ; PLAGES sur 16 niveaux). Objet 8 = mêmes secteurs que 7 : NE PAS UTILISER.
- Page FX DELAY : TIME (1/16 1/8 1/8D 1/4 seulement) FEEDBK **TAPE** SEND. Presets : ECHO, DUB, DUB LONG (1/4 au lieu de
  1/4D), PING-PONG, GHOST, TAPE (SLAP, SPACE, TRIPLET, 16TH DOT retirés). MODE/TAPE dans G_DRLVL/G_DRREV (inertes chez
  Felucca) et octets 28-29 de l'enregistrement FX des scènes.
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
- [ ] Nouveaux moteurs (PLATE, CORDE, RING) · MIDI LEARN ALTER · touches blanches hautes libres.
- [ ] Étape 4 : tranche de console par piste (disto → EQ → comp → delay → reverb).
