# SPDX-License-Identifier: GPL-3.0-only
"""WAVE engine, listening round 4 (Romain 23/11: tables CLANG RUST SHRED FEEDBACK; WARP like Serum = one MODE + AMNT;
knob plan A: TABLE POS MODE AMNT POS-ENV WARP-ENV SUB SPRD). Each hit throws POS up the table and the WARP amount up,
both falling back over ~150 ms (the two envelopes); one note hammered, 130 BPM, 4 bars. Modes: SYNC BEND PWM MIRROR FM
RING FOLD BITS (phase warps read the table elsewhere; RING / FOLD / BITS change what was read).
    python3 tools/alter/alter_wave_warp.py OUTDIR
"""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import alter_wave_wav as W                       # the tables, the WAV writer

FS = W.FS
TABS = ["CLANG", "RUST", "SHRED", "FEEDBACK"]
MODES = ["DRY", "SYNC", "BEND", "PWM", "MIRROR", "FM", "RING", "FOLD", "BITS"]


def warp_phase(mode, p, a, f0, t):
    if mode == "SYNC":
        return (p * (1 + a * 7)) % 1.0
    if mode == "BEND":
        return p ** (1 + a * 4)
    if mode == "PWM":
        w = 0.5 - 0.45 * a
        return p / w * 0.5 if p < w else 0.5 + (p - w) / (1 - w) * 0.5
    if mode == "MIRROR":
        q = p * 2 if p < 0.5 else 2 - p * 2
        return (p * (1 - a) + q * a) % 1.0
    if mode == "FM":
        return (p + a * 0.6 * np.sin(2 * np.pi * (p * 2))) % 1.0
    return p


def play(tab, mode):
    bpm, bars = 130, 4
    step = int(FS * 60 / bpm / 4)
    hits = [1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1]
    n = step * 16 * bars
    out = np.zeros(n)
    tabs = np.array(tab)
    F, N = tabs.shape
    f0 = 440 * 2 ** ((36 - 69) / 12)
    ph = rph = 0.0
    held = 0.0
    since = 10 ** 9
    for j in range(n):
        s = j // step
        if j % step == 0 and hits[s % 16]:
            since = 0
        t = since / FS
        env = np.exp(-t / 0.15)
        amp = np.exp(-t / 0.18) if since < 10 ** 8 else 0.0
        since += 1
        pos = min(0.15 + 0.75 * env, 1.0) * (F - 1)                  # POS ENV: thrown up, falls back
        a = min(0.35 + 0.55 * env, 1.0) if mode != "DRY" else 0.0     # WARP ENV
        ph = (ph + f0 / FS) % 1.0
        p = warp_phase(mode, ph, a, f0, t)
        fa = int(pos)
        fb = min(fa + 1, F - 1)
        x = pos - fa
        k = p * N
        k0 = int(k) % N
        k1 = (k0 + 1) % N
        fr = k - int(k)
        y = (tabs[fa][k0] * (1 - fr) + tabs[fa][k1] * fr) * (1 - x) + (tabs[fb][k0] * (1 - fr) + tabs[fb][k1] * fr) * x
        if mode == "RING":
            rph += f0 * 2.71 / FS
            y = y * (1 - a) + y * np.cos(2 * np.pi * rph) * 1.414 * a
        elif mode == "FOLD":
            g = 1 + a * 6
            y = np.sin(np.pi / 2 * g * y / 1.6) * 1.6 / (1 + (g - 1) * 0.3)
        elif mode == "BITS":
            if j % max(1, int(1 + a * 12)) == 0:
                q = 2 ** (8 - 6.5 * a)
                held = np.round(y * q) / q
            y = held
        y += 0.5 * np.sin(np.pi * ph) * 0                              # (SUB / SPRD: not in this test)
        out[j] = y * amp
    out = np.tanh(out * 1.3)
    return out / (np.max(np.abs(out)) + 1e-9) * 0.7


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    lst = []
    for name in TABS:
        fn = W.TABLES[name]
        tab = [fn(f / (W.FRAMES - 1)) for f in range(W.FRAMES)]
        for m in MODES:
            fname = f"{name}_{m}"
            W.write(out / f"{fname}.wav", play(tab, m))
            lst.append({"table": name, "warp": m, "file": fname})
            print(fname, flush=True)
    (out / "list.json").write_text(json.dumps(lst, indent=1))


if __name__ == "__main__":
    main()
