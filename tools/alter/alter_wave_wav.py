# SPDX-License-Identifier: GPL-3.0-only
"""WAVE engine listening test (Romain 23/11: "un engine wavetable indus, inspiré de Shrub / TastyWaves").
Four original industrial wavetables (16 frames, 64 harmonics, synthesised here: no third-party data) played by a
plain mono voice (130 BPM, a 16th-note riff, POS swept 0 -> 1 over 4 bars), dry and through three warps:
BEND (the phase warp ph + A sin 2 pi ph, after TastyWaves' BEND), SYNC (the read sped up inside each cycle, reset at
its start) and BITS (the output on fewer levels). A sketch made beside the firmware (floats here; the FM-1 would read
the same tables in integers).
    python3 tools/alter/alter_wave_wav.py OUTDIR      -> OUTDIR/<TABLE>_<VARIANT>.wav + list.json
"""
import json
import sys
import wave
from pathlib import Path

import numpy as np

FS = 44100
FRAMES = 16
HMAX = 64
HIRES = 4096
TH = np.arange(HIRES) * 2 * np.pi / HIRES
K = np.arange(1, HMAX + 1)


def band(x):
    """one cycle at HIRES -> harmonics 1..64 only -> 2048-point cycle (band-limited, unit RMS)"""
    X = np.fft.rfft(x)
    X[0] = 0
    X[HMAX + 1:] = 0
    y = np.fft.irfft(X, HIRES)[::2]
    return y / (np.sqrt(np.mean(y ** 2)) + 1e-9)


def t_clang(f):                                   # bell / anvil clusters, brighter and harsher along the table
    a = np.zeros(HMAX)
    rng = np.random.default_rng(7)
    ph = rng.uniform(0, 2 * np.pi, HMAX)
    base = [1, 3, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53]
    for i, k in enumerate(base):
        a[k - 1] = 1 / (1 + i * (1 - f) * 0.9) * (1 if i < 3 + f * 12 else 0.15)
    return band(sum(a[k - 1] * np.sin(k * TH + ph[k - 1] * f) for k in K if a[k - 1]))


def t_grind(f):                                   # a saw crushed onto fewer and fewer levels
    saw = 1 - TH / np.pi
    lv = 2 ** (5 - 4 * f)                         # 32 levels .. 2
    return band(np.round(saw * lv / 2) / (lv / 2))


def t_robot(f):                                   # a buzz through vowel formants A E I O U (f0 ~110 Hz)
    V = [(730, 1090, 2440), (530, 1840, 2480), (270, 2290, 3010), (570, 840, 2410), (300, 870, 2240)]
    p = f * (len(V) - 1)
    i = min(int(p), len(V) - 2)
    t = p - i
    F = [V[i][j] * (1 - t) + V[i + 1][j] * t for j in range(3)]
    a = np.zeros(HMAX)
    for k in K:
        hz = 110 * k
        a[k - 1] = sum(g / (1 + ((hz - fc) / (fc * 0.12)) ** 2) for fc, g in zip(F, (1, 0.6, 0.3))) / k ** 0.3
    return band(sum(a[k - 1] * np.sin(k * TH) for k in K))


def t_tear(f):                                    # a hard-synced saw, its slave from x1 to x8: it tears
    r = 1 + 7 * f
    return band(1 - 2 * ((TH / (2 * np.pi) * r) % 1))


TABLES = {"CLANG": t_clang, "GRIND": t_grind, "ROBOT": t_robot, "TEAR": t_tear}


def voice(tab, warp, amt):
    bpm, bars = 130, 4
    step = int(FS * 60 / bpm / 4)
    riff = [36, 36, 48, 36, 39, 36, 46, 36, 36, 43, 36, 48, 36, 39, 34, 36]    # C minor, low
    n = step * 16 * bars
    out = np.zeros(n)
    ph = 0.0
    tabs = np.array(tab)                          # FRAMES x 2048
    N = tabs.shape[1]
    for s in range(16 * bars):
        f0 = 440 * 2 ** ((riff[s % 16] - 69) / 12)
        for i in range(step):
            j = s * step + i
            pos = j / n * (FRAMES - 1)
            fa = int(pos)
            fb = min(fa + 1, FRAMES - 1)
            x = pos - fa
            ph = (ph + f0 / FS) % 1.0
            p = ph
            if warp == "BEND":
                p = (p + amt * 0.15 * np.sin(2 * np.pi * p)) % 1.0
            elif warp == "SYNC":
                p = (p * (1 + amt * 3)) % 1.0
            k = p * N
            k0 = int(k) % N
            k1 = (k0 + 1) % N
            fr = k - int(k)
            ya = tabs[fa][k0] * (1 - fr) + tabs[fa][k1] * fr
            yb = tabs[fb][k0] * (1 - fr) + tabs[fb][k1] * fr
            y = ya * (1 - x) + yb * x
            if warp == "BITS":
                q = 2 ** (6 - 4 * amt)
                y = np.round(y * q) / q
            env = np.exp(-i / (FS * 0.09))
            out[j] = y * env
    # a little drive and a DC-free, gentle low-pass: the FM-1's track after the engine
    out = np.tanh(out * 1.5)
    return out / (np.max(np.abs(out)) + 1e-9) * 0.7


def write(path, x):
    d = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(d.tobytes())


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    lst = []
    for name, fn in TABLES.items():
        tab = [fn(f / (FRAMES - 1)) for f in range(FRAMES)]
        for warp, amt in (("DRY", 0), ("BEND", 1), ("SYNC", 0.7), ("BITS", 0.8)):
            fname = f"{name}_{warp}"
            write(out / f"{fname}.wav", voice(tab, warp, amt))
            lst.append({"table": name, "warp": warp, "file": fname})
            print(fname, flush=True)
    (out / "list.json").write_text(json.dumps(lst, indent=1))


main()
