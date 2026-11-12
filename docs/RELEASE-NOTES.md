# ALTER — release notes

## v1.0 (draft — not released yet: waiting for the tests on real FM-1s)

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
- Known flash chips: `0x856014` and `0xB36014`. Another chip is reported on screen (UNKNOWN FLASH + its ID): ALTER plays,
  but does not save and does not take USB updates.

**Known limits**
- Tested on one FM-1 with the UC25HQ80 flash (`0xB36014`); the Puya P25Q80 (`0x856014`) is supported as in Felucca, not
  tested by us.
- MIDI clock over TRS: not verified yet on hardware (a cable problem during the tests); USB clock and notes work.
- The companion page and the installer need Chrome or Edge (Web MIDI / Web Serial); the rescue kit is Windows only.

**Files**: `alter-1.0.fwsc` (the package the install page writes), `alter-1.0-app.bin`, `SHA256SUMS`, the licences.

---

Test versions before 1.0 (v0.41 … v0.7x) were published only on the project's site, for testing.
