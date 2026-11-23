# CLAUDE.md — Firmware « ALTER » pour M-VAVE FM-1 (fork de Felucca)
Court par choix. Le détail (pourquoi, noms de fonctions, historique des étapes) : `docs/alter/HISTORIQUE.md`
(archives en bas : CLAUDE.md complet au 04/11 et au 10/11 soir = commandes détaillées + journal jour par jour). Mettre à jour CE fichier à chaque session, en le gardant court.

## >>> v1.1 (décidé par Romain le 22/11) <<<
Règle : **une liste de besoins par version (Sheet) ; on publie (Release) quand la liste est finie.** Entre deux : BETA seulement.
**Sheet de la liste : https://claude.ai/artifact/FhZkHbyXGPKKUxAC4wwPVi** (Sheets, nœud `af8de520-948a`, onglet « v1.1 » :
# Besoin Statut Catégorie Priorité Source « Fait en » Notes ; lire avant de coder, mettre à jour en fin de session).
1. ~~Site BETA~~ FAIT v1.02. 2. **Bouton Feedback** (v1.02, `web/alter/feedback.js`, BETA seulement jusqu'à la v1.1) →
   relais Vercel `https://alter-feedback.vercel.app/api/feedback` (`tools/alter/feedback-relay/`, projet alter-feedback,
   équipe rominou1) → Issue `a-trier` + `feedback` + `cat-…` + `v-…`. `GITHUB_TOKEN` mis par Romain le 22/11.
   **Redéployer = nouveau déploiement avec le fichier** (un « redeploy » d'un ancien déploiement garde ses anciennes variables). En début de session :
   `gh api 'repos/Forban5929/alter-fm1/issues?labels=a-trier'`, résumer à Romain, ajouter au Sheet ce qu'il accepte, retirer `a-trier`. 3. **Son** : ~~insert DIST~~ **v1.03** (fx.c `track_dist` : DRIVE 1..32 = fondu sec→saturé `DIST_XF`, make-up +2,6 dB à 1 → 0 à 64 ;
   0→1 = 0,1 dB au lieu de −2,5 dB ; **validé par Romain « smooth »**) ; ~~INSERT DRIVE~~ **v1.04** (track_insert : SOFT..FUZZ, A 0..`INS_XF` 32 = fondu,
   A 0 = rien ne tourne ; presets ≥ 40 inchangés ; reste : creux −4 dB vers A 20-32 sur sons brillants avec TONE/LEVEL par défaut 96 ;
   ~~CRUSH~~ **v1.05** (Romain : « inverser + lisser ») : A = CRUSH 0 propre → 127 = 2 bits, pas continu `ins_crush_q` (16..8192,
   exponentiel ; 1/pas calculé 1 fois par bloc), fondu A 0..32 aussi ; presets BITS 85, DESTROY 127 ; scènes USER sauvées avec
   l'ancien BITS : sens inversé, à refaire à la main). **v1.06** : la vraie cause du « on/off » d'AMNT (Romain) = alter_play.c
   `alt_take` : AMNT tourné avec TYPE OFF allume FOLD à fond **depuis l'AMNT restant** (50) → maintenant depuis 0 ; page FX : AMNT
   affiche 0 si TYPE OFF (`tools/alter/alter_insert_click.mjs` : 1er cran = −0,2 dB).
   **v1.07 (Romain)** : page FX = onglets **REVERB DELAY DIST** (`AFP_FXT`) ; DIST = **DRIVE · INS · TYPE · TONE** (TONE = P_IB,
   CRUSH : P_IC ; pas de MIX : MIX < 127 d'une scène → fondu dans INS au 1er geste sur l'insert, alter_play.c `alt_take`) ;
   **MASTER + FILTER déplacés dans GLO tenu** (MIXER : MIX I II III MASTER FILTER, `ALT_MX_GEN` ; même code `afp_ctl` /
   `afp_gen_scene`, tête sur une ligne `afp_hy` 62) ; P_IA défaut 0, libellé « INS » ; page compagnon : MIX d'insert caché
   (GROUPS Effects 12..18). TYPE depuis OFF = INS 0. Les éditions par la page compagnon ne font pas ce fondu MIX→INS. Mesures : `node tools/alter/alter_param_probe.mjs wasm SON "id=v,…" ID v1,v2`
   (P_DIST 33, P_ITYPE 96, P_IA 97, P_IB 98, P_IC 99, P_IMIX 100). **Audit des knobs FAIT (22/11, v1.07)** :
   `tools/alter/alter_knob_audit.mjs wasm OUT.json [sons]` (41 sons × E1..E8 par pas de 4, ~2 min en 2 moitiés) puis
   `alter_knob_report.mjs a.json b.json` ; résultat = onglet **« Audit knobs »** du Sheet (31 lignes, colonne « À corriger ? »
   remplie par Romain) → corriger ce qu'il marque Oui. Mesure = volume/brillance/longueur (pas la hauteur ni la stéréo).
   Pires : RING MATR (trou −15 dB vers 72-80), PHYS BOW (−20 dB dès 0→16), PHYS BRIT/DAMP, PLATE MATR/TONE, NOISE DENS.
   **v1.08 (corrections de l'audit, choix de Romain)** : `eng_curve.h` `alt_curve` (0..10 doux = 4x, puis 40..127) sur
   ALTER NOIS/DRFT, NOISE DENS/DRFT (`eng_noise.c` = notre copie MIT), RESO DIRT ; PHYS EXC = alt_curve³ (×62 max) ;
   PHYS BOW : la frappe/pincée reste sous l'archet (phys_dsp.c), plus de chute ; BRIT/DAMP bas : +6/+12 dB de compensation ;
   PLATE MATR 0..84 seulement + SCRAPE refait, TONE/HIT ; CORDE MATR/BUZZ ; RESO DCAY/RES (niveau) ; ALTER TILT plus linéaire
   (le plus grave ne meurt qu'avec TILT fort à gauche : sinon LENGTH ne prolonge plus, test emu) ; RING : trou MATR comblé,
   **RDIO = radio AM** (bande étroite + sifflement en haut) ; TRIGGER = vélocité du prochain pas. Presets convertis
   (alter_sounds.c). Reste mesuré : RING SIREN 0..52 faible, NOISE DENS sur sons DENS 0, TILT gauche sur THUD léger (1 osc).
   **Rien écouté à l'oreille** : WAV / FM-1 par Romain.
   **v1.09 (Romain : « DIRT ne marche que sur RING BELL », choix « les deux »)** : `eng_dirt.h` = étage DIRT de sortie commun
   RING/PLATE/CORDE (tanh ×1..×26 via alt_curve, make-up 1/(1+(G−1)/14) ; > 90 : grain = bits en moins, échantillons tenus,
   craquements ≤ 4 %) ; le DIRT dans la boucle reste. Mesuré : ±3 dB de 0 à 96, −5..−9 dB à 127 (les queues partent).
   **v1.11 (Romain 22/11 : « trop de doublons dirt/saturation/dst/ins »)** : **RATCH retiré** (lane gardée dans les scènes, à 0 ;
   `alt_rat_draw` vide, dé sans ratchet, lecture `rep` = fills seulement) ; SEQ = **VELO PITCH CHNC DLY REV** (`ALT_LANE_ORD`,
   touches noires 1 3 5 8 10 ; ⚠ `ALT_NTAB` existe déjà = onglets SAVE : nom pris `ALT_NLTAB`) ; **INSERT sans SOFT/HARD/FUZZ**
   (alt_take les saute, une vieille scène garde le sien ; presets WARM/CRUNCH = DIST seul, FUZZ/SCREAM = DIST + FOLD ; page :
   options cachées). **Décidé : le knob DIRT des moteurs sera remplacé par RING MOD ou FOLD selon le moteur**, après écoute :
   page https://claude.ai/artifact/GCqAHt17EWzZaXNypcQbtm (`tools/alter/alter_rmfold_wav.mjs` : 15 sons × sec, RING ×0,5 ×1,5
   ×2,71 700 Hz, FOLD léger/moyen/fort, hors firmware) → **v1.12 (choix de Romain)** : `eng_xform.h` (`ering_*` : ≤ 64 mélange à ×0,5 la note (vmod inc), puis ×0,5 → ×2,71 ;
   `efold_*` : sin(π/2·g·x/131072), g 1..24, make-up `EFOLD_MK` 25 ; PHYS ×2 + make-up 60). **RING** : ALTER (E5, était NOIS :
   plus de bruit d'attaque), CORDE (E3, était DIRT) ; **FOLD** : RING (E3), RESO (E3 : feedback/drive de l'ancien DIRT coupés),
   PHYS (E7, était EXC), NOISE (E6, était DRFT) ; **PLATE garde DIRT**. Presets/sons : ces knobs à 0, LEVEL des sons RESO/RING/
   CORDE remonté (±1 dB d'avant). Perdu : LARSEN/FRICTION ne « chantent » plus (feedback de DIRT), bruit d'attaque de SNAP/GRIND.
   Mesuré : pas de saut, volume ±3..10 dB sur la course ; scènes USER avec ces knobs ≠ 0 sonnent autrement.
   **v1.14 (Romain 23/11 : balance « par familles », « au milieu »)** : LEVEL des 41 sons d'usine réglés en LUFS
   (`tools/alter/alter_balance.py` : devant −25 KICK THUD TOMS GROWL WARM, derrière −31 HAT OPEN HAT RAIN WIND CRUSH METAL
   ARCADE, le reste −28) ; moteur ALTER +6 dB (`voice_amp << 1`, ses sons −6 dB de LEVEL). Reste : CRUSH −6 dB, WARM −2,4,
   BROKEN −3 (LEVEL déjà à 127) → **v1.17 (Romain)** : gain des moteurs NOISE ×2,5 (+8 dB), CORDE ×2, RESO ×2, ALTER ×3 (était ×2),
   PHYS ×3 (était ×2), LEVEL de tous les sons rééquilibrés (3 passes) ; restent courts ≤ 1,7 dB (CRUSH WARM LARSEN SHREDDER : leurs crêtes
   touchent le limiteur MASTER `LIM_T`, plus de gain n'y fait rien) ; scènes USER avec ces moteurs = plus fortes (LEVEL absolu
   sauvé) ; alter_fx_test : COMP et MID +12 mesurés avec 12 dB de marge (op 17 LEVEL −24). Tests relâchés : TRIGGER (traîne), RUMBLE (+2 dB), CONSOLE COMP (crest × 0,95).
   **v1.18 (Romain 23/11)** : REVERB presets = **SHORT BOX HALL BRIGHT HALL CLOUD** (ENDLESS GATED LONG GATE retirés ; SHORT = index 0 =
   démarrage, NEW, scènes d'usine) ; modèle GATED retiré (fx.c : G_RTYPE 1 joue SHORT ; page FX le saute ; page compagnon le cache ;
   params.c G_RTYPE défaut 0 ; **v1.20 : les scènes d'usine gardent leur reverb d'avant = HALL** (Romain : « des saves déjà faites » ;
   alter_store.c `OLD_REV` : rpre = l'ancienne liste) ; SHORT = NEW + sons ; démarrage = scène TECHNO donc HALL ; **+ mix des
   scènes d'usine comme en v1.13** (Romain : « revient sur les effets de HOUSE TECHNO autant que possible ») : `alt_scene_fix` = LEVEL
   par cercle `fix[]` (ALT_SCENE, mesuré cercle par cercle v1.13 vs aujourd'hui, `tools/alter/alter_scene_fix.mjs`, RUMBLE coupé ;
   reste ±0,6 dB) + dans AMBIENT (scène stockée entière) les knobs devenus RING/FOLD remis à 0 ; RUMBLE du KICK = DARK ROOM) ; CHAOS TOTAL : 65 % SHORT, sinon HALL/CLOUD SIZE ≤ 89 ; **sons d'usine secs** (alter_ui.c `alt_pre_fx` :
   fx[] seulement dans une scène d'usine). Reverbs courtes « convolution bizarre » : écoute https://claude.ai/artifact/HaH9apsi9KLoXDBJ8hGAU1
   (`tools/alter/alter_shortverb_wav.py` : CAN PIPE METAL SHARDS vs SHORT BOX) → **v1.19 : Romain garde les 4** : fx.c `rev_odd` (RV_CAN 4..
   RV_SHARDS 7, 44,1 kHz dans rev_comb/rev_ap, état `ro` .pool ~120 o ; SIZE = feedback, DAMP = passe-bas ; `RO_OUT` calé à la
   même sonie que SHORT, limiteur évité : niveaux −12 dB) ; `rev_run` = HALL ou rev_odd ; presets SHORT BOX CAN PIPE METAL SHARDS
   HALL BRIGHT HALL CLOUD ; CHAOS TOTAL : les courtes = SHORT/CAN/PIPE/METAL/SHARDS au hasard. Coût ≈ HALL (wasm, bruité ; FM-1 pas mesuré). CI vert ; **pool libre 8,6 Ko = 444 o au-dessus du seuil CI 8 Ko** (ne plus rien ajouter en .pool sans libérer). Écoute firmware : https://claude.ai/artifact/YPjm6P9y7VqDJdUiwwapGf.
   **RUMBLE (23/11)** : écoute https://claude.ai/artifact/N9BUje7vdzx4TAz4AKEYQF (`tools/alter/alter_rumble_wav.mjs`, hors
   firmware) : actuel 70/127, B DARK ROOM (réverbe ~2,5 s saturée, LP 110 Hz, pompe), C ROLL (sinus à la hauteur du kick
   ~65 Hz, gonfle entre les coups), D GROWL (bruit brun, LP résonant à la hauteur du kick, pompe) → **v1.15 : Romain = DARK ROOM**
   (« la pompe est parfaite », sans le clic d'attaque, knob continu LOW ~40 MID ~80 HIGH 127) : alter_fx.c `rmb_part` refait
   (4 combs 11 kHz ~80 ms fb 0.944 + LP dans la boucle, tanh ×4..×10,4, LP 110 Hz 2 pôles, HP 30 Hz, pompe sur `beat_samples()`
   déclenchée par l'enveloppe du kick > ½ crête, gain lissé ~1,5 ms = plus de clic ; `RMB_GAIN` 24 : 70 ≈ MID, 127 ≈ HIGH +3 dB) ;
   tampon .pool 7,3 Ko (était 3,6 ; à 9,1 Ko le CI refusait : marge pool 6,9 < 8 Ko ; **reste ~8,8 Ko**).
   **Moteur WAVE (23/11, Romain : wavetable indus, « inspire-toi de Shrub / TastyWaves »)** : Shrub = propriétaire (pas de code) ;
   **TastyWaves = GPL-3.0** (github.com/marksweiss/tastywaves, Mark Weiss : `firmware/src/synth.h` oscillateur entier,
   mip levels, Catmull-Rom, BEND ph + A sin ; ses 12 tables = 391 Ko, trop gros pour nous : ~110 Ko de marge image) → nos
   tables maison. Écoute https://claude.ai/artifact/GEnhZSqZQgztEGfF9bNTdT (`tools/alter/alter_wave_wav.py`, 16 images,
   64 harmoniques) : CLANG GRIND ROBOT TEAR × sec/BEND/SYNC/BITS → Romain : **CLANG oui, le reste trop mélodique / sage**.
   Tour 2 https://claude.ai/artifact/WpH1CCp77DC3Nz6NTTFtCu : CLANG + RUST GEAR SHRED STATIC SCREECH FEEDBACK, note martelée,
   × sec / JITTER / SYNC → Romain : **JITTER non** ; « du wave folding et du ring en plus de bouger dans la table ».
   Tour 3 https://claude.ai/artifact/FqS95yySDFK5fxTvpF2Qo5 : les 7 tables × sec / FOLD / RING ×2,71 / FOLD+RING (POS balaie)
   → Romain : **tables CLANG RUST SHRED FEEDBACK** ; WARP façon Serum (MODE + AMNT) ; knobs plan A = TABLE · POS · MODE ·
   AMNT · POS ENV · WARP ENV · SUB · SPRD. Tour 4 https://claude.ai/artifact/4EXmb1BDvTyGQpboz29Wqw
   (`tools/alter/alter_wave_warp.py` : 4 tables × DRY SYNC BEND PWM MIRROR FM RING FOLD BITS, enveloppes POS + WARP)
   → Romain garde **SYNC BEND FM RING FOLD BITS** (PWM MIRROR non). **v1.16 : moteur WAVE codé** (`eng_wave.c`, ENGI_WAVE 5,
   ALT_ENGS index 7 ; tables `eng_wave_tab.h` générées par `tools/alter/alter_wave_tables.py` : 4 × 16 images × mips 256/128/64/32,
   int8 + gain par image, ~31 Ko flash) ; knobs TABL(enum) POS MODE(enum) AMNT PENV WENV SUB SPRD ; SHP → POS, FLT → AMNT ;
   AMNT 0 = aucun warp (**v1.21** : WENV et FLT poussent en proportion d'AMNT, Romain) ; alt_sys[1] = 8 bits (WAVE = 0x80, migration alt_sys[7] = 2) ; sons HAMMER BENDER CORRODE WAVE RING
   SHREDDER HOWL (−28 LUFS). Audit knobs : pas de saut. CI vert ; marge image ~64 Ko (était ~110). Écoute : https://claude.ai/artifact/BvvHvBSGMBZwXTuNWZ6WqW. **Pas écouté, CPU pas mesuré sur la FM-1.** balance des niveaux des sons d'usine, moteurs plus différents
   (écoutes WAV d'abord), **RUMBLE à refaire** (2-3 variantes WAV à écouter). 4. Vu ailleurs (Felucca 1.0, fm1-dock,
   ChoralRoot) : backup complet depuis la page, mise à jour qui garde scènes/réglages, MIDI Bluetooth.
5. ~~THEME SPLIT~~ (v1.10, retiré en **v1.13** : Romain 23/11 « on reste sur notre DA ») ; `alt_sys[11]` peut valoir 1 sur
   la FM-1 de Romain (inutilisé : le remettre à 0 avant de le reprendre). Maquettes gardées : https://claude.ai/artifact/BYCHi6JSFNoxxGFRJ721Pe.

## Utilisateur
- Romain, non-développeur, techno live indus (Analog Rytm pour les drums). Français simple, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Retours de jam = issues/listes : résumer, clarifier, puis coder.
- A sa FM-1 (puce flash UC25HQ80 0xB36014), PC Windows. Installe par https://forban5929.github.io/Alter-FM1/install/
  (secours : OCT− + OCT+ tenus 5 s = UBOOT + kit /secours/ ; n'installer que des firmwares qui connaissent 0xB36014).

## Démarrer une session (≈ 2 min)
1. `add_repo Forban5929/alter-fm1` (push) · `git clone --depth 1` · `git fetch --depth 40 origin etape-3d:etape-3d` ·
   `git checkout etape-3d` · `git submodule update --init --depth 1`. **Travail sur `etape-3d`** ; `main` (vitrine GitHub) :
   **seulement à une Release** (`git push origin etape-3d:main`, avance rapide) — depuis la v1.1, plus à chaque envoi.
2. Construire + tester :
   - `sh tools/alter/wasm/build_emu.sh` (→ `build/emu/`, ~30 s ; `mkdir -p build/alter build/host` avant les cc)
   - `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` · `node tools/alter/alter_fx_test.mjs build/emu/felucca.wasm`
   - `cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
   - `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm` · `node tools/alter/alter_learn_test.mjs build/emu/felucca.wasm`
     · `node tools/alter/alter_smooth_test.mjs build/emu/felucca.wasm` (bandes = redessin complet, coût ON/OFF)
     · `node tools/alter/alter_boot_test.mjs build/emu/felucca.wasm` (démarrage + veille) · `alter_macro_test.mjs` (MACRO) · `node web/emu/emu_test.mjs …` (**4 échecs VOULUS**)
   - 2 cœurs (GitHub le lance) : `cc -std=gnu99 -O1 -w -fsanitize=thread -DFELUCCA_CPU2=1 -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Itests -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 tests/dualcore_test.c -o build/host/dualcore_test -lpthread -lm`
   - `node tools/alter/alter_save_test.mjs build/emu/felucca.wasm` (sauver/rappeler, tirages, PROG) · `cc -O2 -o build/alter/alter_dvm_test tools/alter/alter_dvm_test.c && build/alter/alter_dvm_test`
   - Page compagnon : servir `/tmp/site` (build/emu + web/alter → editor/) par `python3 -m http.server`, piloter Chrome headless
     par CDP (`--remote-debugging-port`, script type /tmp/cdp.mjs à refaire : Runtime.evaluate + Page.captureScreenshot) ;
     pas de sortie son en headless (AudioContext suspendu), getUserMedia à remplacer par un oscillateur.
   - Syntaxe FM-1 : `clang --target=wasm32 -fsyntax-only -std=gnu99 -w -Werror=implicit-function-declaration -Werror=undefined-internal -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 firmware/src/felucca.c`
3. Outils : `alter_render.mjs wasm DIR` (WAV + niveaux de tous les sons), `alter_shot.mjs wasm DIR "w:500 B:GLO+ w:700 B:GLO- s:nom"`
   (captures), `alter_knob_probe.mjs`. Ops de test `web_alter` : liste en tête de `alt_web_cmd` (alter_ui.c) ;
   **34** = copie/reprise de plantage, **17** = un réglage de piste (a = cercle | P_id<<8, 255<<8 = RUMBLE), **18** = RUMBLE coupé (les tests comptent les coups à l'audio), **19** = une PLAGE, **21** LEN, **22** CHAOS TOTAL, **23** SAVE scène a, **24** LOAD (b&1 patt, b&2 NEW), **25** save ancien format, **26** PROG, **27** LFO.
   Peek 13 = empreinte tirage/pas d'un cercle, 14 = treg|tfreeze<<16|dé<<17|prog<<24, 15 = note. `alter_save_test.mjs` (sauver/rappeler), `alter_fxcost.mjs` (coût par effet, bruité).
   `web_alter_peek` : 11 CONSOLE, 12 mute/hits/sel/playing/REC, 100+P un param, 400+i un lien MIDI LEARN ; `web_midi(st,d1,d2)` = MIDI USB.
4. **Version** : `FELUCCA_VERSION` (felucca.c) = « v1.21 » (23/11, BETA) ; **+0.01 à CHAQUE envoi à Romain** (v1.01, v1.02…), à monter
   avant le commit. Affichée : démarrage, SYSTEM > ABOUT (en gros + date/heure de build, UTC), page émulateur, /version.txt.
5. **Deux sites (v1.02, 22/11)** : pousser sur `etape-3d` → pages.yml construit (tools/alter/site_build.sh, 2 fois) :
   **racine = dernière Release** (tag vX.YY le plus haut ; ce que voient les gens) et **/beta/ = etape-3d** (bandeau rouge
   BETA, `site_beta_banner.py` ; tests emu lancés seulement pour la beta). Une Release (firmware.yml) relance pages.yml →
   la racine passe à la nouvelle version. Romain + testeurs : https://forban5929.github.io/Alter-FM1/beta/ (…/beta/editor/,
   …/beta/install/). Version : /version.txt et /beta/version.txt. Le cloud ne télécharge pas les artifacts ni le compilateur JieLi : **rien n'est mesuré sur la FM-1 depuis le cloud.**

## Règles techniques (vérifiées)
- JieLi AC79 pi32v2 : **entiers seulement** (Q15/Q24), pas de float/malloc/math.h ; 44,1 kHz, blocs de 32.
- Un seul fichier compilé `firmware/src/felucca.c` (inclut tout) ; navigateur : `web/emu/felucca_web.c` + `tests/hostsim.c`.
- RAM ~16 Ko libres (gros tampons → `__attribute__((section(".pool")))`, ~19 Ko libres ; CI exige 4 Ko RAM / 8 Ko pool). Sous-modules `firmware/engines`, `firmware/bsp` : ne pas toucher.
- Mise à jour USB : identité **`FM-1_956`** (v1.01 ; v0.79-v1.0 : 929, PRIS par Wanderer sur le Hub ; 900 = Felucca) ;
  le loader dit « ota-FM-1_956 » (firmware/loader/loader.c ; la page d'installation ne compare que « FM-1 » ; le kit de
  secours accepte 956, 929 et 900). make_site.py exige FM-1_9xx. Vérifier un numéro : thefirmwarehub.com/submit/ « Is my identity free? ». Garder « FM-1 » dans le nom USB. GPL-3.0 : garder la page ABOUT.
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
**PLATE 12 (4)** `eng_plate.c`, **CORDE 2 (5)** `eng_corde.c`, **RING 3 (6)** `eng_ring.c`, **WAVE 5 (7)** `eng_wave.c` (NENGINES 15 fixe, libres :
0 1 4 6-8 13 — 13 = alt_sys en flash, ne pas prendre ; 1 = DIGITAL réservé). Knobs 1-4 des nouveaux = MATR SPRD RES DIRT ;
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

## Où on en est (21/11, **v1.0 publiée**)
**Release : pousser un tag `vX.Y` (sur le commit testé) → firmware.yml construit `--release X.Y` et publie la Release
GitHub (paquet, app, SHA256SUMS, licences ; notes = section « ## vX.Y » de docs/RELEASE-NOTES.md).** v1.0 = v0.79 + le texte
de version ; v1.01 = v1.0 + identité FM-1_956 (installée V15 → ALTER, Felucca → ALTER, retour V15 : OK le 21/11 par Romain).
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
Mesure v0.57 (banc, 4 tirages CHAOS TOTAL) : une scène légère 35-55 % sans trous ; les lourdes 65-76 % moy., pics 85-115 %,
L +2..+11 en 8 s, 5-20F. Un cercle seul monte à 45-54 % d'un bloc → **le moteur ALTER** (3 voix × 6 osc × 2 formes).
v0.58 : `alt_render` saute les osc à niveau 0 (hors METAL) et la 2e forme quand frac = 0 : identique au bit (10 tirages),
coût du moteur ÷ 2 (natif). À faire si besoin : même chose pour RING/CORDE/PLATE ; puis JIANT HOTRAM (code chaud en RAM).
**Mesure v0.58 (banc, 6 tirages CHAOS TOTAL + 16/16 + tous effets + filtre) : 0 trou (L), 0 voix effacée (S)** ;
CPU moy. max 56-74 %, pics 82-91 %, 8-24F ; un cercle jusqu'à 37-50 %. Le plus lourd : CORDE ×2 + ALTER (74 %, 8F).
Reste : marge faible (pics ~90) → même nettoyage CORDE/RING/PLATE ; écran (8-24F sous charge) → REFONTE 4.
v0.59 (Romain : « marge son d'abord ») : divisions 64 bits PAR ÉCHANTILLON retirées (logicielles sur pi32v2, alter_divdi.c) :
DIRT de CORDE (1), PLATE (4 lignes), RING (1 + softclip(bias) par bloc), RESO (jusqu'à 8). Valeurs bornées → 32 bits,
identique au bit (24 tirages, 7 moteurs : /tmp/prof/esum.c). Règle : **jamais de `(int64_t) … /` dans une boucle par échantillon** (et v0.75 : une division par une valeur du bloc → `dvm_div`).
Mesures v0.59 sur la FM-1, effets coupés : les 3 cercles seuls ≈ 33-38 % moy., pics ~60 ; MASTER FILTER +13, bus +12..24.
v0.60 : **code du son rangé en tête de flash** (build.py `audio_sections`, app.ld `.text.a.*`, size_fns `isr_reach` suit les
tables ; ALTER_LAYOUT=0 = comme avant), d'après JIANT 0.9.12 : **pas de gain visible** au banc (même coût moteurs).
Banc : `alt.worst(seed)` tire aussi DIST par cercle, DELAY, REVERB, filtre A/B + MORPH qui balaie ; `alt.bench(n, s, seed0)`
(lancer sans await, lire window.br ; `alt.cmd` à envelopper d'un réessai, réponses lentes sous charge). 8 scènes v0.60 :
moy. 37-65 %, **pics 82-105 %**, L 0-5 / 8 s, X (bus) 19-24, P 25-40, F 8-12. Piste suivante : HOTRAM (RAMTEXT : 21 Ko libres).
Coût par effet mesuré au banc (v0.60, 3 cercles THUD 16/16, effets un à un ; base 42 %) : presets DELAY/REVERB ≈ pareils,
DIST +4..10, **MASTER FILTER +17..22 %** (F seul 7-11 : le filtre ralentit tout le reste = cache flash saturé).
**v0.61 (Romain : piste 1) : code chaud du cœur 0 en RAM** (build.py `HOT_FNS` mix_block, mix_part0, afl_block, afl_side0,
am_master = 17,4 Ko ; copies cœur 0 `mix_part0`/`afl_side0`, le cœur 1 garde les copies flash comme JIANT) ; app.ld : RAM à
0x01C00000, POOL 0x01C12000, **RAMTEXT 0x01C6C000 (32 Ko, haut du pool)** pour que les appels (±4 Mio) atteignent la flash :
marge ~110 Ko de croissance d'image (CI vérifie). ALTER_HOTRAM=0 = comme avant. Pile 2e cœur 52 %. **À mesurer sur la FM-1.**
**Mesure v0.61 (banc) : gros gain** : filtre +4..10 % (était +17..22), 8 pires scènes moy. 27-58 %, **pics 47-77 %** (étaient
82-105), 20-27F, **0 trou, 0 voix effacée**. Le goulot était bien le cache de la flash.
**v0.62 : filet de sécurité (REFONTE 3, choix de Romain « écran puis traînes »)** : audio.c `alt_gov_half` (charge de chaque
demi-bloc > 80/88/93 % deux fois de suite → niveau +1, gardé 2 s puis −1) ; fx.c `alt_gov` : 1 écran à 25 img/s (main.c),
2 une note déjà relâchée finie en 1 bloc tous les 4 demi-blocs (`alt_gov_tail`), 3 MASTER FILTER sans DRIVE en 12 dB. Le
shed de Felucca (95 %) reste derrière. G sur la 3e ligne CPU ; DIAG + banc : G, Gup, Gtail (fin de la réponse 30).
Mesure v0.62 (banc, 8 pires scènes) : pics 60-77 %, 0 trou, filet jamais déclenché (G0). Romain : « écran fluide » ensuite.
Écran sous charge : 20-30 img/s, MS 20-50 (dessin + interruptions son). LCD = SPI 12 MHz (lcd.c LCD_BAUD 4), DMA sondé.
v0.63 : mesures écran au banc (DIAG fin : dw di dl kpx loops = ms d'une image, dont son, dont attente LCD ; kpx ; tours de
boucle/s) + test op 30 = diviseur SPI de l'écran à chaud (2..8). Prochaine étape : REFONTE 4 selon ces chiffres.
Mesure v0.63 sous charge : une image ≈ 22 ms = son 9-14 + attente LCD 7-10 + dessin 3-5 ; ~8 kpx/image ; boucle 15-20 ms →
l'ancienne règle « 28 ms depuis la dernière image » tombait 1 tour sur 2 = 25 img/s. **v0.64** : `alt_frame_due` (une image
due toutes les 33 ms, 30/s en moyenne) pour SMOOTH et la vague LFO ; **LCD à 20 MHz** (lcd.c LCD_BAUD 2 ; Romain : écran
propre à 20 MHz, testé à chaud au banc).
Mesure v0.64 (banc, 8 pires scènes) : **25-29 img/s** (étaient 20-27), attente LCD 2-7 ms (7-10), pics 60-79 %, 0 trou, G0.
Reste pour l'écran : le son prend ~10 ms de chaque image (même cœur) ; pistes : moins de pixels par image (kpx 5-9), double
tampon (dessiner pendant l'envoi), dessin sur le 2e cœur (risqué).
Romain : écran assez fluide → **v0.65 : CHAOS / CHAOS TOTAL avec glitch** (choisi sur maquette https://claude.ai/artifact/TcLMHhqFVi9boA77avYUJD :
« glitch, après une mesure entière ») : alter_ui.c `alt_chaos_req` (touches + MIDI LEARN ; la page compagnon reste immédiate),
en lecture le tirage attend la 2e barre de mesure (`acx.at`, fait pendant la dernière double-croche avant le 1 : `acx_poll`),
écran entier en glitch (`acx_frame`/`acx_glitch` ~18 img/s, plus fort à l'approche du 1, + 320 ms sur la nouvelle scène) ;
à l'arrêt : tout de suite ; ré-appui pendant l'attente = annulé ; TOTAL < 3 s après le tirage = annulation comme avant.
Test op 31 (b 0 CHAOS, 1 TOTAL, comme les touches) ; op 22 reste immédiat (banc). Test emu « CHAOS TOTAL playing ».
Bug trouvé en v0.66 : l'attente était vérifiée dans le dessin → `acx_poll` maintenant dans alter_ui_input (à chaque tour).
**v0.66 (Romain) : OCT− + HOME → SYSTEM › RANDOM** (`alt_sys_open_rand`) ; lignes ALL FX, DIST INSERT, DELAY, REVERB
(`alt_sys[9]` = bits LAISSÉS par CHAOS TOTAL, 0 = tout tiré) ; **CHAOS ne tire plus aucun effet** (`alt_fx_keep/alt_fx_back` :
DIST+INSERT, envois DLY/REV/CHOR et couloirs de pas remis) ; TOTAL remet ceux coupés (+ réglages delay/reverb globaux gardés).
Test ops : 31 b2 = CHAOS tout de suite, 32 = alt_sys[9] ; peek 17 = attente CHAOS. Test emu « CHAOS keeps every effect »
(machine à part : les tests d'avant laissent REC/mouvements) ; test TURING passe par op 22.
**v0.67 (Romain : « CHAOS reset tous les FX à zéro »)** : CHAOS met à 0 DIST, INSERT, envois CHORUS/DELAY/REVERB et les
couloirs de pas DELAY/REVERB (`alt_fx_zero`) ; TOTAL inchangé (tire les effets laissés ON dans RANDOM).
**v0.68 (Romain)** : CHAOS respecte les LOCKS du cercle (SOUND = pas de nouveau son ; RHYTHM = type, HITS, ROT, PROB, TURING,
RATCH ; LENGTH ; VELOCITY ; CHANCE ; PITCH = racine + couloir ; THROWS = couloirs de pas DELAY/REVERB gardés) et tire
EUCLID ou TURING (`alt_tur_draw`) ; CHAOS TOTAL ignore toujours les LOCKS. Test op 33 = LOCKS d'un cercle. Test link BLOB :
un cercle TURING comparé sur son type seul (il avance en jouant).
**v0.69 (ménage 1.0, choix de Romain)** : lignes CPU cachées par défaut, **SYSTEM › SETUP › CPU METER** (`alt_sys[10]`,
1 = affichées ; la mesure tourne toujours pour le banc) ; banc gardé (mesure + pilotage, test.html sans lien) ; ABOUT :
« (C) 2026 Forban5929 », « Built on Felucca (C) Leo Kuroshita », crédits ALTER (Basimilus sans lien, écrit avec Claude).
**v0.70 (Romain)** : CPU METER OFF = **rien mesuré** (fx.c `alt_prof` : `APF_ADD`, apf_end ; UI : alt_cpu_poll, alt_fps_tick) ;
restent (utiles au son) : part_tk/fix_tk (partage des cœurs), filet `alt_gov`, shed. **Pour le banc : allumer CPU METER sur la
machine** (sinon DIAG renvoie 0). **Un seul TURING par tirage** (CHAOS : les TURING verrouillés comptent ; TOTAL aussi).
**v0.71 (Romain)** : CHAOS = aucun LFO ni matrice (tous remis à 0), dé lancé avec `AS_DW_NOTHR` (plus de « jets » delay/reverb :
c'étaient eux les barres SENDS de la page FX, via alt_offset pendant les coups). DENSITY − (`as_density`, UI `alt_dens_press`) :
enlève d'abord les « extra » du dé (dhits) puis les pas ajoutés (TRIGGER + REC, `add`), puis HITS ; HITS 0 = silence même avec
le dé (`as_repat`) ; le bouton HITS affiche les coups qui jouent. Page d'installation : la version en grand (« ALTER v0.71 »).
Banc : si le panneau latéral est fermé, ouvrir son propre onglet test.html (tabs_context createIfEmpty) et cliquer Connect
par javascript (permission MIDI déjà accordée au site).
Test ops ajoutés : web_alter 29 = HITS d'un cercle ; peek 16 = voix qui sonnent d'un cercle.
**v0.72 (REFONTE 6, Romain : filet du plantage)** : le repli sur 1 cœur existait déjà (Felucca `FM1_CPU1_FALLBACK` :
plantage avec le 2e cœur actif → 2e cœur barré jusqu'à l'extinction). Ajouté : **copie de la scène jouée toutes les 3 s**
(alter_store.c `alt_crs` en `.noinit`, 1868 o, somme FNV ; pas avant 5 s de jeu ; reprise une seule fois) → au démarrage
après plantage/chien de garde (`boot_clean` de main.c, pas si `fm1_crash.early`) la scène revient (« CRASH: SCENE BACK »,
transport arrêté) ; écran rouge 1,5 s (était 4) + « RESTART, SCENE KEPT » ; ABOUT : bloc **LAST CRASH** (nombre, VEC, PC,
secondes de jeu, chien de garde, 2e cœur coupé). Test op 34 (a 1 = copie, 2 = reprise) ; alter_save_test. **Pas vérifié sur la FM-1.**
**Tests FM-1 v0.72 (11/11, onglet « v0.72 ») : 18/20 OK** (CPU, CHAOS, LOCKS, RANDOM, DENSITY, sauvegardes, extinctions,
SONG, FX, MIXER, page, patch) ; restent 19 (BCR32) et 20 (jam 30 min). **v0.73** : page LOCKS, SELECT ou une touche
CERCLE = un autre cercle (couleur du cercle, sélection suit, sa touche CERCLE allumée) ; peek 18 = LOCKS | lockc<<8.
**v0.74 (Romain, maquette https://claude.ai/artifact/7KF2zSNpU1VBwS3NHZDfKm piste E sans allumage ni cadre)** : animation de
démarrage (alter_ui.c `alt_splash_draw`, 1,5 s : 3 cercles gravés or, aiguille d'or qui révèle couleurs + graduations + ruban
ALTER lettre par lettre, coup de glitch `acx_glitch`, version) ; main.c `alt_boot_splash` (toute touche passe ; après plantage :
image finale direct) ; LEDs à nous (`alt_splash_leds`, `boot_anim` remplace le balayage Felucca) ; draw_splash = fond seul.
**Veille** : ui.c `SCR_SEAL` (SCREEN OFF écoulé → `alt_seal_frame` 1 min, cercles sombres qui respirent et dérivent, 10 img/s,
puis vraiment noir ; une touche réveille, `alt_redraw_all`). Test op 35 = veille ; `tools/alter/alter_boot_test.mjs` ;
emu_test/link_test démarrent 2 s (animation).
**v0.75 (Romain : marge CPU)** : `eng_dvm.h` (division par une valeur fixe du bloc = 1 multiplication 32x32→64, exacte :
`alter_dvm_test.c` vérifie 1,1 milliard de quotients) dans PLATE (4 DIRT + exciteur), CORDE (DIRT + exciteur), RING (DIRT),
RESO (exciteur) ; RING ne calcule plus que les 2 formes d'onde qu'il mélange (−13 % d'instructions natif). Identique au bit
(/tmp/prof/esum.c 24 tirages, engp.c 16 cas). Gain des divisions **à mesurer au banc** (callgrind compte 1 div = 1 instruction).
**v0.76 (Romain : LEARN dans la page)** : LINK 24 LEARN (alter_link.c `lk_learn` ; LINK.md) ; alter_learn.c `alm_page_arm`
/ `alm_page_poll` (attente de la page, 20 s, `alm_pwait`, `alm_last`), `alm_cname`, `alm_valid`, `alm_clear_all` ; page :
onglet **Learn (XXI)** (groupes, Learn / Clear par ligne, Clear every link via dialogue, aide) ; `window.altSim` = le simulateur
de ?demo (tests CDP : `altSim.web_midi(0xB0, 21, 80)`). Testé : alter_link_test (7) + Chrome headless.
**v0.77 (Romain : MACRO, toute la scène, recettes + modifiables, OCT+ tenu + KNOB 1)** : `alter_macro.c` (`amc`) ; 4 lignes
= cibles MIDI LEARN (ligne de cercle ALM_CT × I/II/III/I-III, ou GLOBAL) + quantité −64..63 ; recettes TENSION SPACE DARK
DESTROY ; base = valeur à MACRO 0, réglage tourné à la main = nouvelle place (`w`/`w0`) ; `alm_get` (alter_learn.c) ;
GLOBAL de LEARN « MACRO » (ALG_MACRO, ajouté à la fin) ; gardée dans la scène (`alt_ub.s.mc`, BLOB `mc` à la fin, fichier
ancien accepté `ALT_BLOB_OLD`, copie de plantage) ; CHAOS / TOTAL / NEW / usine = 0 (`amc_zero`). Peek 19 (état), 20-23
(lignes). Test : `tools/alter/alter_macro_test.mjs`. Attention : les REC de mouvements bougent aussi les cibles.
**v0.78 (Romain : PROG + vague LFO dans la page)** : LINK variable de cercle **PROG** (`LV_PROG`, ajoutée à la fin de LK_V,
groupe SCALE : onglet Rhythms) ; page MODULATION : la vague défile vers une barre à gauche, le point sur la barre (`waveDots`,
comme la machine) ; S&H/SLEW acceptent les phases négatives. Vérifié Chrome headless (?demo + op 27).
LOCKS refaits en v0.48 (7 verrous
= les dés : RHYTHM LENGTH VELOCITY CHANCE PITCH THROWS SOUND ; bits 1 2 32 gardés, anciens 4 8 16 64 relus LEN CHANCE SOUND
THROWS) ; UPDATE (OCT− + OCT+ 5 s) seulement s'ils sont tenus SEULS (main.c `combo`, pas testable dans l'émulateur).
Tout ce qui précède est codé et testé **dans l'émulateur / Chrome headless**. Romain a validé sur sa FM-1 : LFO fluide (v0.42),
son USB dans la page (v0.44). **Pas encore vérifié sur la FM-1** : économies CPU (v0.41), tirages sauvés + PROG + LEN, banque C,
fichier patch entier, MIXER de la page, MIDI LEARN (BCR32), TRS MIDI (câble en cause, en pause).

## Feuille de route vers la 1.0 (proposée à Romain le 10/11 ; ordre à confirmer avec lui)
1. **Tests sur la FM-1** (le plus important) : liste FAITE (10/11, 44 tests, Sheet https://claude.ai/artifact/4wMzoCU4XtT5LgPXGqZvES ;
   la relire avant de coder : ses lignes « Problème » = le travail suivant ; **onglet « v0.72 » = session guidée de 20 tests : 19/19 OK** (21/11 : jam 30 min RAS ; test 19 BCR32 abandonné par Romain,
   MIDI LEARN à essayer plus tard avec un autre contrôleur)) ;
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
   `main` rattrapé le 10/11 (avance rapide). 12/11 : notes de version complétées (Live safety, Known limits), README statut
   v0.7x, message à Leo rédigé (Romain l'envoie). RESTE : la Release v1.0 **après** le test d'installation du Hub.
   **The Firmware Hub (21/11, v0.79)** : https://thefirmwarehub.com/submit/ ; identité `FM-1_956` (depuis v1.01) ; `fmhub.json` à la racine
   (kinds groove, image `assets/fmhub.png` **1024×1024** = artwork « Day/Night » (**le Hub n'accepte que du carré** 240..1024, ≤ 300 Ko ; le 1024×512 a été refusé ; Felucca 1200×600 = fiche gérée par le Hub, pas de fmhub.json), source : canvas https://claude.ai/artifact/FZkborqU2gZQoNpSKcL3J4, rendu PNG par Playwright + polices Google Fonts du dépôt google/fonts ; changer l’image = commit sur main puis merge main dans etape-3d ; liens play/editor/guide) ; pas de fichier son dans le dépôt.
   Test d'installation OK (21/11, v0.79) ; Release v1.0 publiée (tag v1.0, identité 929 = prise !) → v1.01 (956) ;
   le tag ne peut pas être poussé d'ici : Romain (ou Claude in Chrome, connecté) crée la Release sur github.com
   (releases/new?tag=vX&target=main), firmware.yml remplit titre, notes et fichiers (`--release` accepte X.YY depuis v1.01).
   **Release v1.01 publiée (22/11, FM-1_956, alter-1.01.fwsc)** ; formulaire du Hub rempli dans Chrome, envoi à faire (email).
   **Miniature du Hub (22/11, Romain : C « le glitch »)** : `assets/fmhub.png` 1024×1024 dessinée (source `tools/alter/hub_card/`) ;
   poussée seule sur `main` avec l'accord de Romain (le Hub lit `main`), puis main fusionné dans etape-3d.
5. **Documentation** : README réécrit pour ALTER + `docs/MANUAL.md` (anglais, 17 sections, d'après guide.html + le code)
   FAITS le 10/11 (v0.47). À tenir à jour avec guide.html. Reste : notes de version ; vidéo/audio de jam (Romain).
6. ~~Ménage~~ **FAIT v0.69** ; filet du plantage **FAIT v0.72** (CPU METER, ABOUT, textes écran déjà en anglais).
7. **Légal / politesse** : GPL + ABOUT déjà OK ; note sur l'aide de l'IA (comme Felucca) ; « inspiré de Basimilus », sans
   laisser croire à un lien avec Noise Engineering ; prévenir Leo Kuroshita (Felucca) avant l'annonce ; Issues GitHub ouvertes.

## Idées (après la 1.0, si Romain le demande)
~~Macro~~ (v0.77) ; ~~LEARN dans la page compagnon~~ (v0.76) ; DENSITY/ROT continus ; ~~PROG + vague LFO dans la page~~ (v0.78) ;
MACRO dans la page compagnon.
