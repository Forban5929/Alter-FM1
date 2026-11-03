# ALTER LINK — la page compagnon par USB-MIDI

Firmware : `firmware/src/alter_link.c` · page : `web/alter/editor.html` + `web/alter/link.js` (publiée sur `/editor/`) ·
test : `node tools/alter/alter_link_test.mjs build/emu/felucca.wasm`.

Trame : `F0 7D 41 4C cmd args… F7` (« AL » ; l'ancien éditeur Felucca disait « FL », il n'est plus servi). Valeur = 2 octets,
poids faible d'abord, + 8192. Une réponse par requête valide, même `cmd` ; requête fausse = pas de réponse.
Branché dans `main.c` avant `ota_service()` : une trame qui n'est pas « AL » reste pour la mise à jour USB (intacte).

| cmd | requête | réponse |
| --- | --- | --- |
| 1 INFO | — | version, 3 cercles, 4 moteurs (noms), N sons (moteur, nom), 16 places USER, 22 ids de réglage |
| 2 STATE | cercle | cercle, moteur, son, mode d'enveloppe, 22 valeurs (BASE : sans le décalage du dé) |
| 3 DESC | cercle, id | fmt, min, max, déf, nom, unité, noms d'une liste |
| 4 SET | cercle, id, valeur | cercle, id, valeur gardée (dans ses limites ; le dé reste par-dessus) |
| 5 SOUND | cercle, son | STATE (comme PRESETS sur la machine) |
| 6 SCENE | 0 liste / 1 place = ranger (= SAVE tenu) / 2 place = charger | op, place, rc (1 ok, 0 vide/protégée, 2 flash), [16 octets occupée] |
| 7 WATCH | 0/1 | 0/1 ; tant qu'on parle dans les 3 s : poussées 8 CHANGED (cercle, id, valeur) et 9 RELOAD (cercle) |
| 10 PING | — | 0 (la page en envoie 1/s) |

Ids (ordre de STATE) : E0..E7, ATK DEC SUS REL, DIST CHOR DLY REV, ITYPE IA IMIX, LEVEL PAN GLIDE = ce qu'une scène USER garde.
Patch fichier : JSON `alter-fm1-patch` v1 (moteur, nom du son, valeurs par id) ; à l'ouverture : SOUND puis SET de chaque id.
Mode démo : `editor/?demo` fait tourner `../felucca.wasm` dans la page (exports `web_link_*`, sans son).
