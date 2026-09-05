# LEDBOY MINI Watch 3.1

Improved ATtiny416 watch firmware. Version 3.0 remains unchanged in its own
folder.

## Reset confirmation

Selecting the reset icon now opens a confirmation screen with the reset icon
and a large number:

- `0` = cancel; do not reset the watch
- `1` = confirm; reset the watch

The screen starts at `0`. Press the button to toggle between `0` and `1`, then
leave it untouched for two seconds to accept the displayed choice. This uses the
same familiar interaction as the watch's other numeric options.

## Additional improvements

- Removed the misleading reset logo and 1.5-second delay shown on every normal
  startup. The watch now starts directly on its normal display.
- A button press used to dismiss a finished minute timer no longer also opens
  the options menu.
- Setting the minute timer to zero now fully resets its counters, LED, and sound
  output instead of only clearing its active flag.
- Added protection against division by zero if the ADC ever returns an invalid
  zero reading.
- Clears a pending RTC tick before re-enabling the RTC interrupt after setting
  the time, preventing an immediate unexpected one-second increment.
- Simplified the battery bar, LED color selector, option positioning, and digit
  drawing to recover enough flash for confirmation and improve maintainability.

## Verified build settings

- Board/core: ATtiny416 with megaTinyCore 2.6.10
- Clock: 5 MHz internal
- millis()/micros(): disabled
- `printf()`: **Default**
- Library: TinyI2C 2.0.1

The `printf: Minimal` setting must not be used for this firmware. The sketch
does not call `printf`, so the default implementation is removed by the linker.
Selecting Minimal forces its approximately 1.1 KB formatter into the image and
overflows the ATtiny416 flash.

Verified default-printf build:

- Flash: 3987 / 4096 bytes (97%)
- Global RAM: 26 / 256 bytes (10%)
- Remaining: 109 bytes flash and 230 bytes RAM for locals/stack

Use the same programmer, UPDI, BOD, and pinout settings as the working 3.0
release.
