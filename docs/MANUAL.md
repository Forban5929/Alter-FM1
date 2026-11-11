# ALTER — manual

Firmware for the M-VAVE FM-1 · test versions v0.4x (the version is shown at power-on and in SYSTEM › ABOUT).
The one-page [controls guide](https://forban5929.github.io/Alter-FM1/editor/guide.html) has drawings of every gesture;
this manual says the same in words, with a little more about each page.

1. [The idea](#1-the-idea)
2. [The panel](#2-the-panel)
3. [The keys](#3-the-keys)
4. [Sounds](#4-sounds)
5. [Rhythm](#5-rhythm)
6. [The dice](#6-the-dice)
7. [Performance](#7-performance)
8. [Scales and chords](#8-scales-and-chords)
9. [Modulation](#9-modulation)
10. [Effects and mixer](#10-effects-and-mixer)
11. [Song](#11-song)
12. [Scenes (save and load)](#12-scenes-save-and-load)
13. [MIDI](#13-midi)
14. [SYSTEM](#14-system)
15. [The companion page](#15-the-companion-page)
16. [Combos and safety](#16-combos-and-safety)
17. [Updating, going back, rescue](#17-updating-going-back-rescue)

---

## 1. The idea

ALTER is one instrument, not a groovebox. Three **circles** play at once; each circle is a rhythm (euclidean, or a TURING
machine) with its own sound. You don't play notes on the keys: the keys mute, roll the dice, push the patterns and fire fills.
The screen shows the three circles, one row each, with a gold hand going round.

The circle you work on is the **chosen circle** (SELECT, or the CIRCLE keys). The four knobs, the quick pages and most
gestures act on it.

## 2. The panel

| Control | What it does |
| --- | --- |
| MASTER | volume |
| SELECT | the chosen circle; in a menu, a row |
| PRESETS | the chosen circle's sound (steps through the sounds of all the engines) |
| ALGORITHM | the chosen circle's envelope mode (FREE, PERC, STAB); in a menu, a tab |
| KNOB 1-4 | the four settings of the page shown (the screen names them) |
| OCT− / OCT+ | modifiers, held with the dice, SONG and REC (see [Combos](#16-combos-and-safety)) |

A page button **tapped** opens its quick page (the four knobs change with it); **held 0.5 s** it opens its menu.
**HOME tapped always brings you back** to the three circles.

| Button | Tapped: quick page | Held: menu |
| --- | --- | --- |
| (none) | the engine's four main settings | |
| SEQ | RHYTHM: LEN HITS ROT PROB (TURING: LEN LOCK DENS RANGE) | the lanes (VELO CHANCE DELAY REVERB PITCH RATCH), the lower keys are the steps; SEQ held + ALGORITHM: EUCLID / TURING |
| ENV | the envelope's four settings, by mode | ENVELOPE: the shape drawn |
| FX | DIST · INSERT · DELAY send · REVERB send | FX page: DIST, INSERT, DELAY (TIME FEEDBK TAPE SEND), REVERB, MASTER |
| LFO | LFO 1: WAVE RATE DEPTH DEST | LFO 1 2 3, GLOBAL, MATRIX |
| EDIT | the engine's four other settings | DEEP: every setting of the circle's sound as a list |
| SCL | SCALE · ROOT · CHORD · PROG | SCALES |
| GLO | BPM · LEVEL · PAN · SWING | MIXER |
| HOME | back to the circles | SYSTEM |
| ARP | SONG chain on / off | SONG page |
| SAVE | the SAVE page | (+ A B C: store a SONG slot) |
| PLAY | play / stop | |
| REC | overdub on / off | 1 s: the chosen circle's moves cleared |

## 3. The keys

On the circles' view (no menu open) the keys play ALTER, not notes.

**Lower row (16 keys), left to right:** MUTE 1 · MUTE 2 · MUTE 3 · (free) · DICE 1 · DICE 2 · DICE 3 · (free) ·
SHIFT − · SHIFT + · LOCK (TURING) · SONG A · SONG B · SONG C · (free) · (free).

**Upper row (11 keys):** CIRCLE 1 · CIRCLE 2 · CIRCLE 3 · DENSITY − · DENSITY + · DUB DELAY · DUB REVERB · DESTROY ·
ROLL 1/16 · ROLL 1/8 · TRIGGER.

- **MUTE n** mutes circle n. **CIRCLE n** chooses circle n.
- **SHIFT − / +** moves the chosen circle's pattern one step back / forward (on a TURING circle: the previous / next pattern).
  **DENSITY − / +** removes / adds one hit.
- **LOCK** freezes a TURING machine's pattern.
- **SONG A B C**: see [Song](#11-song).
- The fills (DUB DELAY … TRIGGER): see [Performance](#7-performance).

## 4. Sounds

Each circle has its own sound: an **engine** plus its settings, an envelope, effects sends and a CONSOLE channel.

| Engine | Character |
| --- | --- |
| ALTER | six oscillators struck as one voice: from harmonic to metallic to dissonant (after the Basimilus Iteritas Alter) |
| PHYS | physical models (ported from DaisySP and Mutable Instruments Rings) |
| NOISE | from analog-style noise to shift-register noise, through a resonant filter |
| RESO | four resonant filters in a feedback ring: they growl, beat, scream (played by hand: the dice leave it out) |
| PLATE | a struck metal sheet: plate, cymbal, gong, cracked bell |
| CORDE | a mistreated metal string: it clangs, screams, buzzes |
| RING | a ring modulator through a resonant filter and a valve drive, with radio troubles: bells, sirens, robots |

On PLATE, CORDE and RING, KNOB 1-4 are MATR (material), SPRD (spread), RES (ring time: the same length at every pitch) and
DIRT. **PRESETS** steps through the sounds of every engine; **EDIT** tapped gives the engine's four other settings; **EDIT held**
opens **DEEP**, every setting of the circle as one list (SOUND, ENV, FX, MIX; MIX ends with **DICE DOSE**: how far the dice
may move this circle).

**Envelope** (ENV): three modes, chosen with ALGORITHM — FREE, PERC, STAB — each with its own four settings.

## 5. Rhythm

**SEQ tapped**: LEN (5-16 steps), HITS, ROT (rotation), PROB. **SEQ held + ALGORITHM** switches the circle between
EUCLID and TURING; a TURING circle shows LEN, LOCK, DENS, RANGE.

**SEQ held** opens the **lanes**: per step VELO, CHANCE, DELAY (a throw into the delay), REVERB (a throw), PITCH, RATCH
(ratchets). The lower keys are the 16 steps.

**SWING** and **BPM** are on GLO's quick page.

## 6. The dice

The three DICE keys each draw for their own circle. What a die draws depends on what you hold with it.

| Gesture | Draws |
| --- | --- |
| DICE tapped | a new pattern: HITS, ROT, flams and a ratchet |
| OCT− + DICE | the VELO and CHANCE lanes, and a new LEN (5 to 16, mostly near 16) |
| OCT+ + DICE | the PITCH lane and the throws into the delay and reverb |
| DICE held 0.6 s | clean: back to the plain pattern, the draw undone |
| OCT− + OCT+ + DICE | the circle's **LOCKS** page: lower keys 1-7 lock RHYTHM (hits, rotation, flams, ratchet), LENGTH, VELOCITY, CHANCE, PITCH, THROWS (delay / reverb), SOUND (what CHAOS changes on the sound: timbre, glide, envelope, force); a locked part is left alone by the dice and CHAOS (CHAOS TOTAL changes everything); HOME: back |
| DICE 1 + 2 + 3 | **CHAOS**: everything drawn on the three circles (timbre, glide and length per hit too), every effect switched off, EUCLID or TURING drawn too; what the circle's LOCKS keep stays. Playing: the screen glitches and it lands on the 1 after a whole bar; again while it waits: cancelled |
| CIRCLE 1 + 2 + 3 | **CHAOS TOTAL**: sounds, engines, modes, rhythms (EUCLID or TURING, LEN 5-16), all at random. Playing, the draw waits: the screen glitches and the new scene lands on the 1 after a whole bar (press again while it waits: cancelled). Stopped: at once. Again within 3 s of the draw: undone |

What the dice drew is kept: save the scene and it comes back. SYSTEM › RANDOM (or **OCT− + HOME**, straight there) chooses
which engines and envelope modes the random may use, and which effects CHAOS TOTAL may draw (ALL FX, DIST INSERT, DELAY,
REVERB; all on by default). CHAOS switches every effect off (DIST, INSERT, the sends, the steps' DELAY and REVERB).

## 7. Performance

- **TRIGGER**: a hit of the chosen circle at once. **REC held + TRIGGER**: the hit is added to its pattern.
- **Fills** (upper keys, held): DUB DELAY, DUB REVERB, DESTROY, ROLL 1/16, ROLL 1/8 on the chosen circle as long as you
  hold them. With REC on, they are written.
- **REC** (overdub): the knobs you turn are written on the steps and play back in a loop. REC held 1 s clears the chosen
  circle's moves; OCT− + OCT+ + REC clears every circle's. (The CONSOLE's EQ and COMP are not recorded, on purpose.)

## 8. Scales and chords

**SCL tapped**: SCALE, ROOT, CHORD, PROG. PROG is a chord progression, one chord a bar, in the scale from the ROOT; CHORD
says which chord. **SCL held**: the SCALES page.

## 9. Modulation

**LFO tapped**: LFO 1's WAVE, RATE, DEPTH, DEST (destination); its wave scrolls on the screen with a dot for where it is.
**LFO held**: LFO 1 2 3, GLOBAL and the modulation MATRIX.

## 10. Effects and mixer

**FX tapped**: the chosen circle's DIST, INSERT, DELAY send, REVERB send. **FX held**: the FX page — DIST, INSERT, DELAY
(TIME, FEEDBK, TAPE mode, SEND), REVERB, MASTER (with the MASTER FILTER: FREQ RES DRIVE MIX MORPH).

**GLO held** opens the **MIXER**:
- tab **MIX**: KNOB 1 2 3 the three circles' levels, KNOB 4 the MASTER, and the output meter;
- tabs **I II III** (ALGORITHM): each circle's **CONSOLE**. SELECT the row; KNOB 1-4 are DRIVE, LOW, MID, HIGH on the first
  row and COMP, DELAY, REVERB, LEVEL on the second. LOW, MID or HIGH at 0 is a KILL.

## 11. Song

Three slots **A B C** keep the **rhythms** of the three circles (steps, lanes, ranges, the dice's draw, REC's moves, the
mutes). The sounds stay as they are. A slot always starts at the next bar, every circle from step 1.

| Gesture | What it does |
| --- | --- |
| A tapped | slot A plays from the next bar (a chain that was looping stops; an empty slot says EMPTY) |
| SAVE held + A | the rhythms playing are stored in A (kept after power-off) |
| A held + C, then B | a chain A › C › B, 4 bars each, looping from the next bar |
| ARP tapped | the chain on / off |
| ARP held | the SONG page: SELECT a link · KNOB 1 its slot · KNOB 2 its bars (1 2 4 8) · KNOB 3 how many links (16 at most) · KNOB 4 the chain on / off |

What you change while a slot plays stays in it until power-off; SAVE held + its key keeps it for good.

## 12. Scenes (save and load)

**SAVE** opens the SAVE page: three tabs (ALGORITHM) — SCENES A, B, C — of 16 slots each, **48 scenes**. Tab A also has
NEW and the four factory scenes (locked).

A **scene** is everything: sounds, rhythms, effects, CONSOLE, REC's moves, ranges, what the dice, CHAOS and CHAOS TOTAL
drew, the chord PROG. A TURING circle comes back frozen as it was saved.

- SELECT a row, **SAVE tapped**: load it.
- **SAVE held 1 s** on a USER row: the NAME page — KNOB 1 2 3 are the three characters (A-Z, 0-9, -, space); SAVE stores,
  HOME cancels. The list then reads e.g. « A05  KCK ».
- **OCT− + OCT+ + HOME**: a blank start (the engines' first sounds, no hits).

## 13. MIDI

ALTER listens on **USB** and on the **TRS** jack.

- **Fixed map**: channels 1 2 3 play / move circles 1 2 3; CC7 LEVEL, CC74 timbre, CC91 REVERB, and more.
- **Clock**: SYSTEM › SETUP › CLOCK — INT, USB, or TRS (e.g. an Elektron's clock).
- **MIDI learn** (SYSTEM › LEARN): any controller. GROUP (KNOB 1 on the first row) chooses what the list shows. SELECT a row,
  turn **KNOB 1 right**: it says WAITING — move a knob or press a pad on your controller: linked. **KNOB 1 left** clears the
  row's link.

| GROUP | What its rows link |
| --- | --- |
| CIRCLE I · II · III | a setting of that circle: the sound's eight, ATTACK DECAY SUSTAIN RELEASE, the CONSOLE (DRIVE LOW MID HIGH COMP DELAY REVERB LEVEL), PAN, INSERT, CHORUS, RUMBLE, GLIDE, HITS, DICE DOSE — knobs and faders (CC) only |
| GLOBAL | MASTER, TEMPO, SWING, the MASTER FILTER (FREQ RES DRIVE MIX MORPH), DELAY FEEDBACK / COLOR, REVERB SIZE / DAMP; last row CLEAR ALL |
| ACTIONS | a key or button of the FM-1, held as long as the controller's button or pad: MUTE 1-3, DICE 1-3, CHAOS, CIRCLE 1-3, CHAOS TOTAL, DENSITY, SHIFT, LOCK, the fills, TRIGGER, SONG A B C, PLAY / STOP, REC, OCT− / OCT+, ARP |

Good to know: a link listens to its own channel and number only. One control moves one thing — learning it again moves the
link. A setting jumps to the controller's value, so set encoders to **absolute 0-127**, not relative. Links are kept after
power-off, for every scene. CCs not linked keep the fixed map.

## 14. SYSTEM

**HOME held** opens SYSTEM; ALGORITHM chooses the tab, SELECT the row, KNOB 1 changes it.

- **SETUP**: THEME (NIGHT / DAY), SMOOTH (the gold hand turns smoothly and the hits swell as they play), CLOCK, MIDI IN,
  TRS SEEN / USB SEEN (the last MIDI each jack received: handy to check a cable).
- **LEARN**: MIDI learn (above).
- **AUDIO**: CLICK, CLICK LEVEL, SPEAKER EQ, USB LEVEL, TUNE, 2ND CORE (uses the FM-1's second processor core for more
  headroom; from the next power-on).
- **RANDOM** (OCT− + HOME): which engines and envelope modes the random may use (at least one stays on); which effects CHAOS TOTAL may draw (ALL FX, DIST INSERT, DELAY, REVERB). CHAOS switches every effect off.
- **ABOUT**: the version, the build date, the credits and licences.

## 15. The companion page

<https://forban5929.github.io/Alter-FM1/editor/> in **Chrome or Edge** on a computer, with the FM-1 on USB.

- Every setting on a big screen: sounds, rhythms, modulation, MIXER (a mixing desk for the three circles), effects, song.
  What you change on the page moves on the FM-1, and the other way round.
- **Patch files**: a whole scene saved as a file on your computer; sent back, it lands in the first empty USER slot.
- **USB audio**: listen to the FM-1 on the computer, record a WAV, a level meter.
- The page fits its window (AUTO size).

## 16. Combos and safety

| Gesture | What it does |
| --- | --- |
| CIRCLE 1 + 2 + 3 | CHAOS TOTAL (playing: a glitch, then on the 1 after a whole bar; again while it waits: cancelled; within 3 s of the draw: undone) |
| DICE 1 + 2 + 3 | CHAOS (playing: a glitch, then on the 1 after a whole bar) |
| OCT− + OCT+ + DICE n | circle n's LOCKS |
| OCT− + OCT+ + HOME | NEW SCENE (blank start) |
| OCT− + HOME | SYSTEM › RANDOM (what the random may draw: engines, modes, CHAOS TOTAL's effects) |
| OCT− + OCT+ + REC | every circle's REC moves cleared |
| REC held + TRIGGER | a hit added to the chosen circle's pattern |
| MUTE 1 + 2 + 3 | the three mutes (nothing more) |
| **OCT− + OCT+ held 5 s, alone** | **UPDATE mode** (USB update; the FM-1 stops playing). Not while another key or button is held with them |
| **OCT− + OCT+ held at power-on** | the buttons' calibration |

## 17. Updating, going back, rescue

- **Install / update**: <https://forban5929.github.io/Alter-FM1/install/> in Chrome or Edge, FM-1 on USB.
- **Back to M-VAVE's firmware**: *Back to official V15* on [The Firmware Hub](https://thefirmwarehub.com/).
- **If an update went wrong**: OCT− + OCT+ held 5 s puts the FM-1 in UPDATE mode; the Windows rescue kit linked from the
  install page writes ALTER again.
- **UNKNOWN FLASH**: if ALTER shows this at power-on with a code (e.g. `0x123456`), your FM-1 has a memory chip ALTER does
  not know yet. It plays, but nothing is saved after power-off and the USB update is off. Use UPDATE mode to install another
  firmware, and please [report the code](https://github.com/Forban5929/Alter-FM1/issues).

---

ALTER is free software (GPL-3.0-only), built on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita. Not
affiliated with M-VAVE, Hügelton Instruments or Noise Engineering.
