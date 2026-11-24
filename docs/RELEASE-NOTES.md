# ALTER — release notes

## v1.1 (24 November 2026)

Install it over v1.01 as usual: your scenes, SONG slots, MIDI learn links and settings stay (the update only writes the
firmware; checked in the code and by automatic tests at every build).

**Sound**
- A new engine, **WAVE**: industrial wavetables (CLANG, RUST, SHRED, FEEDBACK) with a Serum-style WARP (SYNC, BEND, FM,
  RING, FOLD, BITS), envelopes on the position and the warp, SUB and SPREAD. Seven new sounds use it.
- Every knob of every engine was measured from 0 to 127: about twenty jumps and dead zones fixed (RING MATR, PHYS BOW,
  BRIT, DAMP and EXC, PLATE MATR and HIT, NOISE DENS, CORDE BUZZ, RESO DIRT, ALTER TILT and NOIS, RING RDIO = an AM radio…).
- Engine knobs that did almost nothing (DIRT, ALTER NOIS, PHYS EXC, NOISE DRFT) became a RING MOD or a wave FOLD,
  depending on the engine; PLATE keeps DIRT, and RING, PLATE and CORDE get a new DIRT output stage.
- Factory sounds balanced by family (kicks and big sounds in front, hats and textures behind), louder engines.
- The KICK's RUMBLE redone: a dark room that pumps with the kick, continuous from LOW to HIGH.
- Reverb presets: SHORT, BOX, CAN, PIPE, METAL, SHARDS, HALL, BRIGHT HALL, CLOUD. Factory sounds are now dry.
- DIST: DRIVE fades in smoothly (no more on/off jump); INSERT = FOLD or CRUSH (CRUSH goes from clean to 2 bits).
- TRIGGER plays through the envelope, with the velocity of the next step.

**Pages and controls**
- FX page in three tabs: REVERB, DELAY, DIST (DRIVE, INS, TYPE, TONE). MASTER and FILTER moved to the MIXER (GLO held).
- SEQ page: VELO, PITCH, CHNC, DLY, REV. The ratchet was removed.
- **16 USER sounds**: save a circle's sound with a 4-letter name (SAVE page, SOUNDS tab); they appear after the factory
  sounds in PRESETS.
- **MIDI OUT over USB** (SYSTEM › SETUP › MIDI OUT: OFF, CLOCK, NOTES, ALL; OFF by default): clock when CLOCK = INT,
  the three circles' notes on channels 1, 2 and 3.

**Companion page**
- **Backup… / Restore…**: every scene, SONG, MIDI learn, settings and USER sounds in one file; choose what to restore.
- **Store…**: save a circle's sound into a USER slot from the page.
- **Feedback**: a button to send a problem or an idea, no account needed.
- **JAM**: the FM-1's PLAY screen redrawn full screen, to film a live set.

**Known limits**
- Scenes saved before v1.1 that used CRUSH, SOFT/HARD/FUZZ inserts, the ratchet or the old DIRT/NOIS/EXC/DRFT knobs
  sound a little different now.
- New in this version and tested in the emulator and the companion page, not yet on a real FM-1: the WAVE engine's CPU
  load, Backup/Restore, USER sounds, MIDI OUT (with a DAW).

**Files**: `alter-1.1.fwsc` (the package the install page writes), `alter-1.1-app.bin`, `SHA256SUMS`, the licences.

## v1.01 (22 November 2026)

- New package identity `FM-1_956`: the number used by v0.79 and v1.0 (`FM-1_929`) already belongs to another firmware
  on The Firmware Hub. Nothing else changes: same sound, same scenes, same settings. Install it over v1.0 as usual.

## v1.0 (21 November 2026)

The first release of ALTER, alternative firmware for the M-VAVE FM-1, built on Felucca.

**The instrument**
- Three euclidean circles (or TURING machines), each with its own sound, envelope, effects and mixer channel.
- Seven engines: ALTER, PHYS, NOISE, RESO, PLATE, CORDE, RING.
- Dice per circle (pattern; velocity, chance, length; pitch and throws), locks, CHAOS and CHAOS TOTAL (undoable).
- Fills while held (DUB DELAY, DUB REVERB, DESTROY, ROLL 1/16, ROLL 1/8), TRIGGER, REC of knob moves.
- Scales and chord progressions (PROG), three LFOs and a modulation matrix.
- Per-circle CONSOLE (drive, 3-band EQ with kill, compressor, sends), MASTER FILTER, delay with TAPE mode, reverb.
- SONG: three rhythm slots A B C and a chain of up to 16 links.
- 48 named scenes in three banks; a scene brings back everything, including what the dice drew.
- MIDI over USB and TRS, clock in, fixed CC map and MIDI learn.
- Companion page in Chrome / Edge: every setting, the mixer, patch files, USB audio.
- Optional second processor core for more headroom.

**Live safety** (new since the first test versions)
- Measured CPU on the FM-1: the heaviest random scenes (three circles 16/16, every effect, master filter, LFOs) peak at
  60-79 %, with no audio gaps (the hot sound code runs from RAM; delay, chorus and reverb run on the second core).
- A safety net: when a moment gets too heavy, the screen slows down first, then old note tails end sooner, then the master
  filter simplifies, all before Felucca's last resort (dropping a voice), which the tests never reached.
- If the FM-1 ever crashes, it restarts by itself in about 2 s with the scene that was playing, on one core until the next
  power-off, and SYSTEM › ABOUT says what happened (LAST CRASH).
- CPU METER (SYSTEM › SETUP): the load lines on screen, off by default (and nothing measured while off).

**Performance details**
- CHAOS / CHAOS TOTAL while playing land on the next bar but one, with a full-screen glitch; pressing again cancels.
- CHAOS keeps each circle's LOCKS, switches between EUCLID and TURING (one TURING at most), and clears effects and LFOs;
  OCT− + HOME opens RANDOM: which engines, modes and effects CHAOS TOTAL may draw.
- DENSITY − removes the dice's extra hits and recorded triggers first; HITS shows what plays.
- A power-on animation (~1.5 s, any key skips it) and a breathing seal before the screen sleeps.

**Install and safety**
- Install from <https://forban5929.github.io/Alter-FM1/install/>; back to M-VAVE's firmware with *Back to official V15*
  on The Firmware Hub; a Windows rescue kit for a failed update.
- Package identity `FM-1_929` (from v1.01: `FM-1_956`; test versions before v0.79 used Felucca's `FM-1_900`). The package
  carries the update loader, so ALTER can be removed again from the install page or the Hub.
- Known flash chips: `0x856014` and `0xB36014`. Another chip is reported on screen (UNKNOWN FLASH + its ID): ALTER plays,
  but does not save and does not take USB updates.

**Known limits**
- Tested on one FM-1 with the UC25HQ80 flash (`0xB36014`); the Puya P25Q80 (`0x856014`) is supported as in Felucca, not
  tested by us.
- MIDI learn: tested in the emulator and the companion page, not yet with a hardware controller.
- MIDI clock over TRS: not verified yet on hardware (a cable problem during the tests); USB clock and notes work.
- The companion page and the installer need Chrome or Edge (Web MIDI / Web Serial); the rescue kit is Windows only.

**Files**: `alter-1.0.fwsc` (the package the install page writes), `alter-1.0-app.bin`, `SHA256SUMS`, the licences.

---

Test versions before 1.0 (v0.41 … v0.7x) were published only on the project's site, for testing.
