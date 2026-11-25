#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Construit UN site ALTER complet (émulateur, page compagnon, page d'installation, kit de secours) depuis un arbre
# de sources donné. Appelé deux fois par pages.yml (v1.1, 22/11) :
#   public = la dernière Release (tag vX.YY)  → racine du site
#   beta   = la branche de travail etape-3d   → /beta/ (bandeau BETA sur chaque page)
# Usage : site_build.sh SRC OUT public|beta REF RESCUE_ZIP
# Le script vit dans la branche de travail mais s'applique aussi aux anciens tags : il ne copie que ce qui existe.
set -e
SRC=$(cd "$1" && pwd); OUT="$2"; KIND="$3"; REF="$4"; ZIP="$5"
HERE=$(cd "$(dirname "$0")" && pwd)
ZIP=$(cd "$(dirname "$ZIP")" && pwd)/$(basename "$ZIP")
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
cd "$SRC"
VER=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)
# (25/11) le site public montre le numéro de la Release (tag v1.2), pas celui de la dernière BETA du code (v1.29…) :
# cet arbre est une copie jetable, on y réécrit FELUCCA_VERSION avant de construire (émulateur, firmware, installation).
if [ "$KIND" = public ] && [ -n "$REF" ] && [ "$REF" != "$VER" ]; then
  sed -i "s/^#define FELUCCA_VERSION \"[^\"]*\"/#define FELUCCA_VERSION \"$REF\"/" firmware/src/felucca.c
  VER="$REF"
fi
SHA=$(git rev-parse --short=7 HEAD)
echo "== site $KIND : $VER ($REF @ $SHA)"

# 1. version navigateur (le vrai firmware en WebAssembly)
sh tools/alter/wasm/build_emu.sh
if [ "$KIND" = beta ]; then
  node tools/alter/alter_emu_test.mjs build/emu/felucca.wasm build/emu/alter_play.wav
  node tools/alter/alter_link_test.mjs build/emu/felucca.wasm
  node tools/alter/alter_learn_test.mjs build/emu/felucca.wasm
  [ -f tools/alter/alter_midiout_test.mjs ] && node tools/alter/alter_midiout_test.mjs build/emu/felucca.wasm
  [ -f tools/alter/alter_song_test.mjs ] && node tools/alter/alter_song_test.mjs build/emu/felucca.wasm
  [ -f tools/alter/alter_stems_test.mjs ] && node tools/alter/alter_stems_test.mjs build/emu/felucca.wasm
  # (v1.28) la dernière Release → cette version : scènes, NOMS, SONG, MIDI LEARN gardés (ALTER_OLD_WASM : pages.yml)
  if [ -n "$ALTER_OLD_WASM" ] && [ -f "$ALTER_OLD_WASM" ]; then
    node tools/alter/alter_upgrade_test.mjs "$ALTER_OLD_WASM" build/emu/felucca.wasm
  fi
fi
mkdir -p "$OUT/fonts" "$OUT/editor"
cp build/emu/index.html build/emu/felucca.wasm build/emu/worklet.js "$OUT/"
cp build/emu/fonts/* "$OUT/fonts/"
cp web/alter/editor.html "$OUT/editor/index.html"
for f in link.js guide.html test.html feedback.js; do [ -f "web/alter/$f" ] && cp "web/alter/$f" "$OUT/editor/"; done
cp -r web/alter/fonts "$OUT/editor/"
echo "$VER  $REF @ $SHA" > "$OUT/version.txt"

# 2. vrai firmware FM-1 + page d'installation (compilateur JieLi déjà installé par pages.yml)
./build.sh > "build-fw-log-$KIND.txt" 2>&1 || { tail -40 "build-fw-log-$KIND.txt"; exit 1; }
python3 tools/alter/make_install.py build/felucca.fwsc "$VER ($REF-$SHA)" "$OUT/install"

# 3. kit de secours (outil Windows commun) + le firmware de CE site
mkdir -p "$OUT/secours"; cp "$ZIP" "$OUT/secours/"
Z="$OUT/secours/$(basename "$ZIP")"
python3 tools/alter/rescue/fm1_recover.py build/felucca.fwsc --check --loader tools/alter/rescue/wl82loader.bin
python3 -c "import zipfile,sys;z=zipfile.ZipFile(sys.argv[1],'a');z.write('build/felucca.fwsc','ALTER.fwsc');z.close()" "$Z"

# 4. BETA : bandeau sur chaque page HTML + lien vers la version stable
if [ "$KIND" = beta ]; then
  python3 "$HERE/site_beta_banner.py" "$OUT" "$VER"
fi
echo "== site $KIND prêt : $OUT"
