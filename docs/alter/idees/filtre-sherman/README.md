# Idée gardée : le MASTER FILTER « Sherman » (ALTER v0.36 → v0.50)

Le premier MASTER FILTER d'ALTER, d'après un Sherman Filterbank / Rodec Restyler : DRIVE asymétrique + clip dur,
SVF TPT en Q24 avec une résonance qui **s'auto-oscille** (« scream ») tenue par une tanh dont le plafond baisse avec RES,
pente 24 dB, LP → BP → HP en fondu, suiveur d'enveloppe (MOD, MODTYP), FM audio toutes les 8 échantillons, DETUNE stéréo,
MIX en puissance constante, deux instantanés A / B et MORPH.

**Pourquoi il est retiré (10/11, Romain)** : trop cher pour la FM-1 — environ 21 % du calcul du son (mesure callgrind,
copie native du firmware), ~250 opérations par côté et par échantillon (deux tanh, deux étages, trois bandes, coefficients
toutes les 8 échantillons). Remplacé par un filtre simple façon Felucca qui garde les mêmes réglages et le MORPH A / B.

**À reprendre** pour un autre synthé plus puissant, ou plus tard sur le 2e cœur. Le fichier est la version v0.50 complète
(le côté droit déjà séparé dans `afl_side` pour le 2e cœur). Il s'incluait dans `alter_fx.c` ; `fx.c mix_block` appelait
`afl_block(mix_l, mix_r, wet, n)` avant le MASTER. Tag git : `idee-filtre-sherman`.
