# SPDX-License-Identifier: GPL-3.0-only
# Scènes proposées (v1.2.24, Romain 28/11 : « les scènes d'usine = des vitrines ») : le bouton « Propose scene… » de la page
# BETA crée une Issue « [son] SCENE PROPOSED: … » avec la scène ENTIÈRE (le BLOB, comme un fichier patch) en JSON.
#   python3 tools/alter/alter_scene_intake.py            (liste)
#   python3 tools/alter/alter_scene_intake.py N          (Issue N : écrit firmware/src/alter_fs_<nom>.h, imprime la ligne ALT_SCENE)
# Intégrer TELLE QUELLE (Romain) : remplacer = mettre la ligne à la place de l'ancienne dans ALT_SCENE (alter_sounds.c) ;
# nouvelle = à la fin (8 au plus, ALT_NFACT) ; #include du .h près d'alter_user_ambient.h ; mettre à jour FACTORY_SCENES
# (web/alter/editor.html : les noms dans l'ordre) ; tests ; fermer l'Issue (gh api, REST).
import base64, json, re, subprocess, sys

raw = subprocess.run(["gh", "api", "repos/Forban5929/alter-fm1/issues?labels=a-trier&state=open&per_page=100"],
                     capture_output=True, text=True, check=True).stdout
want = int(sys.argv[1]) if len(sys.argv) > 1 else None
for it in json.loads(raw):
    body = it.get("body") or ""
    if "SCENE PROPOSED" not in (it.get("title") or "") + body:
        continue
    m = re.search(r"```json\s*(\{.*?\})\s*```", body, re.S)
    if not m:
        print(f"#{it['number']} : pas de JSON lisible"); continue
    j = json.loads(m.group(1))
    b = base64.b64decode(j["scene"])
    print(f"#{it['number']} {j['name']} « {j.get('about', '')} » : " + (f"remplace {j['replaces']}" if j.get("replaces") else "nouvelle")
          + f" — sons {' . '.join(j.get('sounds', []))} — {len(b)} o (déclaré {j.get('size')}) — version {j.get('version', '?')}"
          + (f" — note : {j['note']}" if j.get("note") else ""))
    if want != it["number"]:
        continue
    slug = re.sub(r"[^a-z0-9]", "_", j["name"].lower()).strip("_") or "scene"
    sym = "ALT_FS_" + slug.upper()
    rows = ["    " + ", ".join(str(x) for x in b[i:i + 24]) + "," for i in range(0, len(b), 24)]
    path = f"firmware/src/alter_fs_{slug}.h"
    with open(path, "w") as f:
        f.write(f"/* {j['name']}: a factory scene proposed by Romain (Issue #{it['number']}, {j.get('version', '?')}), the whole scene as\n"
                f" * the page's patch file keeps it (alter_store.c alt_blob_t), loaded as is (alt_scene_blob) */\n"
                f"static const uint8_t {sym}[{len(b)}] = {{\n" + "\n".join(rows) + "\n};\n")
    about = j.get("about") or " . ".join(j.get("sounds", []))
    print(f"\nécrit {path} ; #include \"alter_fs_{slug}.h\" dans alter_sounds.c, puis la ligne :\n"
          f'    {{"{j["name"]}", "{about}", 0, 0, 0, {{{{0}}}}, 0, 0, 0, 0, {{0, 0, 0}}, .blob = {sym}, .blob_n = sizeof {sym}}},')
sys.exit(0)
