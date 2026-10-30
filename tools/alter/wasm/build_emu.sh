#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ALTER's browser build with the system clang + wasm-ld (no Emscripten): same output as web/emu/build.sh,
# build/emu/{index.html, worklet.js, felucca.wasm, fonts/}. Needs build/gen (python3 tools/build.py's generate()).
# The tiny libc (include/, libc.c) only covers what the page reaches; the wasm must import nothing.
set -e
cd "$(dirname "$0")/../../.."
[ -f build/gen/felucca_tables.h ] || python3 -c "import sys; sys.argv=['x']; sys.path.insert(0, 'tools'); import build; build.generate()"
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)
O=build/emu
mkdir -p $O/fonts
CF="--target=wasm32 -O2 -ffp-contract=off -std=gnu99 -w -nostdlibinc -isystem tools/alter/wasm/include -isystem $(clang -print-resource-dir)/include"
clang $CF "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware/bsp/chip -Ifirmware/bsp/boards/fm1 \
    -c web/emu/felucca_web.c -o $O/emu.o
clang $CF -c tools/alter/wasm/libc.c -o $O/libc.o
wasm-ld --no-entry --export-dynamic --gc-sections -z stack-size=1048576 --initial-memory=33554432 -o $O/felucca.wasm $O/emu.o $O/libc.o
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/ (ALTER)/g" web/emu/index.html > $O/index.html
python3 tools/alter/wasm/alter_page.py $O/index.html --artifact $O/alter.html
cp web/emu/worklet.js $O/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt $O/fonts/
echo "emu: $O ($(wc -c < $O/felucca.wasm | tr -d ' ') B wasm, $VERSION)"
