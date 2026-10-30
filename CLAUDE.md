# CLAUDE.md — Firmware « ALTER » (fork de Felucca) pour M-VAVE FM-1

## Utilisateur
- Romain, non-développeur, techno live indus. Explications simples, honnêteté (vérifié / pas vérifié).
- Choix de design/direction → AskUserQuestion AVANT de coder. Garder ce fichier court et à jour.
- **N'a pas encore de FM-1** : cible = version navigateur (émulateur Felucca), qui doit rester compatible FM-1.

## Rôle de Claude
Ingénieur DSP + développeur C embarqué.

## Vision (validée le 30/10/2026)
**Un instrument, pas une groovebox.** UN seul son à la fois, riche et complexe, joué par un séquenceur
euclidien orienté live. En live : variations subtiles (rythme, ghost notes, motif), jamais de rupture.
En sound design : le hasard peut être radical. Influences : Basimilus Iteritas Alter, Plaits (Mutable),
Collision (Ableton), Euclidean Circles v2 (vpme.de), Sting 2 (M4L). Drums externes : Analog Rytm (sync MIDI clock).

### Son
- Modèles façon Plaits : chaque modèle a 4 macros communes (les 4 boutons du FM-1).
- Ordre : 1) **ALTER** (Basimilus, spec ci-dessous) ; plus tard COLLIDE (mailloche+résonateurs),
  STRING (corde inharmonique), PARTICLE/NOISE.
- 2 voix internes : la queue du coup précédent continue sous le nouveau.
- Effets : ceux de Felucca (DIST, INSERT FOLD, delay, reverb). Enveloppe : ADSR Felucca.

### Séquenceur
- **3 cercles euclidiens** jouant le même son : **HITS**, **GHOST** (doux), **ACCENT** (fort).
  Par cercle : longueur, coups, rotation (longueurs différentes = polyrythmie).
- Changement de rythme appliqué **au début du cycle suivant** (pas de rupture).
- **Hasard (le dé)** : mode optionnel par cercle. Un appui = nouveau tirage, FIGÉ jusqu'au prochain appui.
  Peut toucher : hauteur (dans une gamme), accents/intensité, glide/slide, timbre (macros)… tout,
  avec un **verrou par paramètre** pour l'exclure du tirage.

### Interface
- Vue **PLAY** (live) : 4 macros, cercles animés, dé, mutes/fill ; macros assignables par preset.
- Vue **DEEP** (sound design) : tous les réglages.
- Minimaliste, coloré, une couleur par cercle.

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
  Tests : `node web/emu/emu_test.mjs build/emu/felucca.wasm` (Felucca) et
  `node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav` (PLAY → les 3 cercles jouent ALTER).
  Clavier de la page : touche `-` = PLAY ; `1`..`=` = FX…REC.

## Code — dépôt https://github.com/Forban5929/alter-fm1 (branche `main`)
Source de vérité = le dépôt GitHub (copies de secours dans le Projet, `claude/code/`).
Démarrer une session : attacher `Forban5929/alter-fm1` (add_repo, push), cloner, `git submodule update --init`.
- `firmware/src/eng_alter.c` : le moteur, inscrit en DERNIER dans `engines.c` (`ENGI_ALTER` = NENGINES-1 dans core.h,
  1er dans ENGINE_ORDER, partie 1 au démarrage = ALTER THUD ; ENG_VCOST 330 ; MIDI CUT=HARM RES=SPRD ; pas de favoris).
- `firmware/src/alter_play.c` (inclus en tête de seq.c, `alter_tick` appelé après click_tick) : relie alter_seq au
  transport Felucca (PLAY, tempo, horloge MIDI) sur la partie 1 ; note d'un pas ; GLIDE seulement si le dé l'a tiré ;
  offsets de timbre annulés exactement au pas suivant (les boutons restent la base).
- `tools/alter/alter_wav.c` : programme PC, rend 15 extraits WAV + compare le coût à ANALOG.
  Recréer : `python3 firmware/engines/tools/gen_engine_tables.py build/alter/fe_tables.h`
  puis `cc -std=c99 -O2 -I build/alter -o build/alter/alter_wav tools/alter/alter_wav.c -lm && build/alter/alter_wav build/alter/wav`
  (son ADSR est une approximation, pas celui de `voice.c`).
- Choix techniques ALTER : état des 2 voix dans `ENG_MEM` (slot par voix), ratios SPRD harmonique → cymbale 808 →
  barre libre, oscillateurs tous en phase au coup, normalisation √(nb d'oscillateurs) + `soft_knee(20000)`,
  64 bits seulement 1×/bloc (incréments). Presets : THUD, BELL, CLANG, SNAP, STAB, RUST.
- Mesures (PC x86) : ALTER ≈ 1,7 à 2,3 × ANALOG par voix → estimé ~4-5 % CPU FM-1 par voix (≈10 % pour 2).
  NON vérifié sur pi32v2 (toolchain JieLi pas encore installée). Crêtes -2 à -5 dBFS, pas d'écrêtage, DC ≈ 0.
- `firmware/src/alter_seq.c` : le séquenceur (étape 2), C pur, sans dépendance, appelé 1×/double-croche
  (`as_init`, `as_set` = réglage appliqué au cycle suivant du cercle, `as_roll` = dé, `as_step` → `as_event_t`).
  ACCENT ne joue jamais seul ; GHOST seulement là où HITS est vide ; PROB 0..127 ; 32 pas max ; DOSE par cercle ;
  verrous AS_LK_PITCH/VEL/GLIDE/MRPH/SPRD/HARM/AMNT ; gammes OCT 5TH MPEN MIN PHRY DOR CHR. Le dé d'ACCENT
  ne bouge que la force de l'accent. Offsets de timbre appliqués aux paramètres au déclenchement.
- `tools/alter/alter_seq_test.c` : 9 tests (motifs, cycle suivant, dé figé, verrous, PROB, polyrythmie).
  `cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test`
- `tools/alter/alter_seq_demo.c` : 8 démos du séquenceur (inclut alter_wav.c avec ALTER_HOST_NO_MAIN) + `rings.txt`.
- `tools/alter/make_listen_page.py build/alter` : page d'écoute HTML (MP3 via ffmpeg depuis wav/ dans mp3/).
- Page d'écoute publiée (23 extraits) : https://claude.ai/artifact/BqNkjf2dYiPQFnkgco6zey

## État d'avancement
- [x] Dépôts vérifiés, vision et design validés
- [x] Étape 1 : moteur ALTER en C + extraits audio (validé : « bon début »)
- [x] Étape 2 : séquenceur 3 cercles + dé (validé par Romain, fusionné dans main)
- [ ] Étape 3 (choix validés 30/10 : vues SUR L'ÉCRAN FM-1 240×240, ALTER REMPLACE Felucca (comme SLOOP),
      cercles CONCENTRIQUES HITS dehors → ACCENT dedans, livraison par morceaux) :
  - [x] 3a : ALTER + séquenceur jouables dans le navigateur (branche `etape-3a-navigateur`, attend l'écoute de Romain).
        Page : https://claude.ai/artifact/JpKofrvF7kb17ZmrJZmx8X (republier `build/emu/alter.html` avec ses fichiers).
        Encore l'interface Felucca ; réglages des cercles = valeurs par défaut (4/16, 3/16 rot 2, 2/16), pas de dé.
  - [ ] 3b : vue PLAY. **Règle de Romain : une touche fait ce que dit son nom** (ne jamais détourner un libellé).
        Mapping VALIDÉ (30/10) : HOME = vue PLAY ; EDIT = vue DEEP ; SEQ TENU = KNOB 1-4 → LEN HITS ROT PROB du
        cercle choisi (lâché = macros MRPH SPRD HARM MODE) ; SELECT = choix du cercle ; SCL = gamme + note de base ;
        ENV = enveloppe ; LFO = modulations ; FX = effets ; GLO = tempo / sync MIDI ; SAVE = preset ; PLAY = start/stop ;
        ARP, REC = rien pour l'instant. CLAVIER = zone live : 3 touches MUTE (HITS GHOST ACCENT), 3 touches DÉ,
        le reste = FILLS / VARIATIONS (à définir). LED des touches blanches (pas de couleur) → couleurs sur l'écran.
  - [ ] 3c : vue DEEP. Puis retirer l'interface Felucca.
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
