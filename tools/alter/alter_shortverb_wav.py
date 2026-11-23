# SPDX-License-Identifier: GPL-3.0-only
"""Short, strange reverbs to listen to (Romain 23/11: "j'adore les shorts verb genre convolution bizarre"). Sketches beside
the firmware (floats here), each a small network the FM-1 could run in integers on the reverb's own buffers:
  CAN     a tin can: four very short combs (1-3 ms) ringing together, band-passed: boxy, metallic, ~0.3 s
  PIPE    a pipe: two combs (7 and 11 ms) with a low-pass in their loops: hollow, pitched, ~0.5 s
  METAL   sheet metal: six short all-passes (a dispersion, the "boing") into two combs, bright: ~0.6 s
  SHARDS  shards: 14 sparse reflections at random over 15-140 ms, random signs, each brighter or darker: a
          broken, granular room (what a strange impulse response does)
Input: a dry loop rendered by the emulator (tools/alter/alter_shortverb_wav.py DRY.wav OUTDIR); each output mixed so
its loudness matches the SHORT preset's render (REF.wav).
"""
import sys
import wave

import numpy as np
from scipy.signal import lfilter

FS = 44100


def rd(p):
    with wave.open(p) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, 2).astype(float) / 32768
    return a


def wr(p, a):
    a = np.clip(a, -1, 1)
    with wave.open(p, "wb") as w:
        w.setnchannels(2), w.setsampwidth(2), w.setframerate(FS)
        w.writeframes((a * 32767).astype(np.int16).tobytes())


def comb(x, ms, fb, lp=0.0):
    d = max(1, int(FS * ms / 1000))
    a = np.zeros(d + 2)
    if lp:                                         # y = x + fb * LP(y[n - d]), LP: one pole (1 - lp) / (1 - lp z^-1)
        b = np.array([1.0, -lp])
        a[0], a[1] = 1.0, -lp
        a[d] += -fb * (1 - lp)
        return lfilter(b, a, x)
    a[0] = 1.0
    a[d] = -fb
    return lfilter([1.0], a, x)


def allpass(x, ms, g):
    d = max(1, int(FS * ms / 1000))
    b = np.zeros(d + 1)
    a = np.zeros(d + 1)
    b[0], b[d] = -g, 1.0
    a[0], a[d] = 1.0, -g
    return lfilter(b, a, x)


def onepole(x, f, hp=False):
    k = 1 - np.exp(-2 * np.pi * f / FS)
    y = lfilter([k], [1, -(1 - k)], x)
    return x - y if hp else y


def can(x, ch):
    ms = [1.13, 1.71, 2.29, 3.07] if ch == 0 else [1.19, 1.63, 2.41, 2.93]
    y = sum(comb(x, m, 0.9) for m in ms) / 4
    return onepole(onepole(y, 4500), 700, hp=True)


def pipe(x, ch):
    y = comb(x, 7.3 if ch == 0 else 7.6, 0.93, lp=0.35) + 0.7 * comb(x, 11.1 if ch == 0 else 10.7, 0.9, lp=0.35)
    return onepole(y, 120, hp=True)


def metal(x, ch):
    y = x
    for k, m in enumerate([0.31, 0.47, 0.73, 1.03, 1.49, 2.11]):
        y = allpass(y, m * (1.0 if ch == 0 else 1.07), 0.72)
    y = comb(y, 13.3 if ch == 0 else 14.1, 0.86) + comb(y, 17.9 if ch == 0 else 16.7, 0.84)
    return onepole(y, 400, hp=True)


def shards(x, ch):
    r = np.random.default_rng(5 + ch)
    y = np.zeros_like(x)
    for _ in range(14):
        t = int(FS * r.uniform(0.015, 0.14))
        g = r.choice([-1, 1]) * r.uniform(0.3, 1) * np.exp(-t / FS / 0.08)
        tap = np.concatenate([np.zeros(t), x[:-t]]) * g
        y += onepole(tap, r.uniform(800, 9000)) if r.random() < 0.5 else onepole(tap, r.uniform(200, 2000), hp=True)
    return y


def lufs_rms(a):                                    # (a plain RMS: enough to match loudness between versions)
    return np.sqrt(np.mean(a ** 2))


def main():
    dry, ref, out = rd(sys.argv[1]), rd(sys.argv[2]), sys.argv[3]
    tgt = lufs_rms(ref)
    for name, fn in (("CAN", can), ("PIPE", pipe), ("METAL", metal), ("SHARDS", shards)):
        wet = np.stack([fn(dry[:, c], c) for c in (0, 1)], axis=1)
        wet *= lufs_rms(dry) / (lufs_rms(wet) + 1e-9)
        lo, hi = 0.0, 4.0                            # the wet level that brings the mix to the reference's loudness
        for _ in range(30):
            g = (lo + hi) / 2
            if lufs_rms(dry + g * wet) < tgt:
                lo = g
            else:
                hi = g
        wr(f"{out}/{name}.wav", dry + g * wet)
        print(name, round(g, 2))


if __name__ == "__main__":
    main()
