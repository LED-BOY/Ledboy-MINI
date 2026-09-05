# LedboyAdventures 1.4

Advanced gameplay release for LEDBOY MINI / ATtiny416. Version 1.3 remains
unchanged in its own folder.

## New gameplay

- The dotted grass/ground texture has been removed for a cleaner playfield;
  the trees remain.
- Player, ground-enemy, flying-enemy, shield-orb, and shield-hit movement now
  explicitly erase the previous coordinates so no ghost trails remain.
- Two hazard heights:
  - Ground invaders are avoided with a normal jump and award 2 points.
  - Mirrored flying invaders occupy the middle row, can be cleared by a normal
    jump, and award 3 points.
- Enemy types change during the run instead of following one fixed pattern.
- Respawn positions vary between columns 112 and 119, creating less predictable
  obstacle spacing.
- Shield orbs begin appearing at 12 points and then every 15 points.
- Collecting an orb during a jump awards 5 points and arms a shield.
- The battery-shaped icon at the upper-left indicates an active shield.
- A shield absorbs one collision, resets that hazard, and then disappears.
- Existing progressive difficulty remains: hazards accelerate at scores 20 and
  60, with faster jump animation at the highest tier.
- Current and persistent best-score bars remain on the game-over screen.

## Strategy

Any active jump now clears both ground and flying enemies. Shield orbs still
reward an earlier, higher jump because they travel on the upper row. Saving a
shield lets one mistimed jump continue the run. The score is capped at 117 so
every display write remains inside the OLED.

## Controls

- Press on the title screen to start.
- Press once to jump; release to arm the next jump.
- Shake during gameplay to toggle sound.
- Hold the button for three seconds at power-on to erase the high score.

## Verified build

Compiled with ATtiny416, megaTinyCore 2.6.10, 5 MHz internal clock,
millis/micros disabled, and TinyI2C 2.0.1:

- Version 1.3: 3583 / 4096 bytes flash; 16 / 256 bytes global RAM
- Version 1.4: 3965 / 4096 bytes flash; 15 / 256 bytes global RAM
- Remaining: 131 bytes flash and 241 bytes RAM for locals/stack

Use the same programmer and pinout settings as the working earlier releases.
