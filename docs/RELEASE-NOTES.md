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

**Install and safety**
- Install from <https://forban5929.github.io/Alter-FM1/install/>; back to M-VAVE's firmware with *Back to official V15*
  on The Firmware Hub; a Windows rescue kit for a failed update.
- Known flash chips: `0x856014` and `0xB36014`. Another chip is reported on screen (UNKNOWN FLASH + its ID): ALTER plays,
  but does not save and does not take USB updates.

**Known limits** (to fill in from the test list)
- …

**Files**: `alter-1.0.fwsc` (the package the install page writes), `alter-1.0-app.bin`, `SHA256SUMS`, the licences.

---

Test versions before 1.0 (v0.41 … v0.47) were published only on the project's site, for testing.
