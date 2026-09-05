/* LedboyAdventures 1.4 for LEDBOY MINI and compatible ATtiny 0/1/2 series.
  Flash CPU Speed 5MHz.
  this code is released under GPL v3, you are free to use and modify.
  released on 2022.

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
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

#if defined(MILLIS_USE_TIMERA0) || defined(MILLIS_USE_TIMERB0)
#error "This sketch does't use TCA0 nor TCB0 to save flash, select Disabled."
#endif

#define SCREEN_ON PORTC.OUT &= ~PIN0_bm;  // P CHANNEL mosfet low to activate
#define SCREEN_OFF PORTC.OUT |= PIN0_bm;  // P CHANNEL mosfet high to deactivate
#define BUTTONLOW !(PORTC.IN & PIN3_bm)   // button press low
#define BUTTONHIGH (PORTC.IN & PIN3_bm)   // button not pressed high
#define MAXVOLTAGE 4100                   // max voltage allowed to the battery
#define MINVOLTAGE 3200                   // min voltage allowed to be operational
#define BATTCHR !(PORTA.IN & PIN4_bm)     // battery charging
#define EEPROM_SCORE 126
#define ENEMY_COUNT 3
#define MAX_SCORE 117

uint8_t bitShift1 = 8;
uint8_t bitShift2 = 0;
uint8_t jumpLength = 19;
uint16_t timer = 0;
uint16_t timer2 = 0;
uint16_t timer3 = 0;
volatile uint8_t  frameCounter = 0;
volatile uint8_t interruptDebounce = 0;
volatile uint16_t interruptTimer = 0;
volatile bool sound = true;


void setup() {
  PORTA.DIR = 0b00000000;                   // setup ports in and out //  pin2 (GREEN) on
  PORTB.DIR = 0b00000011;                   // setup ports in and out
  PORTC.DIR = 0b00000011;                   // setup ports in and out
  PORTC.PIN3CTRL = PORT_PULLUPEN_bm;        // button pullup
  PORTC.PIN2CTRL = PORT_PULLUPEN_bm;        // sensor pullup
  PORTA.PIN4CTRL = PORT_PULLUPEN_bm;        // charge pin pullup
  PORTA.PIN4CTRL |= PORT_ISC_BOTHEDGES_gc;  //attach interrupt to portA pin 4 keeps pull up enabled

  TCB0.CCMP = 5000;                  // Set compare value for 1ms (5 MHz clock)
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm; // Enable TCB0 with no prescaler
  TCB0.INTCTRL = TCB_CAPT_bm;        // Enable interrupt

  sei();                                             // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);               // configure sleep mode
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);  // enable watchdog 4 sec. aprox.

  // Hold the button during power-up for three seconds to clear the high score.
  while (BUTTONLOW || interruptTimer < 300) {
    if (interruptTimer > 3000) {  // clear HI score before the 4-second watchdog
      writeScoreToEEPROM(EEPROM_SCORE, 0);
      while (BUTTONLOW);
      break;  // never wear EEPROM by writing repeatedly while held
    }
  }
  oled.begin();  // start oled screen
  oled.clearScreen();
  _PROTECTED_WRITE(WDT.CTRLA, 0);  // disable watchdog

  if (BATTCHR) {
    drawSprite(4, 0, battIcon, 0);
    goToSleep();
  }
}

void loop() {
  oled.clearScreen();// screen clear
  intro();
  oled.clearScreen();
  game();
}

// Main game function
void game() {
  uint8_t playerXPos = 16;
  uint8_t playerYPos = 2;
  uint8_t enemiesPos[ENEMY_COUNT] = {46, 80, 120};
  uint8_t score = 0;
  uint8_t backgroundXPos = 0;
  uint8_t flyingEnemies = 0;
  uint8_t bonusXPos = 255;
  uint8_t nextBonus = 12;
  const uint8_t skyElementsPos[8] = {0, 7, 10, 17, 31, 37, 60, 66};
  bool birdState = false;
  bool jump = false;
  bool jumpReady = true;
  bool shield = false;
  bool soundStatus = sound;

  // Wait for button press to start the game
  while (BUTTONLOW);
  timer = timer2 = timer3 = interruptTimer;
  PORTC.PIN2CTRL |= PORT_ISC_RISING_gc;

  // Main game loop
  while (true) {
    if (soundStatus != sound) {
      soundStatus = sound;
      PORTC.PIN2CTRL |= PORT_ISC_RISING_gc;
    }

    // Update game state every 50 milliseconds
    if ((interruptTimer - timer) > 50) {
      timer = interruptTimer;
      uint8_t enemyStep = 1 + (score >= 20) + (score >= 60);

      // Update enemy positions
      for (uint8_t x = 0; x < ENEMY_COUNT; x++) {
        if (enemiesPos[x] <= 120) {
          oled.drawLine(enemiesPos[x], 2, 8, 0x00);
          oled.drawLine(enemiesPos[x], 3, 8, 0x00);
        }
        if (enemiesPos[x] <= enemyStep || enemiesPos[x] > 200) {
          if (enemiesPos[x] <= enemyStep) {
            uint8_t points = (flyingEnemies & _BV(x)) ? 3 : 2;
            score = min((uint8_t)MAX_SCORE, (uint8_t)(score + points));
          }
          enemiesPos[x] = 112 + ((interruptTimer + x) & 0x07);
          if (((score >> 1) + x) & 0x02) flyingEnemies |= _BV(x);
          else flyingEnemies &= ~_BV(x);
        } else {
          enemiesPos[x] -= enemyStep; // Progressive difficulty as score rises
          jumpLength = score >= 60 ? 13 : 19;
        }
      }

      // Shield bonuses enter play at score milestones and travel with the world.
      if (bonusXPos <= 120) {
        oled.drawLine(bonusXPos, 1, 8, 0x00);
        if (bonusXPos <= enemyStep) bonusXPos = 255;
        else bonusXPos -= enemyStep;
      } else if (score >= nextBonus) {
        bonusXPos = 120;
        nextBonus += 15;
      }

      // Update player position during jump
      if (playerYPos < 2) {
        clearPlayerTrail(playerXPos);
        playerXPos++;
      } else if (playerXPos > 16 && playerYPos == 2) {
        clearPlayerTrail(playerXPos);
        playerXPos--;
      }

      // Draw the trees; the dotted grass/ground texture is intentionally absent.
      for (uint8_t x = 0; x < 127; x += 8) {
        bool outsidePlayer = (x + 7 < playerXPos) || (x > playerXPos + 7);
        if (outsidePlayer || playerYPos > 1) {
          drawSprite(x, 1, trees, false);
        }
      }
    }

    // Update background every 100 milliseconds
    if ((interruptTimer - timer2) > 100) {
      timer2 = interruptTimer;
      oled.drawLine(0, 0, 127, 0x00); // Clear sky

      // Scroll background
      backgroundXPos = (backgroundXPos - 2) & 0x7F;

      // Draw sky elements
      bool cloudState = false;
      for (uint8_t x = 0; x < 8; x++) {
        cloudState = !cloudState;
        drawSprite((backgroundXPos + skyElementsPos[x]) & 0x7F, 0, cloud, cloudState);
      }
      drawSprite((backgroundXPos + 48) & 0x7F, 0, bird, birdState);
      drawSprite((backgroundXPos + 90) & 0x7F, 0, bird, birdState);
      if (shield) drawSprite(2, 0, battIcon, false);
    }

    // Handle player jump
    if (BUTTONHIGH) jumpReady = true;
    if (BUTTONLOW && jumpReady && playerYPos == 2) {
      jumpReady = false;
      jump = true;
      playerYPos = 1;
      beep(250, 25);
    }

    // Update player position during jump
    if (playerYPos != 2) {
      if (bitShiftLedBoySprite(playerXPos, playerYPos, jump)) {
        if (playerYPos == 1 && jump) {
          playerYPos = 0;
        } else {
          jump = false;
          playerYPos++;
        }
      }
    }

    // Animate player, enemies, and sun
    if (frameCounter < 25) {
      birdState = !birdState;
      drawSprite(119, 0, sun1, false);

      if (playerYPos == 2) {
        drawSprite(playerXPos, playerYPos, ledBoyHead, 0);
        drawSprite(playerXPos, (playerYPos + 1), ledBoyBody1, 0);
      }

      for (uint8_t x = 0; x < ENEMY_COUNT; x++) {
        bool flying = flyingEnemies & _BV(x);
        drawSprite(enemiesPos[x], flying ? 2 : 3, enemy1A, flying);
      }
    } else {
      drawSprite(119, 0, sun2, false);

      if (playerYPos == 2) {
        drawSprite(playerXPos, (playerYPos + 1), ledBoyBody2, 0);
      }
      for (uint8_t x = 0; x < ENEMY_COUNT; x++) {
        bool flying = flyingEnemies & _BV(x);
        drawSprite(enemiesPos[x], flying ? 2 : 3, enemy1B, flying);
      }
    }

    if (bonusXPos <= 120) drawSprite(bonusXPos, 1, bonusOrb, false);

    // A high jump collects the orb and arms one collision-saving shield.
    if (bonusXPos <= playerXPos + 7 && bonusXPos + 7 >= playerXPos && playerYPos < 2) {
      bonusXPos = 255;
      shield = true;
      score = min((uint8_t)MAX_SCORE, (uint8_t)(score + 5));
      beep(180, 30);
    }

    // Check for collisions with enemies
    for (uint8_t x = 0; x < ENEMY_COUNT; x++) {
      // Once a jump has started, both ground and flying enemies are clearable.
      // Requiring playerYPos == 0 made the flying-enemy safe window too short.
      if (playerYPos == 2 && enemiesPos[x] <= playerXPos + 7 && enemiesPos[x] + 7 >= playerXPos) {
        if (!shield) goto gameOver;
        shield = false;
        oled.drawLine(enemiesPos[x], 2, 8, 0x00);
        oled.drawLine(enemiesPos[x], 3, 8, 0x00);
        enemiesPos[x] = 120;
        flyingEnemies &= ~_BV(x);
        beep(80, 60);
      }
    }
  }

gameOver:
  PORTC.PIN2CTRL &= ~PORT_ISC_gm;
  // Game over sequence
  for (uint8_t x = 0; x < 10; x++) {
    beep(250, 100);
  }
  oled.clearScreen();
  drawSprite(score, 1, ledBoyHead, 0);
  oled.drawLine(0, 2, score, 0xFF);
  uint8_t highScore = readScoreFromEEPROM(EEPROM_SCORE);
  if (score > highScore) {
    highScore = score;
    writeScoreToEEPROM(EEPROM_SCORE, score);
  }
  oled.drawLine(0, 3, highScore, 0x18); // persistent best-score bar
  timer = interruptTimer;
  while ((interruptTimer - timer) < 2000);
}

// Intro screen function
void intro() {
  buttonDebounce();
  uint16_t introStart = interruptTimer;
  timer = introStart;

  // Animate title screen
  for (uint8_t x = 8; x > 0 && BUTTONHIGH; x--) {
    drawTitle(0, 1, titleText, x, 127);
    while ((interruptTimer - timer) < 220);
    timer = interruptTimer;
  }
  drawTitle(0, 1, titleText, 0, 127);

  // Wait for button press to start the game
  while (BUTTONHIGH) {
    if ((interruptTimer - timer) > 150) {
      drawSprite(60, 2, ledBoyHead, false);
      drawSprite(60, 3, ledBoyBody1, false);
      oled.drawLine(10, 0, batteryBar(), 0b00111111); // Draw voltage meter
      drawSprite(0, 0, battIcon, 0);
      timer = interruptTimer;
    }

    // Enter sleep mode if no input for 7 seconds
    if ((uint16_t)(interruptTimer - introStart) > 7000) {
      oled.clearScreen();
      oled.ssd1306_send_command(0xAE);
      goToSleep();
    }
  }
}

// Bit shift function for smooth vertical scrolling of player sprite
bool bitShiftLedBoySprite(uint8_t xPos, uint8_t yPos, bool scrollUp) {
  // Erase the complete previous jump frame before advancing to the next one.
  if ((interruptTimer - timer3) > jumpLength) {
    timer3 = interruptTimer;
    clearPlayerTrail(xPos);

    if (bitShift1 == 0 && bitShift2 == 8) {
      bitShift1 = 8;
      bitShift2 = 0;
      return true;
    }
    bitShift1--;
    bitShift2++;
  }

  if (scrollUp) {
    drawLedBoy(xPos, yPos, ledBoyHead, bitShift1, scrollUp);

    if (yPos == 1) {
      drawLedBoy(xPos, (yPos + 2), ledBoyBody1, bitShift2, !scrollUp);
    }

    if ((interruptTimer & 1) == 0) {
      drawLedBoy(xPos, (yPos + 1), ledBoyHead, bitShift2, !scrollUp);
    } else {
      drawLedBoy(xPos, (yPos + 1), ledBoyBody1, bitShift1, scrollUp);
    }
  } else {
    drawLedBoy(xPos, (yPos + 1), ledBoyBody1, bitShift1, scrollUp);

    if ((interruptTimer & 1) == 0) {
      drawLedBoy(xPos, yPos, ledBoyBody1, bitShift2, !scrollUp);
    } else {
      drawLedBoy(xPos, yPos, ledBoyHead, bitShift1, scrollUp);
    }
  }

  return false;
}

void clearPlayerTrail(uint8_t column) {
  for (uint8_t page = 0; page < 4; page++) {
    oled.drawLine(column, page, 8, 0x00);
  }
}

// Function to draw the player sprite with bit shifting
void drawLedBoy(uint8_t column, uint8_t page, const uint8_t sprite[8], uint8_t bitShift, bool scrollUp) {
  oled.setCursor(column, page); // Position cursor
  oled.ssd1306_send_data_start();
  for (uint8_t x = 0; x < 8; x++) {
    oled.ssd1306_send_data_byte(scrollUp ? sprite[x] << bitShift : sprite[x] >> bitShift);
  }
  oled.ssd1306_send_data_stop();
}

// Function to draw the title with bit shifting
void drawTitle(uint8_t column, uint8_t page, const uint8_t* sprite, uint8_t bitShift, uint8_t length) {
  oled.setCursor(column, page); // Position cursor
  oled.ssd1306_send_data_start();
  for (uint8_t x = 0; x < length; x++) {
    oled.ssd1306_send_data_byte(sprite[x] << bitShift);
  }
  oled.ssd1306_send_data_stop();
}

void drawSprite (uint8_t column, uint8_t page, const uint8_t* sprite, bool mirrored) {
  oled.setCursor(column, page);// position cursor column - page
  oled.ssd1306_send_data_start();
  for (uint8_t x = 0; x < 8; x++) {
    oled.ssd1306_send_data_byte(sprite[mirrored ? 7 - x : x]);
  }
  oled.ssd1306_send_data_stop();
}

void buttonDebounce(void) {
  timer = interruptTimer;
  while (BUTTONLOW || (interruptTimer - timer) < 150); // super simple button debounce
}

void writeScoreToEEPROM(uint8_t address, uint8_t value) {
  eeprom_update_byte((uint8_t *)(uintptr_t)address, value);
}

uint8_t readScoreFromEEPROM(uint8_t address) {
  uint8_t value = eeprom_read_byte((uint8_t *)(uintptr_t)address);
  return value <= MAX_SCORE ? value : 0;
}

void goToSleep(void) {

  if (!BATTCHR) {
    //oled.clearScreen();
    SCREEN_OFF;
  }
  PORTC.PIN3CTRL |= PORT_ISC_BOTHEDGES_gc;  //attach interrupt to portC pin 3 keeps pull up enabled
  ADC0.CTRLA = 0; // disable adc
  //PORTC.PIN2CTRL &= ~PORT_ISC_gm; // disable sensor interrupt
  //_PROTECTED_WRITE(WDT.CTRLA, 0);
  //TCA0.SPLIT.CTRLA = 0; //disable TCA0 and set divider to 1
  //TCA0.SPLIT.CTRLESET = TCA_SPLIT_CMD_RESET_gc | 0x03; //set CMD to RESET to do a hard reset of TCA0.
  TCB0.INTCTRL &= ~TCB_CAPT_bm; // Disable the capture interrupt
  TCB0.INTFLAGS = TCB_CAPT_bm;  // Clear the interrupt flag
  sleep_enable();
  sleep_cpu();  // go to sleep
}

// Read Supply Voltage (in millivolts)
uint16_t readSupplyVoltage() {
  ADC0.CTRLA = ADC_ENABLE_bm; // Enable ADC
  analogReference(VDD); // Set reference voltage to VDD
  VREF.CTRLA = VREF_ADC0REFSEL_1V5_gc; // Set internal reference to 1.5V

  // Take two readings to ensure accuracy (first reading may be invalid)
  uint32_t reading = analogRead(ADC_INTREF);
  reading = analogRead(ADC_INTREF);

  // Convert ADC reading to millivolts
  uint32_t intermediate = 1023UL * 1500; // 1023 * 1.5V reference
  if (!reading) return 0;
  reading = intermediate / reading; // Calculate voltage in mV
  return reading;
}

uint8_t batteryBar() {
  uint16_t millivolts = readSupplyVoltage();
  if (millivolts <= MINVOLTAGE) {
    goToSleep();
    return 1;
  }
  if (millivolts >= MAXVOLTAGE) return MAX_SCORE;
  return 1 + (uint16_t)(millivolts - MINVOLTAGE) * 13 / 100;
}

void beep(uint8_t freq, uint16_t delayTime) {  // sound function

  if (sound) analogWrite(PIN_PC1, freq);
  delay(delayTime);
  turnOffPWM(PIN_PC1);
}

ISR(TCB0_INT_vect) {// timmer
  interruptTimer++;

  if (frameCounter < 50) {
    frameCounter++;
  } else {
    frameCounter = 0;
  }
  TCB0_INTFLAGS = 1; // clear flag
}

ISR(PORTA_PORT_vect) {
  _PROTECTED_WRITE(RSTCTRL.SWRR, 1);
}

ISR(PORTC_PORT_vect) {  // turn sound off or on by shaking the device

  if (PORTC.INTFLAGS & PIN3_bm) {
    _PROTECTED_WRITE(RSTCTRL.SWRR, 1);
  }

  if (PORTC.INTFLAGS & PIN2_bm) {
    VPORTC.INTFLAGS = (1 << 2);

    if (interruptDebounce++, interruptDebounce > 4) {  // interrupt debounce
      PORTC.PIN2CTRL &= ~PORT_ISC_gm;
      interruptDebounce = 0;
      sound = !sound;
    }
  }
}
