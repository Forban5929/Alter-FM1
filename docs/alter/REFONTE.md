# ALTER 1.0 — plan de refonte « robuste en live » (11/11, proposé à Romain, à valider étape par étape)

## Constat (faits)
- FM-1 v0.53, scène « un peu tout » : P39 W0 X12 F8 M8 T5 → ~72 % en moyenne sur le cœur 0, S et L > 500 qui montent, 9F 100MS.
- Natif (/tmp/prof/spike.c, coût par demi-bloc de 128) : le pire demi-bloc coûte **×1,3 à ×1,6 la moyenne**. 72 % × 1,5 > 100 % → trous.
  Règle qui en sort : **≤ 55 % de moyenne par cœur**, sinon ça craque.
- Répartition 2 cœurs (fx.c `mix_split`) : n'équilibre que les cercles entre eux ; ignore le travail fixe du cœur 0 (FX 12 +
  filtre 8 + master 8 + TIMER5 5 = 33 %) ; `ENG_VCOST` vaut 0 pour PLATE (12), CORDE (2), RING (3). D'où cœur 0 ≈ 72 %, cœur 1 ≈ 25-30 %.
- L'écran tourne sur le cœur 0 avec ce qui reste (28 %) → 9 images/s.
- JIANT (fork de Felucca, 6 pistes, 11 moteurs) : profil mesuré sur la puce par section et par cœur (prof.c ; les comptes
  d'instructions sur PC ne voient pas le cache flash), code le plus chaud en RAM (FELUCCA_HOTRAM), retour automatique à 1 cœur
  après un plantage + journal du crash dans ABOUT (cpu_safe.c), option 320 MHz. Felucca : 2e cœur OFF par défaut.

## Architecture visée
1. **Budget mesuré, pas estimé** : chaque cercle chronométré (TIMER4) sur le cœur où il tourne ; la répartition minimise
   max(cœur 0 = ses cercles + FX + filtre + master + TIMER5, cœur 1 = ses cercles + côté droit du filtre). Attendu ≈ 45 / 45.
2. **Cœur 1 = le son, cœur 0 = le jeu** : dès que possible le cœur 1 prend 2 cercles sur 3 (puis les bus FX avec un bloc de
   retard, 0,7 ms, inaudible) ; le cœur 0 garde séquenceur, master, touches, écran.
3. **Jamais de voix effacée, jamais de trou : dégradation par paliers** quand un cœur dépasse 80 % : (a) écran à demi-cadence,
   (b) reverb allégée, (c) filtre en 12 dB, (d) en dernier une traîne coupée. Retour automatique quand la charge redescend.
4. **Écran à budget fixe** : redessin par zones (seulement ce qui a changé), plafond de temps par image, cible 30 img/s ;
   le son passe toujours avant.
5. **Élagage** : sortir de la boucle son ce qu'ALTER n'utilise pas de Felucca (4e partie, slicer, UNISON/SPREAD, perform,
   budget de 8 voix) ; 2 voix par cercle au plus. Moins de code = moins de pics.
6. **Filet de sécurité live** (comme JIANT) : plantage ou 2e cœur muet → redémarrage en 1 cœur, scène gardée, raison dans ABOUT.
7. **Test « pire scène » dans le CI** : CHAOS TOTAL + 16/16 + tous les effets ; moyenne et pire demi-bloc comparés à un
   budget ; une version qui dépasse est refusée.
8. MASTER FILTER : la version simple (8 %) reste ; retirable si le budget l'exige.

## Ordre
- v0.54 : points 1 + début du 2 (répartition mesurée) — le plus gros gain, son identique. Mesure FM-1 par Romain.
- v0.55 : point 3 (paliers) + point 6. v0.56 : point 4 (écran). Puis 5 et 7, puis ménage et 1.0.

## Journal
- 11/11 v0.54 : points 1 + 2 codés (répartition mesurée). Dualcore TSAN identique, y compris tout sur le cœur 1. En attente de la mesure FM-1.
