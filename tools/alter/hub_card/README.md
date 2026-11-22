# Miniature The Firmware Hub (22/11, choix de Romain : C « le glitch »)
`card.html?v=a|b|c` (a sceau, b trois cercles, c glitch) dessine la carte en SVG 1024 × 1024 avec les polices de la page
compagnon (copier pagella-bold.woff, intertight.woff, plexmono-bold.woff à côté). Rendu : servir le dossier, puis
`google-chrome --headless=new --window-size=1024,1024 --screenshot=out.png "http://localhost:PORT/card.html?v=c"`, puis
réduire à 128 couleurs (PIL quantize, < 300 Ko exigés par le Hub). Résultat : `assets/fmhub.png` (le Hub le lit sur `main`).
