# CLAUDE.md — Firmware « ALTER » pour M-VAVE FM-1 (né d'un fork de Felucca)

## Utilisateur
- Romain, non-développeur, techno live indus. Explications simples, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Garder ce fichier court : le détail va dans
  `docs/alter/HISTORIQUE.md` (pourquoi et noms de fonctions de chaque étape).
- **A reçu sa FM-1 (03/11)**, PC Windows. Sait flasher ; utilise https://thefirmwarehub.com/ (sauvegarde, « Back to official
  V15 », secours = tenir OCT− à l'allumage, Rescue box). Le Hub n'installe pas un firmware perso non testé → notre page.

## Étape FLASH (03/11)
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
  Plus tard : enveloppes des modes PERC/STAB dans la page, patch fichier = scène entière ?, assistant d'installation façon baudgirl.
  (L'ancien éditeur Felucca, protocole « FL », est dans l'historique git, commit 8b8f548^ : web/EDITOR_PROTOCOL.md.)

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
- **Moteurs : ALTER (14), PHYS (9), NOISE (11), RESO (10).** `NENGINES` = 15 fixe dans core.h : les autres numéros sont
  des cases vides muettes (`ENG_GONE`, engines.c) pour ne pas décaler les numéros. Partie 4 : silencieuse.
- **Mémoire flash** (`storage.c`) : objet 0 = réglages (`alter_persist.c`) · objets 2, 3, 4 = banques ALTER
  (`alter_store.c` : rythmes des scènes USER, scènes, patterns + enveloppes) · objet 1 = MASTER FILTER + coups TRIGGER des scènes USER ·
  **objets 5.. LIBRES** (ex-presets, banque FM6, autosave Felucca) → place pour les PLAGES, les REC, plus de slots…
- **Mise à jour USB** (M-UPGRADE, `bsp/chip/ota.c`, `ota_hw.c`) : INTACTE. Ne pas changer l'identité `FM-1_900`
  (build.py PRODUCT) ni retirer « FM-1 » du nom USB (« ALTER FM-1 » : les outils de mise à jour cherchent /fm-1|felucca/).
  Le chargeur de mise à jour (bsp) s'annonce encore « Felucca Update » : on n'y touche pas (risque de brique).

## Carte du code ALTER (`firmware/src/alter_*`)
- `alter_seq.c` séquenceur pur (3 cercles EUCLID/TURING, dé, couloirs, plages, accords, gammes) · `alter_play.c` côté audio
  (inclus en tête de seq.c : cercle i → partie i, fills, modes PERC/STAB/DRONE, file UI→audio `alt_q`)
- `alter_ui.c` vue PLAY + pages rapides/menus (inclus dans ui_input.c) · `alter_look.c` style tarot · `alter_fxpage.c` page FX
- `alter_fx.c` presets FX + MASTER (inclus dans fx.c ; inclut `alter_filter.c` = MASTER FILTER) · `alter_lfo.c` LFO + enveloppes par mode (inclus dans mod.c)
- `alter_sounds.c` 23 sons + 4 scènes d'usine · `alter_store.c` SAVE/LOAD · `alter_user_ambient.h` scène AMBIENT de Romain
- `alter_persist.c` réglages en flash · `eng_alter.c` / `eng_reso.c` les moteurs · `alter_legacy.c` **restes** (voir ci-dessous)
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

## GitHub
- Compte **Forban5929**, dépôt public **alter-fm1** (renommé Alter-FM1, liens redirigés). Copies de secours dans le Projet
  (`claude/code/`). Une branche par étape ; issues = retours d'écoute. La session cloud ne peut pas créer de dépôt :
  add_repo puis git push. Avant de pousser depuis un clone superficiel : `git fetch origin <branche>`.
