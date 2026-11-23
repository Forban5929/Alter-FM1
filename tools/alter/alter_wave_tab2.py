# SPDX-License-Identifier: GPL-3.0-only
"""WAVE, listening round 5 (Romain 23/11: "la POS de RUST et FDBK ne change pas grand-chose: une table plus variée ?").
Today's RUST and FEEDBACK beside two new versions of each; every clip = one held note, POS swept 0 -> 127 -> 0 over 6 s,
then the same note hammered (16ths) with POS stepping through 8 places. Floats here, the firmware's way of reading
(16 frames, each band-limited, its loudness evened out, linear between frames).
  RUST (now)       tuned noise, POS only tilts it brighter
  RUST SWEEP       tuned noise with a resonant peak that POS moves from the 2nd to the 60th harmonic (a filter through rust)
  RUST MORPH       four different noises one after the other, sparse (a few loud harmonics) to dense (all of them)
  FDBK (now)       two close partials beating, POS drives them harder (the loudness evening hides most of it)
  FDBK CLIMB       the howling pair climbs the harmonics with POS (3rd .. 40th), like feedback going up
  FDBK SCREAM      a clean whistle at 0, more and more partials beating against each other, then a folded scream
    python3 tools/alter/alter_wave_tab2.py OUTDIR
"""
import sys
import wave

import numpy as np

FS = 44100
N = 2048
H = 64
TH = 2 * np.pi * np.arange(N) / N
K = np.arange(1, H + 1)


def band(x):
    X = np.fft.rfft(x)
    X[0] = 0
    X[H + 1:] = 0
    return np.fft.irfft(X, N)


def harm(a, ph):
    return band(sum(a[k - 1] * np.sin(k * TH + ph[k - 1]) for k in K))


def rust_now(f):
    r = np.random.default_rng(11)
    a = r.pareto(1.5, H) * (K ** -(1.2 - 0.9 * f))
    ph = r.uniform(0, 2 * np.pi, H) + f * r.uniform(-3, 3, H)
    return harm(a, ph)


def rust_sweep(f):
    r = np.random.default_rng(11)
    base = r.pareto(1.5, H) * K ** -0.6
    ph = r.uniform(0, 2 * np.pi, H)
    c = 2 * (30 ** f)                                 # the peak: harmonic 2 .. 60
    peak = 1 + 14 * np.exp(-0.5 * ((K - c) / (0.8 + 0.15 * c)) ** 2)
    return harm(base * peak * K ** -0.3, ph)


def rust_morph(f):
    seg = f * 3
    i = min(int(seg), 2)
    t = seg - i
    out = []
    for s in (i, i + 1):
        r = np.random.default_rng(100 + s)
        dens = [0.12, 0.3, 0.6, 1.0][s]               # the share of harmonics that sound
        a = r.pareto(1.2, H) * (r.random(H) < dens) * K ** -(0.9 - 0.2 * s)
        a[0] += 0.3
        out.append(harm(a, r.uniform(0, 2 * np.pi, H)))
    a0, a1 = out[0] / (np.std(out[0]) + 1e-9), out[1] / (np.std(out[1]) + 1e-9)
    return a0 * (1 - t) + a1 * t


def fdbk_now(f):
    x = np.sin(9 * TH) + 0.8 * np.sin(10 * TH) + 0.4 * np.sin(TH)
    return band(np.tanh(x * (1 + f * 12)))


def fdbk_climb(f):
    k = 3 + f * 37
    k1 = int(k)
    m = k - k1
    x = (1 - m) * (np.sin(k1 * TH) + 0.8 * np.sin((k1 + 1) * TH)) + m * (np.sin((k1 + 1) * TH) + 0.8 * np.sin((k1 + 2) * TH))
    return band(np.tanh((x + 0.5 * np.sin(TH)) * 3))


def fdbk_scream(f):
    n = 1 + int(f * 7)
    r = np.random.default_rng(7)
    ks = [12, 13, 7, 19, 23, 5, 31, 29]
    x = 0.4 * np.sin(TH)
    for j in range(n):
        x += (1 if j < n - 1 else (f * 7) % 1 + 0.01) * np.sin(ks[j] * TH + r.uniform(0, 6.3))
    g = 1 + f * 8
    y = np.sin(np.pi / 2 * x * g / max(1, n ** 0.5))      # folded harder as it grows
    return band(y)


TABS = [("RUST_NOW", rust_now), ("RUST_SWEEP", rust_sweep), ("RUST_MORPH", rust_morph),
        ("FDBK_NOW", fdbk_now), ("FDBK_CLIMB", fdbk_climb), ("FDBK_SCREAM", fdbk_scream)]


def frames(fn):
    fr = []
    for i in range(16):
        x = fn(i / 15)
        fr.append(x / (np.sqrt(np.mean(x ** 2)) + 1e-12) * 0.25)   # the firmware's per-frame loudness evening
    return np.array(fr)


def osc(fr, hz, pos):                                 # pos per sample 0..1
    n = len(pos)
    ph = (np.cumsum(np.full(n, hz / FS)) % 1) * N
    i0 = ph.astype(int)
    i1 = (i0 + 1) % N
    t = ph - i0
    fp = pos * 15
    f0 = np.minimum(fp.astype(int), 14)
    ff = fp - f0
    a = fr[f0, i0] * (1 - t) + fr[f0, i1] * t
    b = fr[f0 + 1, i0] * (1 - t) + fr[f0 + 1, i1] * t
    return a * (1 - ff) + b * ff


def clip(fn):
    fr = frames(fn)
    hz = 65.4
    n1 = FS * 6
    pos = np.concatenate([np.linspace(0, 1, n1 // 2), np.linspace(1, 0, n1 // 2)])
    held = osc(fr, hz, pos) * np.minimum(1, np.arange(n1) / 400) * np.minimum(1, (n1 - np.arange(n1)) / 2000)
    step = int(FS * 60 / 130 / 4)
    hits = []
    for j in range(32):
        p = (j % 8) / 7
        e = np.exp(-np.arange(step) / (0.09 * FS))
        hits.append(osc(fr, hz * (2 if j % 8 == 6 else 1), np.full(step, p)) * e)
    y = np.concatenate([held, np.zeros(FS // 4), np.concatenate(hits)])
    return y / (np.max(np.abs(y)) + 1e-9) * 0.7


def main():
    out = sys.argv[1]
    for name, fn in TABS:
        y = clip(fn)
        with wave.open(f"{out}/{name}.wav", "wb") as w:
            w.setnchannels(1), w.setsampwidth(2), w.setframerate(FS)
            w.writeframes((y * 32767).astype(np.int16).tobytes())
        print(name)


if __name__ == "__main__":
    main()
