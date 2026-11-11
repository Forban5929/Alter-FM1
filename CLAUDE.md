# CLAUDE.md — Firmware « ALTER » pour M-VAVE FM-1 (fork de Felucca)
Court par choix. Le détail (pourquoi, noms de fonctions, historique des étapes) : `docs/alter/HISTORIQUE.md`
(archives en bas : CLAUDE.md complet au 04/11 et au 10/11 soir = commandes détaillées + journal jour par jour). Mettre à jour CE fichier à chaque session, en le gardant court.

## Utilisateur
- Romain, non-développeur, techno live indus (Analog Rytm pour les drums). Français simple, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Retours de jam = issues/listes : résumer, clarifier, puis coder.
- A sa FM-1 (puce flash UC25HQ80 0xB36014), PC Windows. Installe par https://forban5929.github.io/Alter-FM1/install/
  (secours : OCT− + OCT+ tenus 5 s = UBOOT + kit /secours/ ; n'installer que des firmwares qui connaissent 0xB36014).

## Démarrer une session (≈ 2 min)
1. `add_repo Forban5929/alter-fm1` (push) · `git clone --depth 1` · `git fetch --depth 40 origin etape-3d:etape-3d` ·
   `git checkout etape-3d` · `git submodule update --init --depth 1`. **Travail sur `etape-3d`** ; `main` (vitrine GitHub) = même
   commit depuis le 10/11 : après chaque envoi, `git push origin etape-3d:main` (avance rapide seulement).
2. Construire + tester :
   - `sh tools/alter/wasm/build_emu.sh` (→ `build/emu/`, ~30 s ; `mkdir -p build/alter build/host` avant les cc)
   - `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` · `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm`
   - `cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
   - `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm` · `node tools/alter/alter_learn_test.mjs build/emu/felucca.wasm`
     · `node tools/alter/alter_smooth_test.mjs build/emu/felucca.wasm` (bandes = redessin complet, coût ON/OFF)
     · `node web/emu/emu_test.mjs …` (**4 échecs VOULUS**)
   - 2 cœurs (GitHub le lance) : `cc -std=gnu99 -O1 -w -fsanitize=thread -DFELUCCA_CPU2=1 -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Itests -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 tests/dualcore_test.c -o build/host/dualcore_test -lpthread -lm`
   - `node tools/alter/alter_save_test.mjs build/emu/felucca.wasm` (sauver/rappeler, tirages, PROG)
   - Page compagnon : servir `/tmp/site` (build/emu + web/alter → editor/) par `python3 -m http.server`, piloter Chrome headless
     par CDP (`--remote-debugging-port`, script type /tmp/cdp.mjs à refaire : Runtime.evaluate + Page.captureScreenshot) ;
     pas de sortie son en headless (AudioContext suspendu), getUserMedia à remplacer par un oscillateur.
   - Syntaxe FM-1 : `clang --target=wasm32 -fsyntax-only -std=gnu99 -w -Werror=implicit-function-declaration -Werror=undefined-internal -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 firmware/src/felucca.c`
3. Outils : `alter_render.mjs wasm DIR` (WAV + niveaux de tous les sons), `alter_shot.mjs wasm DIR "w:500 B:GLO+ w:700 B:GLO- s:nom"`
   (captures), `alter_knob_probe.mjs`. Ops de test `web_alter` : liste en tête de `alt_web_cmd` (alter_ui.c) ;
   **17** = un réglage de piste (a = cercle | P_id<<8, 255<<8 = RUMBLE), **18** = RUMBLE coupé (les tests comptent les coups à l'audio), **19** = une PLAGE, **21** LEN, **22** CHAOS TOTAL, **23** SAVE scène a, **24** LOAD (b&1 patt, b&2 NEW), **25** save ancien format, **26** PROG, **27** LFO.
   Peek 13 = empreinte tirage/pas d'un cercle, 14 = treg|tfreeze<<16|dé<<17|prog<<24, 15 = note. `alter_save_test.mjs` (sauver/rappeler), `alter_fxcost.mjs` (coût par effet, bruité).
   `web_alter_peek` : 11 CONSOLE, 12 mute/hits/sel/playing/REC, 100+P un param, 400+i un lien MIDI LEARN ; `web_midi(st,d1,d2)` = MIDI USB.
4. **Version** : `FELUCCA_VERSION` (felucca.c) = « v0.57 » (11/11) ; **+0.01 à CHAQUE envoi à Romain** (v0.42, v0.43…), à monter
   avant le commit. Affichée : démarrage, SYSTEM > ABOUT (en gros + date/heure de build, UTC), page émulateur, /version.txt.
5. Pousser sur `etape-3d` → GitHub Actions construit le vrai firmware + publie https://forban5929.github.io/Alter-FM1/ (version
   dans /version.txt). Le cloud ne télécharge pas les artifacts ni le compilateur JieLi : **rien n'est mesuré sur la FM-1 depuis le cloud.**

## Règles techniques (vérifiées)
- JieLi AC79 pi32v2 : **entiers seulement** (Q15/Q24), pas de float/malloc/math.h ; 44,1 kHz, blocs de 32.
- Un seul fichier compilé `firmware/src/felucca.c` (inclut tout) ; navigateur : `web/emu/felucca_web.c` + `tests/hostsim.c`.
- RAM ~16 Ko libres (gros tampons → `__attribute__((section(".pool")))`, ~19 Ko libres ; CI exige 4 Ko RAM / 8 Ko pool). Sous-modules `firmware/engines`, `firmware/bsp` : ne pas toucher.
- Mise à jour USB : garder l'identité `FM-1_900` et « FM-1 » dans le nom USB. GPL-3.0 : garder la page ABOUT.
- 2e cœur : une partie ne touche que l'état de SON cercle (tout état par cercle, jamais partagé dans mix_part).
- Page compagnon (`web/alter/`, protocole `docs/alter/LINK.md`, **v4** + BLOB + MIXER) + guide (`web/alter/guide.html`, à tenir à jour) :
  **jamais prompt/alert/confirm**.
- CI (tools/build.py) refuse les littéraux qui ressemblent à des adresses (0x1xxxx, 0x4xxxx, 0x5xxxx, 0x20xxxxx… `MMIO_LIT`) et
  certaines constantes (−(1<<22) : liste `ROM_CONST_OK`). Les sauvegardes : ne jamais changer `as_circ_t`/`as_cfg_t` (slots SONG)
  ni `alt_us_t`/`alt_blob_t` sans garder la lecture des anciens objets/fichiers (taille lue, champs ajoutés à la FIN).

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

## L'instrument en bref (détail des touches : guide.html ; détail technique : archive HISTORIQUE)
- 3 cercles euclidiens (CERCLE = noires F# G# A#), 7 moteurs, dés (DÉ / OCT−+DÉ vélo+chance+LEN / OCT++DÉ hauteur+jets / CHAOS /
  CHAOS TOTAL = tout, LEN 5-16), TURING (B = LOCK), REC des mouvements, ROLL, DUB, DESTROY, SONG A B C + chaîne (C D E hautes).
- Pages : PLAY (SMOOTH, une ligne par cercle — **look validé, ne plus y toucher sans demande**), SEQ, FX (DELAY TAPE, REVERB),
  LFO (vague qui défile vers la barre de gauche + point), ENV, SCL (SCALE ROOT CHORD **PROG**), DEEP (tous les réglages,
  DICE DOSE), MIXER (GLO : LEVEL, MASTER, CONSOLE par cercle DRIVE EQ COMP), SYSTEM (HOME tenu : SETUP, LEARN, AUDIO, RANDOM,
  ABOUT avec la version).
- Mémoire : **48 scènes USER** (banques A B C, objets 14-29 + 32-39 ; 30 = marqueur de migration ; patterns retirés), chaque
  scène = tout + NOM + CONSOLE + tirages (TURING figé au rappel). Slots SONG objets 10-13. Réglages `alt_sys` = factory[13],
  liens MIDI LEARN factory[5..8] (codes à ne jamais réordonner).
- Page compagnon : sons, rythmes, modulation, **MIXER (table de mixage)**, effets, song ; fichier patch = scène entière (BLOB) →
  liste USER sur la 1re case vide ; **son USB** (Listen, Rec WAV, vumètre) ; taille AUTO d'après la fenêtre ; scroll thémé.
- Mesures FM-1 connues : écran 28F 24MS (09/11, avant les dernières versions) ; CPU > 60 % avec 3 cercles + effets (09/11).

## Où on en est (11/11, v0.57)
**Retours de tests FM-1 (10/11, Sheet)** : tests 1-5 OK ; **CPU = problème n° 1** : 67/85 % 27F 31MS avec tous les effets,
et avec 3 LFO + envois FX sur les pas : 18F 50MS, **CUT 393** (2C était actif). v0.51 (choix de Romain) : **MASTER FILTER simple façon Felucca** (1 étage TPT, coefficients 1 fois par bloc, sauf FM :
tous les 8 ; RES max k = 0.12, ne crie plus ; presets SHERMAN → ENV WAH, SCREAM → PEAK ; MORPH A/B et les 11 réglages gardés,
côté droit sur le 2e cœur). L'ancien filtre « Sherman » archivé : `docs/alter/idees/filtre-sherman/` + branche git `idee-filtre-sherman` (v0.50 complète).
**Règle de Romain : ce qui est à 0 / éteint ne doit rien coûter** (filtre : DRIVE 0, 12 dB, MOD/MODTYP 64 + FM 0, bandes à 0,
MIX plein ; reverb : rien envoyé + traîne < 4 pendant 3 s → lignes vidées, plus calculée ; delay/chorus/DIST/CONSOLE déjà).
Profil natif (callgrind, /tmp/prof : felucca_web.c compilé en natif) : moteur ALTER ~29 % de l'audio, filtre 21 % → ~10 %
(sans DRIVE ~6 %), reverb ~9 %, master_out ~5 %. pi32v2 : 32x32→64 et MAC en 1 instruction (désassemblage dans le CI).
11/11 Romain (v0.51) : CHAOS TOTAL + 16/16 coups + MASTER FILTER + envois par pas + DIST x3 → 9F 100MS, 200+ CUT.
Vérifié (natif) : des coups denses n'empilent PAS de voix (une voix redéclenchée par cercle) → la « protection de charge »
(couper les traînes) n'aurait rien changé : retirée avant envoi. v0.52 : **ligne de profil sur la FM-1** sous la ligne CPU
(fx.c `apf_pct`, % du temps audio par ~1 s) : P parties de ce cœur, W attente du 2e cœur, X delay/chorus/reverb,
F MASTER FILTER, M master + sortie. Demander à Romain ces 5 chiffres (2C ON puis OFF) avant toute autre optimisation.
Mesure FM-1 (v0.52, « un peu tout ») : P39 W0 X12 F8 M8 (= 67 %), CUT 999. Les CUT étaient surtout la protection de Felucca
(audio.c shed : rendu + TIMER5 > 85 % deux demi-blocs de suite → une voix effacée), pas des trous. v0.53 : seuil à 95 %, la
2e ligne dit S (voix effacées) L (vrais trous, DMA en retard) P W X F M et T (TIMER5 imbriqué, %). W0 = le 2e cœur attend :
il a de la marge (piste : lui donner plus). tests/audio_test.c (Felucca, hors CI) : 8 échecs déjà là avant.
Mesure v0.53 : S et L > 500, T5. **Plan de refonte « robuste en live » : `docs/alter/REFONTE.md`** (à lire en premier ;
cause principale trouvée : `mix_split` ignore le travail fixe du cœur 0 et compte PLATE/CORDE/RING à 0 ; pics = ×1,5 la moyenne).
Références de Romain : Felucca et JIANT (github.com/juanjiant-bit/JIANT : prof.c, cpu_safe.c, FELUCCA_HOTRAM).
**v0.54 (REFONTE 1+2, Romain : « let's go »)** : `mix_split` mesuré (fx.c `part_tk` = pire temps récent de chaque cercle en
ticks TIMER4, `fix_tk[0/1]` = travail fixe de chaque cœur, tous les cercles peuvent aller au 2e cœur, hystérésis 1/16) ;
sans timer (tests hôte) l'ancienne table, corrigée. Ligne 2 : P W X F M **C** (travail du 2e cœur) T ; S/L en rouge sur une
3e ligne. Simulé (/tmp/prof/split.c) : cœur 0 88 % → 51 %. **À mesurer par Romain** ; ensuite REFONTE 3 (paliers) + 6.
**v0.55 : banc de test** (demande de Romain : tester sa vraie FM-1 depuis son navigateur). LINK : 30 DIAG (version, CPU,
F/MS, S, L, P W X F M C T, split, % par cercle, fixes, moteurs), 31 TEST (op, a, b en 5×7 bits = `alt_web_cmd` sur la machine),
32 PEEK (`alt_peek`). Page `web/alter/test.html` → https://forban5929.github.io/Alter-FM1/editor/test.html : Romain clique
« Connect the FM-1 » ; puis par Claude in Chrome (javascript_tool) : `await alt.diag()`, `alt.cmd(op,a,b)`, `alt.peek(c,w)`,
`alt.act(c,a)`, `alt.worst()`, `alt.watch(n)`, `alt.hist`. À retirer ou garder caché pour la 1.0 (TEST surtout).
**Banc de test branché le 11/11 14h** (onglet de Romain sur editor/test.html ; permission MIDI déjà accordée : Claude clique
« Connect » lui-même par javascript). 1re mesure v0.55 pire scène : W 22-29 %, L monte, 6-16F, parts 23/33/17 %, fix0 ~25 %.
Erreur v0.54 : les FX/filtre/master passent APRÈS les parties (ils ont besoin de leur somme) → ne pas les compter dans le
partage. v0.56 : `split_load` = max(parties cœur 0 + passage, parties cœur 1) seulement. Piste suivante : faire tourner les
bus FX du bloc N pendant les parties du bloc N+1 (1 bloc de retard sur les effets) pour paralléliser vraiment.
Mesure v0.56 (banc, pire scène ALTER RING RING) : CPU 73-84 %, pic 100+, L 1 → 36 en 8 s, 5-9F ; parts ~30-45 % chacune,
fixe ~28 % séquentiel. **v0.57 : bus FX en pipeline** (fx.c `bus_block` : delay/chorus/reverb sur les envois du bloc
d'avant, `fx_ps` → `fx_wet`, sur le 2e cœur AVEC ses parties ; 1 cœur : avant les parties ; 1 bloc = 0,73 ms de retard sur
les échos seulement). `c1_job` = bus + parties (vide aussi les sommes de mx[1]). rev_switch a son propre tampon. split :
cœur 1 = bus + ses parties. Dualcore identique (TSAN), fx test « keeps the MASTER » seuil 0.08 → 0.04.
Banc : si le panneau latéral est fermé, ouvrir son propre onglet test.html (tabs_context createIfEmpty) et cliquer Connect
par javascript (permission MIDI déjà accordée au site).
Test ops ajoutés : web_alter 29 = HITS d'un cercle ; peek 16 = voix qui sonnent d'un cercle.
LOCKS refaits en v0.48 (7 verrous
= les dés : RHYTHM LENGTH VELOCITY CHANCE PITCH THROWS SOUND ; bits 1 2 32 gardés, anciens 4 8 16 64 relus LEN CHANCE SOUND
THROWS) ; UPDATE (OCT− + OCT+ 5 s) seulement s'ils sont tenus SEULS (main.c `combo`, pas testable dans l'émulateur).
Tout ce qui précède est codé et testé **dans l'émulateur / Chrome headless**. Romain a validé sur sa FM-1 : LFO fluide (v0.42),
son USB dans la page (v0.44). **Pas encore vérifié sur la FM-1** : économies CPU (v0.41), tirages sauvés + PROG + LEN, banque C,
fichier patch entier, MIXER de la page, MIDI LEARN (BCR32), TRS MIDI (câble en cause, en pause).

## Feuille de route vers la 1.0 (proposée à Romain le 10/11 ; ordre à confirmer avec lui)
1. **Tests sur la FM-1** (le plus important) : liste FAITE (10/11, 44 tests, Sheet https://claude.ai/artifact/4wMzoCU4XtT5LgPXGqZvES ;
   la relire avant de coder : ses lignes « Problème » = le travail suivant) ;
   relever « nnF xxMS » et le % CPU avec filtre + comp + dist + delay + reverb ; longues jams sans plantage ni perte de scène
   (extinctions au milieu) ; sauver/charger A B C ; patch entier aller-retour ; MIDI LEARN avec le BCR32.
2. **CPU si coupures encore** : moteurs ~28 %, MASTER FILTER 15 %, rev_hall 7 % (profil navigateur) ; pistes : reverb sautée
   quand vide, filtre moins cher à son identique, dessin sur le 2e cœur.
3. ~~Compatibilité des autres FM-1~~ **FAIT v0.47** : Felucca (à jour au 04/11) n'accepte que 0x856014 + 0xB36014, nous pareil.
   Puce inconnue → écran « UNKNOWN FLASH 0x… » 6 s au démarrage, ligne dans ABOUT, « USB UPDATE OFF: UBOOT » au lieu du
   silence, outil de secours qui affiche l'ID (`flash_unknown`/`flash_jedec` dans ui_menu.c). Écran de démarrage pas vu en vrai.
4. **Sortie GitHub** : FAIT (10/11) : `build.py --release X.Y` garde `FM-1_900` (Felucca mettait FM-1_9XY, refusé par
   make_install) → `alter-X.Y.fwsc` + dossier release ; essai à chaque push (firmware.yml, `--release 1.0-ci`) ; brouillon
   `docs/RELEASE-NOTES.md` ; retour M-VAVE = lien The Firmware Hub sur la page d'installation (déjà là).
   `main` rattrapé le 10/11 (avance rapide). RESTE : la Release v1.0 **après** les tests FM-1.
5. **Documentation** : README réécrit pour ALTER + `docs/MANUAL.md` (anglais, 17 sections, d'après guide.html + le code)
   FAITS le 10/11 (v0.47). À tenir à jour avec guide.html. Reste : notes de version ; vidéo/audio de jam (Romain).
6. **Ménage** : cacher la ligne CPU « nnF xxMS » (outil de test) ; retirer « VERSION TEST » de SMOOTH ; textes écran tous en
   anglais ; page ABOUT à jour.
7. **Légal / politesse** : GPL + ABOUT déjà OK ; note sur l'aide de l'IA (comme Felucca) ; « inspiré de Basimilus », sans
   laisser croire à un lien avec Noise Engineering ; prévenir Leo Kuroshita (Felucca) avant l'annonce ; Issues GitHub ouvertes.

## Idées (après la 1.0, si Romain le demande)
Macro (un contrôle → plusieurs réglages) ; LEARN dans la page compagnon ; DENSITY/ROT continus ; PROG dans la page compagnon ;
même vague LFO dans la page compagnon.
