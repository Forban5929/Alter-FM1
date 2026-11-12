# ALTER LINK — la page compagnon par USB-MIDI

Firmware : `firmware/src/alter_link.c` · page : `web/alter/editor.html` + `web/alter/link.js` (publiée sur `/editor/`) ·
test : `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm`.

Trame : `F0 7D 41 4C cmd args… F7` (« AL » ; l'ancien éditeur Felucca disait « FL », il n'est plus servi). Valeur = 2 octets,
poids faible d'abord, + 8192. Une réponse par requête valide, même `cmd` ; requête fausse = pas de réponse.
Branché dans `main.c` par `lk_usb_service()` : SEUL endroit qui lit une trame reçue ; « AL » = pour nous, sinon `ota_service()`
(mise à jour USB intacte). Avant le 04/11, ota_service pouvait avaler une trame « AL » → pas de réponse → « liaison coupée ».
File de sortie USB : `SXQ` 256 (felucca.c, avant usb.c).

| cmd | requête | réponse |
| --- | --- | --- |
| 1 INFO | — | version (« ALTER LINK 3 »), 3 cercles, 7 moteurs (noms), N sons (moteur, nom), 32 places USER, 22 ids de réglage |
| 2 STATE | cercle | cercle, moteur, son, mode d'enveloppe, 22 valeurs (BASE : sans le décalage du dé) |
| 3 DESC | cercle, id [, moteur] | fmt, min, max, déf, nom, unité, noms d'une liste |
| 4 SET | cercle, id, valeur | cercle, id, valeur gardée (dans ses limites ; le dé reste par-dessus) |
| 5 SOUND | cercle, son | STATE (comme PRESETS sur la machine) |
| 6 SCENE | 0 liste / 1 place [3 car.] = ranger (= SAVE tenu ; avec NOM) / 2 place = charger / 3 liste + NOMS (v3) | op, place, rc (1 ok, 0 vide/protégée, 2 flash), [32 × occupée] ou [32 × (occupée, 3 car. ; 0 = sans nom)] |
| 7 WATCH | 0/1 | 0/1 ; tant qu'on parle dans les 3 s : poussées 8 CHANGED (cercle, id, valeur) et 9 RELOAD (cercle) |
| 10 PING | — | 0 (la page en envoie 1/s) |

Ids (ordre de STATE) : E0..E7, ATK DEC SUS REL, DIST CHOR DLY REV, ITYPE IA IMIX, LEVEL PAN GLIDE = ce qu'une scène USER garde.
Patch fichier : JSON `alter-fm1-patch` v1 (moteur, nom du son, valeurs par id) ; à l'ouverture : SOUND puis SET de chaque id.
Page : lit les DESC des 4 moteurs UNE fois à la connexion ; une réponse perdue = redemandée 1 fois ; coupée après 3 PING
sans aucune réponse pendant 6 s ; un PING après > 3 s de silence relance les poussées (WATCH) ; un RELOAD pendant une lecture = relu après (un seul refresh par cercle).
Mode démo : `editor/?demo` fait tourner `../felucca.wasm` dans la page (exports `web_link_*`, sans son).
**Émulateur** (04/11) : « Connect the emulator » parle à l'émulateur web d'un autre onglet (BroadcastChannel `alter-link`,
`{to:"emu", hello|data}` / `{to:"page", here, on | data}`), relayé par `web/emu/index.html` vers `worklet.js` (`type: "link"`).

## v2 (04/11) — rythmes, couloirs, LFO, matrice, effets partagés, actions, cercles en direct

| cmd | requête | réponse |
| --- | --- | --- |
| 11 VLIST | — | 15 noms de groupes, nb réglages par cercle (38), partagés (44), 6 noms de couloirs, nb d'actions (26) ; **active les poussées v2** |
| 12 VDESC | portée (1 cercle, 0 partagé), i [, cercle] | portée, i, groupe, min, max, nom, mots d'une liste (MATRIX : ceux de la part) |
| 13 VALL | cercle (3 = partagés) | cercle, n, n valeurs (ordre de `LK_V`) |
| 14 VSET | cercle (3), i, valeur | cercle, i, valeur gardée (rythme : au cycle suivant, comme les knobs) |
| 15 LANE | cercle, couloir | cercle, couloir, 16 valeurs |
| 16 LSET | cercle, couloir, pas, valeur | cercle, couloir, pas, valeur gardée (efface la PLAGE de ce pas) |
| 17 ACT | cercle, action `LA_*` | cercle, action, 1 fait / 0 refusé |
| 18 PLAY | — | en marche, cercle choisi, puis par cercle : pas joué, LEN, mute+dé, motif (3 × 7 bits), compteur de notes |
| 19 VCHANGED / 20 LCHANGED | poussés | comme VSET / LANE |

Actions (`LA_*`, = touches de la machine) : MUTE, DÉ (tirage), PROPRE, DENS −/+, SHIFT −/+, TRIGGER, RYTHME (= MUTE 1+2+3),
TOUTES les séquences, TOUT À PLAT, hasard d'un couloir (11..16), à plat d'un couloir (17..22), CHAOS, CHAOS TOTAL (retapé
< 3 s = annulé), CHOISIR, PLAY (26), STOP (27), REC (28 : overdub du cercle choisi, comme REC tapé). PLAY poussé : 1er octet = en marche | REC allumé << 1. Les actions « du cercle choisi » mettent `au.sel` le temps de l'appel puis le remettent.
PLAY poussé ~30/s seulement s'il a changé ; puis ≤ 10 messages par passe de 20 ms. Page : 3 colonnes (cercle SVG + boutons),
CHAOS / CHAOS TOTAL, onglets SOUNDS (TYPE = ALGORITHM dans ENVELOPE) | RHYTHMS (SCALE, TEMPO, rythme, éditeur de couloirs glissé) | MODULATION
(LFO GLOBAL, LFO 1-3, MATRIX) | EFFECTS (DELAY, REVERB, MASTER, FILTER A/B, MORPH). **Page en anglais** (04/11, Romain : « la langue des synth nerds ») ;
noms des groupes et réglages du firmware aussi (LK_GRP, LK_V : RHYTHM SCALE ENVELOPE MATRIX FILTER A/B, FORCE ROT, CHORD, DICE DOSE, TYPE, SCALE).
Look = l'écran de la FM-1 (cartes de tarot, thèmes NIGHT / DAY gardés dans le navigateur, polices dans `web/alter/fonts/`) ; points des pas
= taille selon la vélocité (couloir VELO tourné par FORCE ROT, sans le dé). Firmware sans v2 : VLIST sans réponse →
la page garde les sons et dit d'installer la dernière version. Patch fichier : toujours les 3 sons seulement.

## v3 (05/11) — NOMS, patterns USER, SONG

| cmd | requête | réponse |
| --- | --- | --- |
| 21 PAT | comme SCENE (0 liste, 1 place [3 car.], 2 place, 3 liste + NOMS) | comme SCENE ; un pattern = les rythmes seuls |
| 22 SONG | 0 état / 1 slot = ranger (= SAVE tenu + A B C) / 2 slot = jouer à la mesure suivante / 3 n, n maillons (slot \| mesures << 2 ; 0..3 = 1 2 4 8) = la chaîne (gardée) / 4 on = chaîne ON/OFF (= ARP) | op, rc (1, 0 refusé, 2 flash), slots pleins (bits), slot joué + 1, demandé + 1, chaîne on, maillon joué, mesures restantes, n, n maillons |

NOM = 3 caractères de « espace A-Z 0-9 - » (le reste = espace ; 3 espaces = pas de nom). La page : liste « A05  KCK », champ NAME à côté
de Store here (scènes et patterns), onglet SONG (3 cartes A B C : PLAY / STORE WHAT PLAYS ; la chaîne : maillons slot + mesures,
+ LINK, ✕, CHAIN ON/OFF, « LINK 1/2 · BAR 2/4 »), interrogé toutes les 500 ms quand l'onglet est ouvert. Firmware sans v3 : SCENE 3
sans réponse → la page reste en v2 (pas de NAME, ni patterns, ni SONG). **Guide des touches** : `web/alter/guide.html` (publié
`/editor/guide.html`, lien « Controls guide ») — dessins du panneau et des touches, gestes ; à tenir à jour avec CLAUDE.md.

## v4 (07/11) — CONSOLE

INFO dit « ALTER LINK 4 ». Groupe 15 **CONSOLE** ajouté À LA FIN de `LK_GRP` ; 8 réglages par cercle ajoutés à la fin de `LK_V`
(les index d'avant ne bougent pas) : DRIVE (= P_DIST du son), LOW MID HIGH COMP (= `acs_v`, alter_console.c), DELAY REVERB
LEVEL (= P_DLY P_REV P_LEVEL). `LV_CONS` / `LV_PAR`. Poussés comme les autres (VCHANGED). Page : onglet EFFECTS, une colonne par
cercle en haut (LOW MID HIGH affichés en dB, 0 = KILL). Firmware v3 : pas de groupe 15 → pas de colonnes (`pl.cols` filtré).
Test : `alter_link_test.mjs` (5 tests CONSOLE).

## v4 + RUMBLE (08/11)

Toujours « ALTER LINK 4 » (ajouts seulement, à la fin). Groupe 16 **RUMBLE** à la fin de `LK_GRP`, 1 réglage par cercle à la
fin de `LK_V` (`LV_RMB` = `alt_rmb[c]`, 0..127, poussé par VCHANGED). INFO : après les ids, 1 octet par son = il a un RUMBLE (1)
ou non (0) (`ALT_PRE[k].rumble`). Page : onglet SOUNDS, une ligne RUMBLE en bas du groupe Engine, seulement pour ces sons
(comme le knob 4 du KICK sur la FM-1). Firmware plus ancien : octets absents = 0 → pas de ligne.

## 10/11 — patterns retirés, banque C, BLOB (fichier = scène entière), MIXER

- **21 PAT** : plus de réponse (les patterns USER sont retirés ; les variations = slots SONG A B C). INFO dit 48 places USER
  (banques A B C ; C = objets 32..39, 0xCC000..0xDBFFF). Liste SCENE : 48 entrées.
- **23 BLOB** (`lk_blob`, `alt_blob_t` = `alt_us_t` + NOM + CONSOLE + tirages, 1848 octets au 10/11) : octets 8 bits envoyés
  7 par 8 (1 octet des bits forts puis 7 octets de 7 bits ; `pack7`/`unpack7` de link.js). Dans une REQUÊTE, une valeur = ses
  14 bits tels quels ; dans une RÉPONSE, comme partout (+ 8192).
  `0` = photo de ce qui joue (comme SAVE tenu : alignements repliés dans LEVEL) → 0, taille, somme (13 bits, `blobSum`) ·
  `1 off` → 1, off, n, n octets (≤ 256) · `2 off data` (≤ 49 octets : la machine lit 64 octets d'une trame) → 2, off, n ·
  `3 taille somme` → 3, ok (joué comme une scène USER chargée ; refusé si taille / somme / `used` faux).
  Fichier patch **v2** : les 3 sons (lisibles, et pour un firmware sans BLOB) + `scene` (base64) + `sceneSize` + `name`.
  À l'ouverture : BLOB si le firmware le prend, sinon les sons ; puis la liste USER va sur la 1re case vide (+ le NOM du fichier).
  Une scène d'une autre version du firmware (taille différente) = refusée → les sons seuls.
- **MIXER** (groupe 17 `GR_MIX`, réglage partagé « MASTER » = `alt_mgain`, 100 = 0 dB, gardé dans `alt_sys[5]`) ; page :
  onglet MIXER = par cercle la CONSOLE + MUTE + fader LEVEL, et le fader MASTER ; la CONSOLE quitte EFFECTS.

## v0.76 (12/11) — MIDI LEARN depuis la page

| cmd | requête (octets tels quels) | réponse |
| --- | --- | --- |
| 24 LEARN | `0 g` (0..2 un cercle, 3 GLOBAL, 4 ACTIONS) | 0, g, n, code attendu + 1 (14 bits), dernier (1 appris, 2 refusé, 3 rien en 20 s), liens utilisés, puis n lignes : code (14 bits), lié, numéro, note, canal 1..16, nom |
| | `1 lo hi` = attendre un contrôle pour ce code (le prochain CC / note entendu par la FM-1, USB ou TRS) | 1, code, ok |
| | `2 lo hi` = effacer le lien de ce code · `3` = arrêter l'attente · `4` = tout effacer | 2, code, ok · 3 · 4 |
Codes = ceux d'`alter_learn.c` (cercle c * 32 + t, GLOBAL 96 + g, action 128 + a). Page : onglet LEARN (XXI), sondé 400 ms
quand il est ouvert ; firmware sans LEARN = pas d'onglet. Test : `alter_link_test.mjs` (7 tests LEARN).

## Banc de test (11/11, v0.55) — versions de test seulement
| cmd | requête | réponse |
| --- | --- | --- |
| 30 DIAG | — | version (chaîne), CPU moy, pic, images/s, ms/image, S, L (valeurs), 2C, P W X F M C, T, split, 4 × % d'une partie, fixe cœur 0, fixe cœur 1, 3 moteurs |
| 31 TEST | op, a (5 × 7 bits), b (5 × 7 bits) | op — exécute `alt_web_cmd(op, a, b)` (les ops des tests) sur la machine |
| 32 PEEK | c, w (5 × 7 bits chacun) | `alt_peek(c, w)` en 5 × 7 bits |
Page : `web/alter/test.html` (publiée `/editor/test.html`), objet `window.alt`.
