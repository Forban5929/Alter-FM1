# SPDX-License-Identifier: GPL-3.0-only
# Sons proposés (v1.27, Romain 24/11) : le bouton « Propose… » de la page BETA crée une Issue « [son] SOUND PROPOSED: … »
# avec le son en JSON. Ce script lit ces Issues (étiquette a-trier) et écrit la ligne ALT_PRE (alter_sounds.c) de chacune.
#   python3 tools/alter/alter_sound_intake.py            (liste + lignes C)
# Remplacer un son : garder son fx[] (les scènes d'usine s'en servent) et sa place dans ALT_PRE (les scènes le trouvent
# par son nom : alt_pre_find ; un nouveau nom = mettre à jour ALT_SCENE). Nouveau son : à la FIN d'ALT_PRE.
# Puis : balance LUFS (alter_balance.py) si Romain veut, tests emu, retirer « a-trier », fermer l'Issue avec le commit.
import json, re, subprocess, sys

MODES = {"FREE": 0, "PERC": 1, "STAB": 2, "DRONE": 3}
raw = subprocess.run(["gh", "api", "repos/Forban5929/alter-fm1/issues?labels=a-trier&state=open&per_page=100"],
                     capture_output=True, text=True, check=True).stdout
for it in json.loads(raw):
    body = it.get("body") or ""
    if "SOUND PROPOSED" not in (it.get("title") or "") + body:
        continue
    m = re.search(r"```json\s*(\{.*?\})\s*```", body, re.S)
    if not m:
        print(f"#{it['number']} : pas de JSON lisible"); continue
    j = json.loads(m.group(1).replace("@​", "@"))
    e = ", ".join(str(int(v)) for v in j["e"])
    env = ", ".join(str(int(v)) for v in j["env"])
    line = (f'    {{"{j["name"]}", {j["eng"]}, {{{e}}}, {{{env}}}, {{0, 0, 0, 0}}, {int(j["level"])}, '
            f'{MODES.get(j.get("mode"), 0)}' + (f', {int(j["rumble"])}' if j.get("rumble") else "") + "},")
    print(f"#{it['number']} {j['name']} ({j['engine']}) : " + (f"remplace {j['replaces']}" if j.get("replaces") else "nouveau son")
          + (f" — note : {j['note']}" if j.get("note") else "") + f" — version {j.get('version', '?')}")
    print(line)
sys.exit(0)
