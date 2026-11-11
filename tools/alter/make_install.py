#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ALTER : la page d'installation pour la FM-1 (version 1, 03/11), à partir de l'installateur Felucca éprouvé
(web/index_pkg.html + fm1pkg.js + fm1ota.js : le protocole de mise à jour USB n'est PAS modifié).
  tools/alter/make_install.py build/felucca.fwsc VERSION DOSSIER      → DOSSIER/index.html, DOSSIER/firmware/…
Changé : le nom (ALTER), le texte d'avertissement, les crédits ; le retour à V15 et le secours renvoient à
The Firmware Hub (la page Felucca sauvegardait d'abord par l'éditeur, retiré le 01/11 : ses éléments restent, cachés,
pour que le script de la page ne change pas)."""
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "web"


def strip_module(src):
    return re.sub(r"^export ", "", re.sub(r"^import [^\n]*\n", "", src, flags=re.M), flags=re.M)


def product_of(raw):
    m = re.search(rb"FM-1_\d{3}", raw)
    return m.group(0).decode() if m else ""


def sub1(s, old, new):
    if s.count(old) != 1:
        raise SystemExit(f"make_install: {old[:60]!r} trouvé {s.count(old)} fois dans index_pkg.html")
    return s.replace(old, new)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if product != "FM-1_900":
        raise SystemExit(f"{pkg}: identité {product!r}, attendu FM-1_900")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: pas le chargeur de mise à jour Felucca dedans : paquet refusé")
    html = (WEB / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((WEB / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((WEB / "fm1ota.js").read_text(encoding="utf-8"))
    name = "alter-" + re.sub(r"-+", "-", re.sub(r"[^A-Za-z0-9.-]", "-", version)).strip("-") + ".fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "firmware/" + name})
    html = sub1(html, "/*LIB*/", lib)
    html = sub1(html, "/*META*/", meta)
    html = sub1(html, "<title>Felucca</title>", "<title>ALTER FM-1</title>")
    short = version.split(" ")[0]                # (v0.71, Romain: the version shown big: "v0.71"; the build after it, small)
    html = sub1(html, "<h1>Felucca</h1>", f"<h1>ALTER <span style=\"font-size:.6em;opacity:.8\">{short}</span></h1>\n"
                f"  <p class=\"small\">Firmware ALTER pour M-VAVE FM-1 · version {version}</p>")
    html = sub1(html, "say(`Felucca ${meta.version}`);", "say(`ALTER ${meta.version}`);")
    html = re.sub(r'beta: "Felucca 1\.0 is a field-testing release; please report problems on GitHub\.',
                  'beta: "ALTER is still a test version: please report problems on GitHub (Issues). SYSTEM > SETUP > CPU '
                  'METER shows the processor load.', html)
    # le retour à V15 : par The Firmware Hub (éléments Felucca gardés, cachés : le script les attend)
    html = sub1(html, '<details style="margin-top:24px">', '<div class="small" style="margin-top:24px">'
                '<b>Retour au firmware officiel / secours :</b> <a href="https://thefirmwarehub.com/" target="_blank" '
                'rel="noopener" style="color:inherit">The Firmware Hub</a> (« Back to official V15 »). FM-1 bloquée : '
                'tenir OCT− en l’allumant (secours USB), puis réinstaller.<br><b>Si l’installation s’arrête sur « after 0 requests : '
                'nothing was written »</b> (rien n’est écrit, la FM-1 est intacte) : débrancher l’USB, éteindre, tenir OCT− en '
                'rallumant, rebrancher, puis Install (la page reprend en mode mise à jour). Copier le journal pour Claude.</div>\n  <details style="display:none">')
    html = sub1(html, '<p><a href="../editor/" style="color:inherit" data-t="editor"></a></p>',
                '<p><a href="../editor/" style="color:inherit">Page compagnon : les réglages des trois sons, en grand</a></p>')
    html = html.replace("../../firmware/", "firmware/")
    html = sub1(html, '<p class="small">Felucca by Leo Kuroshita', '<p class="small">ALTER par Romain (Forban5929), '
                '<a href="https://github.com/Forban5929/Alter-FM1" target="_blank" rel="noopener" style="color:inherit">'
                'github.com/Forban5929/Alter-FM1</a>, construit sur Felucca.<br>Felucca by Leo Kuroshita')
    fw = out / "firmware"
    (fw / "LICENSES").mkdir(parents=True, exist_ok=True)
    (out / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    names = sorted(f.name for f in (ROOT / "LICENSES").glob("*.txt"))
    for n in names:
        shutil.copy(ROOT / "LICENSES" / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / doc, fw / doc)
    print(f"install: {out}/index.html ({len(html)} o), firmware/{name} ({len(raw)} o, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
