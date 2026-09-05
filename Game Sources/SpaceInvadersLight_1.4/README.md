# SpaceInvadersLight 1.4

An optimized maintenance release for the LEDBOY MINI / ATtiny416. Version 1.3
is not modified; this directory is a separate Arduino sketch.

## Changes from 1.3

- Fixes the level-20 soft lock. Clearing the final wave now starts a new run.
- Adds a wave-clear score bonus equal to the completed level.
- Replaces the six-byte enemy-death array and nested search with a compact bitmask.
- Makes mothership spawning elapsed-time based, so a missed exact timer value no
  longer prevents it from appearing.
- Makes enemy firing less dependent on exact timer alignment.
- Counts hits on the left edge of enemies, the mothership, and the player.
- Treats erased or corrupt high-score EEPROM data as zero.
- Prevents the boot-time high-score reset from repeatedly writing EEPROM while
  the button remains held.
- Stores the sound/mute preference and avoids writing it when unchanged.
- Uses explicit, overflow-safe battery percentage calculation and guards invalid
  low-voltage operation.
- Keeps the original controls, display layout, pinout, sprites, sleep behavior,
  and gameplay feel.

## Controls

- Tap the button on the intro screen to start.
- Hold the button to move right; release it to move left.
- The ship fires automatically.
- Shake the unit during gameplay to toggle sound.
- Hold the button for four seconds while powering on to clear the high score.

## Arduino build settings

The 1.3 build artifacts identify the original target as an ATtiny416 using
megaTinyCore 2.6.10 at 5 MHz. Keep both TCA0 and TCB0 millis disabled because the
sketch owns TCB0 for its 1 ms game timer. The TinyI2C library is also required.

Recommended settings:

- Board: ATtiny416 (megaTinyCore)
- Clock: 5 MHz
- millis()/micros(): disabled (TCA0 and TCB0)
- Programmer/pinout: same settings used for the working 1.3 build

Compile before flashing and confirm the result remains within the ATtiny416's
4 KB flash limit.

## Verified build

Compiled successfully with megaTinyCore 2.6.10, TinyI2C 2.0.1, ATtiny416,
5 MHz internal clock, and millis/micros disabled:

- Flash: 4080 / 4096 bytes (99%)
- Global RAM: 13 / 256 bytes (5%)

For comparison, the unchanged 1.3 source compiles to 4096 / 4096 bytes and
uses 14 bytes of global RAM with the same toolchain and settings.
