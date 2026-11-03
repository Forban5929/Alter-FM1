#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ALTER : pile maximale du 2e cœur (FM-1), calculée sur le désassemblage du vrai firmware (build/felucca.dis).
Le 2e cœur n'a que FM1_CPU1_USTACK_WORDS*4 = 2816 octets (bsp/chip/fm1_cpu1.h) ; il exécute c1_parts → mix_part → …
  tools/alter/stack_c1.py build/felucca.dis [--formes]
--formes : affiche les formes d'instructions qui touchent sp (pour régler l'outil).
Appels indirects (moteurs : t->eng->render…) : on prend le pire des fonctions des tables de moteurs (engine_t
.champ = fonction, lues dans les sources). Récursion : signalée. Sortie : chaîne la plus profonde, octets, verdict."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIMIT = 2816
LABEL = re.compile(r"^([A-Za-z_.$][\w.$]*):\s*$")
INSN = re.compile(r"^\s*[0-9a-f]+:\s+(?:[0-9a-f]{2} )+\s*\t(.*)$")
CALL = re.compile(r"\bcall\s+-?\d+\s+<([\w.$]+)(?:\+0x[0-9a-f]+)?\s*:")
ICALL = re.compile(r"\bcall\s+r\d+\b|\bcall\s+\[")
PUSH = re.compile(r"\[--sp\]\s*=\s*\{([^}]*)\}")
SPADD = re.compile(r"\bsp\s*\+=\s*(-?\d+)")


def regs_count(lst):
    n = 0
    for part in lst.split(","):
        part = part.strip()
        m = re.fullmatch(r"r(\d+)-r(\d+)", part)
        if m:
            n += int(m.group(2)) - int(m.group(1)) + 1
        elif part:
            n += 1
    return n


def parse(dis):
    fns, cur = {}, None
    for ln in dis.splitlines():
        m = LABEL.match(ln)
        if m:
            cur = m.group(1)
            fns.setdefault(cur, {"frame": 0, "calls": set(), "icall": False, "lines": []})
            continue
        m = INSN.match(ln)
        if not m or cur is None:
            continue
        ins = m.group(1)
        f = fns[cur]
        f["lines"].append(ins)
        p = PUSH.search(ins)
        if p:
            f["frame"] += 4 * regs_count(p.group(1))
        s = SPADD.search(ins)
        if s and int(s.group(1)) < 0:
            f["frame"] += -int(s.group(1))
        c = CALL.search(ins)
        if c and c.group(1) != cur:
            f["calls"].add(c.group(1))
        elif c:
            f["self"] = True
        if ICALL.search(ins):
            f["icall"] = True
    return fns


def engine_fns():
    names = set()
    for p in list((ROOT / "firmware" / "src").glob("eng_*.c")) + list((ROOT / "firmware" / "engines" / "src").glob("eng_*.c")) \
            + [ROOT / "firmware" / "src" / "engines.c"]:
        t = p.read_text(errors="replace")
        for m in re.finditer(r"static const engine_t \w+ = \{(.*?)\n\};", t, re.S):
            names.update(re.findall(r"\.\w+\s*=\s*([A-Za-z_]\w*)\s*[,}\n]", m.group(1)))
    return names


def main(path, formes=False):
    dis = Path(path).read_text(errors="replace")
    fns = parse(dis)
    if formes:
        seen = {}
        for f in fns.values():
            for ins in f["lines"]:
                if re.search(r"\bsp\b", ins):
                    k = re.sub(r"-?\d+", "N", ins)
                    seen[k] = seen.get(k, 0) + 1
        for k, v in sorted(seen.items(), key=lambda x: -x[1])[:40]:
            print(f"{v:6d}  {k}")
    eng = {n for n in engine_fns() if n in fns}
    memo, path_of, warn = {}, {}, set()

    def depth(fn, stack):
        if fn in memo:
            return memo[fn]
        if fn in stack:
            warn.add(f"récursion : {' → '.join(stack[stack.index(fn):] + [fn])}")
            return 0
        f = fns.get(fn)
        if f is None:
            warn.add(f"fonction inconnue : {fn}")
            memo[fn] = 0
            return 0
        if f.get("self"):
            warn.add(f"s'appelle elle-même : {fn}")
        best, via = 0, None
        targets = set(f["calls"])
        if f["icall"]:
            targets |= eng
        for c in targets:
            d = depth(c, stack + [fn])
            if d > best:
                best, via = d, c
        memo[fn] = f["frame"] + best
        path_of[fn] = via
        return memo[fn]

    root = "c1_parts"
    if root not in fns:
        sys.exit(f"stack_c1: {root} absent du désassemblage")
    total = depth(root, [])
    chain, fn = [], root
    while fn:
        chain.append(f"{fn} ({fns[fn]['frame']} o{' +indirect' if fns[fn]['icall'] else ''})")
        fn = path_of.get(fn)
    print(f"pile 2e cœur, pire chemin : {total} o sur {LIMIT} ({total * 100 // LIMIT} %)")
    print("  " + "\n  → ".join(chain))
    print(f"  appels indirects résolus vers {len(eng)} fonctions de moteurs")
    for w in sorted(warn):
        print("  ATTENTION " + w)
    top = sorted(((f["frame"], n) for n, f in fns.items() if n in memo), reverse=True)[:8]
    print("  plus gros cadres sur ce chemin d'appels : " + ", ".join(f"{n} {s} o" for s, n in top))
    verdict = "OK" if total <= LIMIT * 85 // 100 else "TROP JUSTE" if total <= LIMIT else "DÉBORDE"
    print(f"verdict : {verdict}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1], "--formes" in sys.argv[2:]))
