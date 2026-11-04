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
| 1 INFO | — | version, 3 cercles, 4 moteurs (noms), N sons (moteur, nom), 16 places USER, 22 ids de réglage |
| 2 STATE | cercle | cercle, moteur, son, mode d'enveloppe, 22 valeurs (BASE : sans le décalage du dé) |
| 3 DESC | cercle, id [, moteur] | fmt, min, max, déf, nom, unité, noms d'une liste |
| 4 SET | cercle, id, valeur | cercle, id, valeur gardée (dans ses limites ; le dé reste par-dessus) |
| 5 SOUND | cercle, son | STATE (comme PRESETS sur la machine) |
| 6 SCENE | 0 liste / 1 place = ranger (= SAVE tenu) / 2 place = charger | op, place, rc (1 ok, 0 vide/protégée, 2 flash), [16 octets occupée] |
| 7 WATCH | 0/1 | 0/1 ; tant qu'on parle dans les 3 s : poussées 8 CHANGED (cercle, id, valeur) et 9 RELOAD (cercle) |
| 10 PING | — | 0 (la page en envoie 1/s) |

Ids (ordre de STATE) : E0..E7, ATK DEC SUS REL, DIST CHOR DLY REV, ITYPE IA IMIX, LEVEL PAN GLIDE = ce qu'une scène USER garde.
Patch fichier : JSON `alter-fm1-patch` v1 (moteur, nom du son, valeurs par id) ; à l'ouverture : SOUND puis SET de chaque id.
Page : lit les DESC des 4 moteurs UNE fois à la connexion ; une réponse perdue = redemandée 1 fois ; coupée après 3 PING
sans aucune réponse pendant 6 s ; un PING après > 3 s de silence relance les poussées (WATCH) ; un RELOAD pendant une lecture = relu après (un seul refresh par cercle).
Mode démo : `editor/?demo` fait tourner `../felucca.wasm` dans la page (exports `web_link_*`, sans son).

## v2 (04/11) — rythmes, couloirs, LFO, matrice, effets partagés, actions, cercles en direct

| cmd | requête | réponse |
| --- | --- | --- |
| 11 VLIST | — | 15 noms de groupes, nb réglages par cercle (38), partagés (44), 6 noms de couloirs, nb d'actions (26) ; **active les poussées v2** |
| 12 VDESC | portée (1 cercle, 0 partagé), i [, cercle] | portée, i, groupe, min, max, nom, mots d'une liste (MATRICE : ceux de la part) |
| 13 VALL | cercle (3 = partagés) | cercle, n, n valeurs (ordre de `LK_V`) |
| 14 VSET | cercle (3), i, valeur | cercle, i, valeur gardée (rythme : au cycle suivant, comme les knobs) |
| 15 LANE | cercle, couloir | cercle, couloir, 16 valeurs |
| 16 LSET | cercle, couloir, pas, valeur | cercle, couloir, pas, valeur gardée (efface la PLAGE de ce pas) |
| 17 ACT | cercle, action `LA_*` | cercle, action, 1 fait / 0 refusé |
| 18 PLAY | — | en marche, cercle choisi, puis par cercle : pas joué, LEN, mute+dé, motif (3 × 7 bits), compteur de notes |
| 19 VCHANGED / 20 LCHANGED | poussés | comme VSET / LANE |

Actions (`LA_*`, = touches de la machine) : MUTE, DÉ (tirage), PROPRE, DENS −/+, SHIFT −/+, TRIGGER, RYTHME (= MUTE 1+2+3),
TOUTES les séquences, TOUT À PLAT, hasard d'un couloir (11..16), à plat d'un couloir (17..22), CHAOS, CHAOS TOTAL (retapé
< 3 s = annulé), CHOISIR. Les actions « du cercle choisi » mettent `au.sel` le temps de l'appel puis le remettent.
PLAY poussé ~30/s seulement s'il a changé ; puis ≤ 10 messages par passe de 20 ms. Page : 3 colonnes (cercle SVG + boutons),
CHAOS / CHAOS TOTAL, onglets SONS (+ ALGORITHM) | RYTHMES (GAMME, TEMPO, rythme, éditeur de couloirs glissé) | MODULATION
(LFO GLOBAL, LFO 1-3, MATRICE) | EFFETS (DELAY, REVERB, MASTER, FILTRE A/B, MORPH). Firmware sans v2 : VLIST sans réponse →
la page garde les sons et dit d'installer la dernière version. Patch fichier : toujours les 3 sons seulement.
