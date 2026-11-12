# ALTER for the M-VAVE FM-1

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

ALTER is alternative firmware for the **M-VAVE FM-1** pocket synthesizer. It turns the FM-1 into a single live instrument
for industrial techno, noise and texture work: **three euclidean circles**, each playing its own sound, driven by
**dice**, chaos and performance fills instead of a piano keyboard.

ALTER is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita. It keeps Felucca's hardware layer, effects and
installer, and replaces the groovebox with a new instrument.

> **Status: test versions (v0.7x), on the way to 1.0.** Tested on a real FM-1 (UC25HQ80 flash, 12/11/2026): sound, dice,
> CHAOS, locks, saves, power cuts while playing, song, effects, mixer and companion page pass; a MIDI-learn controller and a
> long jam are the last tests before 1.0. The version is shown at power-on and in SYSTEM › ABOUT.

## What it does

- **3 circles**, each a euclidean rhythm (or a TURING machine) with its own sound, envelope, effects and mixer channel.
- **7 sound engines**: ALTER (noise and metal, inspired by the *Basimilus Iteritas Alter* Eurorack module), PHYS, NOISE,
  RESO, PLATE, CORDE, RING.
- **Dice**: a new pattern per circle, velocity / chance / length, pitch and throws into the delay and reverb, locks for what
  must stay; **CHAOS** (the three circles at once) and **CHAOS TOTAL** (everything, undoable within 3 s).
- **Performance**: mutes, fills while held (DUB DELAY, DUB REVERB, DESTROY, ROLL 1/16 and 1/8), REC of knob moves,
  scales and chord progressions (PROG).
- **Mixer**: per circle CONSOLE (DRIVE, 3-band EQ with KILL, compressor, delay and reverb sends), MASTER FILTER, delay with TAPE
  mode, reverb, 3 LFOs and a modulation matrix.
- **Song**: three rhythm slots A B C and a chain of up to 16 links.
- **Memory**: 48 scenes (banks A B C), each with its name; everything comes back, including what the dice drew.
- **MIDI**: USB and TRS in, a fixed CC map, clock from USB or TRS, and **MIDI learn** for any controller.
- **Companion page** in Chrome / Edge: every setting on a big screen, the mixer, patch files on your computer, USB audio
  (listen, record WAV).

## Try, install, read

| | Link |
| --- | --- |
| Try it in your browser (no FM-1 needed) | <https://forban5929.github.io/Alter-FM1/> |
| Install on your FM-1 (Chrome or Edge, USB cable) | <https://forban5929.github.io/Alter-FM1/install/> |
| Companion page | <https://forban5929.github.io/Alter-FM1/editor/> |
| Controls guide (one page, with drawings) | <https://forban5929.github.io/Alter-FM1/editor/guide.html> |
| Manual | [docs/MANUAL.md](docs/MANUAL.md) |

**Installing is at your own risk.** To go back to M-VAVE's firmware, use *Back to official V15* on
[The Firmware Hub](https://thefirmwarehub.com/). If an update goes wrong, the FM-1 can always be put in UPDATE mode
(OCT− + OCT+ held 5 s) and the rescue kit on the install page writes it again (Windows).

### Which FM-1s

ALTER, like Felucca, knows the two flash chips found in FM-1s so far: Puya P25Q80 (JEDEC `0x856014`) and UC25HQ80
(`0xB36014`, newer units). On an FM-1 with another chip, ALTER says **UNKNOWN FLASH** and the chip's ID at power-on and in
ABOUT: it plays, but saves stay in RAM and the USB update is off. Please
[open an issue](https://github.com/Forban5929/Alter-FM1/issues) with that ID.

## Building

The firmware is one C file (`firmware/src/felucca.c`), integers only. GitHub Actions builds it on every push, with the
browser emulator and the install page. To build it yourself see [BUILDING.md](BUILDING.md) (Felucca's instructions: same
toolchain). The browser build: `sh tools/alter/wasm/build_emu.sh`.

## AI disclaimer

ALTER is developed with the help of an AI coding assistant (code, tests, documentation). The design, the sounds, the
choices and the direction are the musician's who plays it. No generative AI is used for music or artwork.

## Credits and licence

- Built on **Felucca** © 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments — thank you. ALTER's additions © 2026 Forban5929.
- Free software: [GPL-3.0-only](LICENSE). The engines in `firmware/engines` are MIT
  ([Felucca-Engines](https://github.com/hugelton/Felucca-Engines)); the hardware layer in `firmware/bsp` is MPL-2.0
  ([felucca-bsp](https://github.com/hugelton/felucca-bsp)); fonts and ported DSP keep their own licences
  ([LICENSES/](LICENSES/), [LICENSING.md](LICENSING.md), [CREDITS.md](CREDITS.md)).
- The ALTER engine is *inspired by* Noise Engineering's Basimilus Iteritas Alter. ALTER is not affiliated with or endorsed by
  Noise Engineering.
- M-VAVE and FM-1 are trademarks of their respective owners. ALTER is not affiliated with or endorsed by them, nor by
  Hügelton Instruments.
