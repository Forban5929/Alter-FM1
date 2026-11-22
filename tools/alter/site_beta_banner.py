# SPDX-License-Identifier: GPL-3.0-only
# Ajoute un bandeau « BETA » en haut de chaque page HTML du site /beta/ (v1.1, 22/11).
# Le bandeau est fixe, fin, et dit : version de test + lien vers la version stable (racine du site).
import os, re, sys

out, ver = sys.argv[1], sys.argv[2]
BAR = ('<div id="alter-beta" style="position:fixed;top:0;left:0;right:0;z-index:99999;height:22px;'
       'line-height:22px;background:#c0392b;color:#fff;font:600 12px/22px system-ui,sans-serif;'
       'text-align:center;letter-spacing:.04em">BETA ' + ver + ' &middot; test version, may be unstable &middot; '
       '<a href="/Alter-FM1/" style="color:#fff;text-decoration:underline">stable version</a></div>'
       '<div style="height:22px"></div>')
n = 0
for root, _, files in os.walk(out):
    for f in files:
        if not f.endswith('.html'):
            continue
        p = os.path.join(root, f)
        s = open(p, encoding='utf-8').read()
        if 'id="alter-beta"' in s:
            continue
        s2, k = re.subn(r'(<body[^>]*>)', lambda m: m.group(1) + BAR, s, count=1, flags=re.I)
        if k:
            s2 = re.sub(r'<title>', '<title>[BETA] ', s2, count=1, flags=re.I)
            open(p, 'w', encoding='utf-8').write(s2)
            n += 1
print('bandeau BETA :', n, 'pages')
