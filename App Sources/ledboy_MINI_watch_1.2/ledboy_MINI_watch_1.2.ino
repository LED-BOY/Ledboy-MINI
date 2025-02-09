
/*Game&Light  Clock for GAME&LIGHT and any Attiny series 1.
  Flash CPU Speed 8MHz.
  this code is released under GPL v3, you are free to use and modify.
  released on 2023.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    To contact us: ledboy.net
    ledboyconsole@gmail.com
*/
#include "tinyOLED.h"
#include "sprites.h"
#include "digits.h"
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define SCREEN_ON  PORTC.OUT &= ~PIN0_bm;// P CHANNEL mosfet low to activate
#define SCREEN_OFF PORTC.OUT |= PIN0_bm;// P CHANNEL mosfet high to deactivate
#define BUZZER_OFF PORTC.OUT &= ~PIN1_bm;// buzzer off
#define BUTTONLOW !(PORTC.IN & PIN3_bm)// button press low
#define BUTTONHIGH (PORTC.IN & PIN3_bm)// button not pressed high
#define BATTCHR !(PORTA.IN & PIN4_bm)// battery charging
#define MAXVOLTAGE 4100 // max voltage allowed to the battery
#define MINVOLTAGE 3200 // min voltage allowed to be operational
#define LOWBATT 140 // min voltage + 140 mha is low batt indication

uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t lastSecond = 0;
uint16_t totalSeconds = 0;
volatile bool shouldWakeUp = true;
volatile bool resumeOperation = true;
volatile uint8_t mode = 0;
volatile uint8_t ledColor = 0;
volatile uint8_t interruptDebounce = 0;
volatile uint8_t interruptSensitivity = 0;
volatile uint16_t interruptTimer = 0;
volatile uint16_t timer = 0;

// Function Prototypes
void setup();
void loop();
void displayTime(uint8_t hourPos, uint8_t minutesPos);
uint8_t displayBatt();
void ledON(bool optionSelected);
void countdownTimer();
void wakeUpFromSleep(bool activateScreen);
void options();
uint8_t setNumber(uint8_t sprite, uint8_t base, uint8_t total);
void setTime();
void drawTime(uint8_t firstPixel, uint8_t page, uint8_t digitHeight, int8_t value);
void drawSprite(uint8_t column, uint8_t page, uint8_t *sprite, uint8_t SpritePixelsHeight, uint8_t SpritePagesWidth);
bool buttonDebounce(uint16_t debounceDelay);
void goToSleep();
int32_t readSupplyVoltage();

// Interrupt Service Routines
ISR(PORTA_PORT_vect);
ISR(PORTC_PORT_vect);
ISR(TCA0_OVF_vect);
ISR(TCB0_INT_vect);
ISR(RTC_PIT_vect);

void setup() {

  PORTA.DIR = 0b00000000; // setup ports in and out //  pin2 (GREEN) on
  PORTB.DIR = 0b00000011; // setup ports in and out
  PORTC.DIR = 0b00000011; // setup ports in and out
  PORTC.PIN2CTRL = PORT_PULLUPEN_bm;// sensor pullup
  PORTC.PIN3CTRL = PORT_PULLUPEN_bm;// button pullup
  PORTA.PIN4CTRL = PORT_PULLUPEN_bm;// charge pin pullup
  PORTA.PIN4CTRL |= PORT_ISC_BOTHEDGES_gc; //attach interrupt to portA pin 4 keeps pull up enabled
  BUZZER_OFF

  CCP = 0xD8;
  CLKCTRL.XOSC32KCTRLA = CLKCTRL_ENABLE_bm;   //Enable the external crystal
  while (RTC.STATUS > 0); /* Wait for all register to be synchronized */
  RTC.CTRLA = RTC_PRESCALER_DIV1_gc   /* 1 */
              | 0 << RTC_RTCEN_bp     /* Enable: disabled */
              | 1 << RTC_RUNSTDBY_bp; /* Run In Standby: enabled */
  RTC.CLKSEL = RTC_CLKSEL_TOSC32K_gc; /* 32.768kHz External Crystal Oscillator (XOSC32K) */
  while (RTC.PITSTATUS > 0) {} /* Wait for all register to be synchronized */
  RTC.PITCTRLA = RTC_PERIOD_CYC32768_gc /* RTC Clock Cycles 32768 */
                 | 1 << RTC_PITEN_bp;   /* Enable: enabled */
  RTC.PITINTCTRL = 1 << RTC_PI_bp; /* Periodic Interrupt: enabled */

  TCB0.INTCTRL = TCB_CAPT_bm; // Setup Timer B as compare capture mode to trigger interrupt
  TCB0_CCMP = 2000;// CLK
  TCB0_CTRLA = (1 << 1) | TCB_ENABLE_bm;

  //sei();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);// configure sleep mode
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);// enable watchdog 4 sec. aprox.
}

void loop() {
  // Restart display after wake-up
  wakeUpFromSleep(resumeOperation);

  // Handle countdown timer logic
  countdownTimer();

  // Check for button press
  if (buttonDebounce(300)) {
    // Enter options menu only if the screen is not dimmed and battery is above 0
    if (!oled.screenDimmed() && displayBatt() != 0) {
      options(); // Enter options menu
    } else {
      oled.brightScreen(); // Brighten the screen
    }
  }

  // Prepare to sleep or dim the screen after inactivity
  if ((interruptTimer - timer) > 4000) {
    timer = interruptTimer; // Reset the inactivity timer
    oled.dimScreen(); // Dim the screen
    oled.clearScreen(); // Clear the screen

    // If the countdown timer is off, prepare to sleep
    if (totalSeconds == 0) {

      // Display charging sprite if charging, otherwise turn off the screen
      if (BATTCHR) {
        drawSprite(0, 3, chargingSprite, 8, 1);
      } else {
        SCREEN_OFF; // Turn off the screen
      }

      goToSleep(); // Enter sleep mode
    }
  }
}

void displayTime(uint8_t hourPos, uint8_t minutesPos) { // draws time in display
  drawTime(hourPos, 1, 24, hours);
  drawTime(minutesPos, 1, 24, minutes);
}

uint8_t displayBatt(void) {
  // Read battery voltage and constrain it to the valid range
  uint16_t actualVoltage = constrain(readSupplyVoltage(), MINVOLTAGE, MAXVOLTAGE);

  // Map the voltage to a pixel range for the battery bar
  uint8_t batteryBarLength = map(actualVoltage, MINVOLTAGE, MAXVOLTAGE, 0, 117);

  // Draw the battery bar
  for (uint8_t x = 224; x > 159; x -= 64) {
    oled.drawLine(10, 3, batteryBarLength, x); // Draw the battery level bar

    // Limit the battery bar length for low battery indication
    if (batteryBarLength > 20) {
      batteryBarLength = 20; // Display low battery from pixel 0 to 20
    }
  }

  // Draw the battery or charging icon
  if (BATTCHR) {
    drawSprite(0, 3, chargingSprite, 8, 1); // Draw charging icon
  } else {
    drawSprite(0, 3, battSprite, 8, 1); // Draw battery icon
  }

  return batteryBarLength; // Return the battery bar length for further use
}

void ledON(bool optionSelected) {
  // Turn LEDs on or off based on user input
  while ((interruptTimer - timer) < 2000) {
    // Check for button press or option selection
    if (buttonDebounce(250) || optionSelected) {
      timer = interruptTimer; // Reset the timer
      optionSelected = false; // Reset the option selection flag

      // Cycle through LED colors
      ledColor = (ledColor + 1) % 3; // Increment and wrap around after 2

      // Set the LED color based on the current state
      switch (ledColor) {
        case 0:
          PORTA.DIR = 0b00000000; // Turn off all LEDs
          break;
        case 1:
          PORTA.DIR = 0b10000000; // Turn on RED LED (pin 2)
          break;
        case 2:
          PORTA.DIR = 0b01100000; // Turn on GREEN and BLUE LEDs (pins 1 and 0)
          break;
      }
    }
  }
}

// Countdown Timer Logic
void countdownTimer() {
  if (totalSeconds == 0) return; // Exit if no timer is active

  if (seconds != lastSecond) {
    lastSecond = seconds;

    if (oled.screenDimmed()) {
      timer = interruptTimer;
      drawTime(0, 1, 24, (totalSeconds  / 60) + 1);
    } else {
      displayBatt();
      displayTime(64, 0);
    }

    if (--totalSeconds < 1) { // Decrement timer ends
      drawTime(0, 1, 24, 0);
      PORTA.DIR = (1 << 2);
      while (BUTTONHIGH && (interruptTimer - timer) < 8000); // Wait for button press
    }

    if (PORTA.DIR != 0b00000000) { // Turn off LEDs if timer ends
      ledColor = 0;
      PORTA.DIR = 0b00000000;
    }
  }
}

void wakeUpFromSleep(bool activateScreen) {
  // Exit if screen activation is not required or voltage is too low
  if (!activateScreen || readSupplyVoltage() <= MINVOLTAGE) {
    return;
  }

  // Reset debounce and resume operation flag
  resumeOperation = false;
  interruptDebounce = 0;

  // Wait for a short period before initializing the screen
  while ((interruptTimer - timer) < 200);

  // Initialize and clear the OLED screen
  oled.begin();
  oled.clearScreen();

  // Disable watchdog timer
  _PROTECTED_WRITE(WDT.CTRLA, 0);

  // Handle different display modes
  switch (mode) {
    case 0:
      oled.brightScreen();
      displayTime(64, 0);
      displayBatt();
      break;
    case 1:
      oled.dimScreen();
      displayTime(76, 8);
      break;
  }

  // Debounce button input
  buttonDebounce(1000);
}

void options (void) { // options menu
  int8_t option = 0;
  const uint8_t spriteOptionPos [] = {100, 70, 40, 10};
  bool setOptions = true;
  oled.clearScreen();

  for (uint8_t x = 0; x < 4; x++) {
    drawSprite(spriteOptionPos [x], 1, optionSprites[x], 16, 2);
    option++;
  }
  oled.drawLine(10, 3, interruptSensitivity * 4, 0B11110000);
  option = 0;

  while (setOptions) {

    if (buttonDebounce(250) || option == 0) {
      oled.drawLine(0, 0, 127, 0B00000000);
      drawSprite((spriteOptionPos [option]) + 4, 0, arrow, 8, 1);
      if (option++, option > 4) setOptions = false;
    }

    if ((interruptTimer - timer) > 2000) {
      timer = interruptTimer;
      switch (option) {
        case 1: ledON(true); break; // LED control
        case 2: totalSeconds = (setNumber((option - 1), 5, 50) * 60); break; // Set timer
        case 3: setTime(); break; // Set time
        case 4: interruptSensitivity = setNumber((option - 1), 1, 4) * 5; break;// Set interrupt sensitivity to one of three states: 0(off), 1(hi), 2(med), 3(low)
      }
      setOptions = false;
    }
  }
  resumeOperation = true;
}

uint8_t setNumber(uint8_t sprite, uint8_t base, uint8_t total) {
  // Clear the screen and draw the initial UI
  oled.clearScreen();
  drawSprite(90, 1, optionSprites[sprite], 16, 2); // Draw the sprite
  drawTime(0, 1, 24, 0); // Draw the initial value (0)

  uint8_t result = 0; // Variable to store the result
  bool setCurrentNumber = true; // Control flag for the loop

  while (setCurrentNumber) {
    // Check if the button is pressed
    if (BUTTONLOW) {
      timer = interruptTimer; // Reset the timer

      // Increment the result by the base value and wrap around if necessary
      result = (result + base) % total;
      drawTime(0, 1, 24, result); // Update the displayed value
      buttonDebounce(300); // Debounce the button press
    }

    // Exit the loop if no button press is detected for 2 seconds
    if ((interruptTimer - timer) > 2000) {
      setCurrentNumber = false;
    }
  }

  return result; // Return the final value
}

void setTime() {
  uint8_t arrowPos = 94; // Initial position of the arrow

  // Loop through hours, minutes, and seconds
  for (uint8_t x = 0; x < 3; x++) {
    bool setCurrentTime = true;
    oled.clearScreen(); // Clear the screen
    drawSprite(arrowPos, 0, arrow, 8, 1); // Draw the arrow

    // Display the current time component
    if (x < 2) {
      drawTime(76, 1, 24, hours); // Display hours
      drawTime(24, 1, 24, minutes); // Display minutes
    } else {
      drawTime(0, 1, 24, seconds); // Display seconds
    }

    // Handle time setting for the current component
    while (setCurrentTime) {
      // Check for button press and debounce
      if ((interruptTimer - timer) > 250 && BUTTONLOW) {
        timer = interruptTimer; // Reset the timer

        // Increment the current time component
        switch (x) {
          case 0: hours = (hours + 1) % 24; break; // Increment hours
          case 1: minutes = (minutes + 1) % 60; break; // Increment minutes
          case 2: seconds = (seconds + 1) % 60; break; // Increment seconds
        }

        // Update the display for the current time component
        uint8_t displayX = (x == 0) ? 76 : (x == 1) ? 24 : 0;
        uint8_t displayValue = (x == 0) ? hours : (x == 1) ? minutes : seconds;
        drawTime(displayX, 1, 24, displayValue);
      }

      // Exit after 2.5 seconds of inactivity
      if ((interruptTimer - timer) > 2500) {
        setCurrentTime = false; // Exit the loop
        timer = interruptTimer; // Reset the timer
        arrowPos /= 2; // Move the arrow to the next position
      }
    }
  }
}

void drawTime(uint8_t firstPixel, uint8_t page, uint8_t digitHeight, int8_t value) {
  // Ensure the value is positive
  if (value < 0) {
    value = -value; // Convert negative value to positive
  }

  // Always draw 2 digits (e.g., 01)
  if (value < 10) {
    // Clear the area for the first digit if at specific positions
    if (firstPixel == 64 || firstPixel == 76) {
      for (uint8_t x = 1; x < 3; x++) {
        oled.drawLine(100, x, 32, 0B00000000); // Clear line
      }
    } else {
      // Draw '0' as the first digit
      drawSprite(firstPixel + (digitHeight + 2), page, numbers[0], 24, 2);
    }
    // Draw the second digit
    drawSprite(firstPixel, page, numbers[value], 24, 2);
  } else {
    // Draw the first digit (tens place)
    drawSprite(firstPixel + (digitHeight + 2), page, numbers[value / 10], 24, 2);
    // Draw the second digit (units place)
    drawSprite(firstPixel, page, numbers[value % 10], 24, 2);
  }
}

// Draw Sprite on OLED
void drawSprite(uint8_t column, uint8_t page, const uint8_t* sprite, uint8_t spritePixelsHeight, uint8_t spritePagesWidth) {
  uint8_t spriteIndex = 0; // Index to track position in the sprite array

  for (uint8_t y = 0; y < spritePagesWidth; y++) {
    // Set the cursor to the correct column and page
    oled.setCursor(column, page + y);

    // Start sending data to the OLED
    oled.ssd1306_send_data_start();

    // Send a row of sprite data
    for (uint8_t x = 0; x < spritePixelsHeight; x++) {
      if (spriteIndex >= 128) break; // Prevent out-of-bounds access
      oled.ssd1306_send_data_byte(sprite[spriteIndex++]);
    }

    // Stop sending data to the OLED
    oled.ssd1306_send_data_stop();
  }
}

bool buttonDebounce(uint16_t debounceDelay) {
  // Exit if the button is not pressed
  if (BUTTONHIGH) {
    return false;
  }

  // Record the current timer value
  timer = interruptTimer;

  // Debounce loop
  while (true) {
    // Check if the debounce delay has passed and the button is released
    if ((interruptTimer - timer) >= debounceDelay && BUTTONHIGH) {
      break;
    }

    // Reset the device if the timeout is reached
    if ((interruptTimer - timer) > 6000) {
      //SCREEN_OFF; // Turn off the screen
      _PROTECTED_WRITE(RSTCTRL.SWRR, 1); // Reset the device
    }
  }

  return true; // Button press successfully debounced
}

void enableTca0() {
  TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm; // configure TCA as millis counter
  TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc; // set Normal mode
  TCA0.SINGLE.EVCTRL &= ~(TCA_SINGLE_CNTEI_bm);// disable event counting
  TCA0.SINGLE.PER = 50; // aprox 1ms, set the period(1000000/PER*DESIRED_HZ - 1)
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc // set clock  source (sys_clk/64)
                      | TCA_SINGLE_ENABLE_bm;
}

void disableTca0(void) {
  TCA0.SINGLE.CTRLA = 0; //disable TCA0 and set divider to 1
  TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESET_gc | 0x03; //set CMD to RESET to do a hard reset of TCA0.
  BUZZER_OFF
}

void goToSleep(void) {
  // Enable sensor interrupt if conditions are met
  if (!BATTCHR && readSupplyVoltage() > (MINVOLTAGE + LOWBATT) && interruptSensitivity != 0) {
    PORTC.PIN2CTRL |= PORT_ISC_FALLING_gc; // Attach interrupt to PORTC pin 2 (sensor)
  }

  // Enable button interrupt on both edges
  PORTC.PIN3CTRL |= PORT_ISC_BOTHEDGES_gc; // Attach interrupt to PORTC pin 3 (button)

  // Disable ADC to save power
  ADC0.CTRLA = 0;

  // Prepare for sleep
  shouldWakeUp = false; // Reset wake-up flag
  sleep_enable(); // Enable sleep mode
  sleep_cpu(); // Enter sleep mode
}

// Read Supply Voltage (in millivolts)
int32_t readSupplyVoltage() {
  ADC0.CTRLA = ADC_ENABLE_bm; // Enable ADC
  analogReference(VDD); // Set reference voltage to VDD
  VREF.CTRLA = VREF_ADC0REFSEL_1V5_gc; // Set internal reference to 1.5V

  // Take two readings to ensure accuracy (first reading may be invalid)
  uint32_t reading = analogRead(ADC_INTREF);
  reading = analogRead(ADC_INTREF);

  // Convert ADC reading to millivolts
  uint32_t intermediate = 1023UL * 1500; // 1023 * 1.5V reference
  reading = intermediate / reading; // Calculate voltage in mV
  return reading;
}

ISR(PORTA_PORT_vect) {
  // Disable sensor interrupt and clear flags
  PORTC.PIN2CTRL &= ~PORT_ISC_gm; // Disable sensor interrupt
  VPORTC.INTFLAGS = (1 << 2);    // Clear PORTC interrupt flag
  VPORTA.INTFLAGS = (1 << 4);    // Clear PORTA interrupt flag

  // Wake up system and reset state
  shouldWakeUp = true;
  resumeOperation = true; // Reset screen after sleep if "always on" is off
  timer = interruptTimer;
  mode = 0;
  SCREEN_ON;
  sleep_disable();

  // Enable watchdog timer (4 seconds approx.)
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);
}

ISR(PORTC_PORT_vect) {
  // Handle PIN2 interrupt
  if (PORTC.INTFLAGS & PIN2_bm) {
    VPORTC.INTFLAGS |= (1 << 2); // Clear interrupt flag

    // Debounce logic
    if (interruptDebounce < interruptSensitivity) {
      interruptDebounce++;
      sleep_cpu(); // Sleep until next interrupt
    } else {
      mode = 1;
      resumeOperation = true;
    }
  }

  // Handle PIN3 interrupt
  if (PORTC.INTFLAGS & PIN3_bm) {
    VPORTC.INTFLAGS |= (1 << 3); // Clear interrupt flag
    PORTC.PIN3CTRL &= ~PORT_ISC_gm; // Disable sensor interrupt
    PORTA.DIR = 0b00000000; // Reset LED
    ledColor = 0;
    mode = 0;
    resumeOperation = true;
  }

  // Resume operation if required
  if (resumeOperation) {
    PORTC.PIN2CTRL &= ~PORT_ISC_gm; // Disable sensor interrupt
    SCREEN_ON;
    timer = interruptTimer;
    shouldWakeUp = true;
    sleep_disable();
  }
}

ISR(TCA0_OVF_vect) {
  // Clear overflow interrupt flag
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

  // Toggle PORTC.PIN1
  PORTC.OUTTGL = PIN1_bm;
}

ISR(TCB0_INT_vect) {
  // Clear interrupt flag
  TCB0_INTFLAGS = 1;

  // Increment interrupt timer
  interruptTimer++;
}

ISR(RTC_PIT_vect) {
  // Clear PIT interrupt flag
  RTC.PITINTFLAGS = RTC_PI_bm;

  // Update timekeeping
  if (seconds++, seconds > 59) {
    seconds = 0;

    if (minutes++, minutes > 59) {
      minutes = 0;

      if (hours++, hours > 23) {
        hours = 0;
      }
    }
  }

  // Sleep if no wake-up is required
  if (!shouldWakeUp) {
    sleep_cpu();
  }
}
