/*LedBoyCaves  for game&Light and any Attiny series 0,1,2 compatible.
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
//#define wdt_reset() __asm__ __volatile__ ("wdr"::) // watchdog reset macro

int8_t actualVoltage = 0;
uint8_t tempArray [8];
uint16_t timer = 0;
volatile uint16_t interruptTimer = 0;
// game blobal Variables
volatile uint8_t  randomNumber = 1;
volatile uint8_t interruptDebounce = 0;
uint8_t playerXPos = 20;
uint8_t lastPlayerXPos = 0;
uint8_t actualCave[36];
uint16_t score = 0;
bool endGame = false;
bool detectColliton = false;


void setup() {
  PORTA.DIR = 0b00000000;                   // setup ports in and out //  pin2 (GREEN) on
  PORTB.DIR = 0b00000011;                   // setup ports in and out
  PORTC.DIR = 0b00000011;                   // setup ports in and out
  PORTC.PIN3CTRL = PORT_PULLUPEN_bm;        // button pullup
  PORTC.PIN2CTRL = PORT_PULLUPEN_bm;        // sensor pullup
  PORTA.PIN4CTRL = PORT_PULLUPEN_bm;        // charge pin pullup
  PORTA.PIN4CTRL |= PORT_ISC_BOTHEDGES_gc;  //attach interrupt to portA pin 4 keeps pull up enabled

  TCB0.INTCTRL = TCB_CAPT_bm;  // Setup Timer B as compare capture mode to trigger interrupt
  TCB0_CCMP = 4000;            // CLK
  TCB0_CTRLA = (1 << 1) | TCB_ENABLE_bm;

  sei();                                             // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);               // configure sleep mode
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);  // enable watchdog 4 sec. aprox.

  // reset hi score
  while (BUTTONLOW || interruptTimer < 300) {

    if (interruptTimer > 4000) {  // clear HI score
      //writeScoreToEEPROM(126, 0);
      //PORTA.DIR = (1 << 7);
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
  buttonDebounce();
  clearScreen(0, 4);
  intro();
  clearScreen(0, 4);
  game();
}

void game (void) {
  uint8_t caveYPos = 1;
  uint8_t caveYPosBitShiftLeft = 0;
  uint8_t caveYPosBitShiftRight = 8;
  uint8_t caveDropXPos = 0;
  uint8_t caveDropYPos = 0;
  uint8_t dropState = 0;
  uint16_t caveCollapseSpeed = 2000;
  uint16_t scrollRightTimer = 0;
  uint16_t scrollLeftTimer = 0;
  uint16_t caveScrollTimer = 0;
  uint16_t caveCollapseTimer = 0;
  uint16_t cavesDropsTimer = 0;
  bool caveCollapse = false;
  bool caveDown = true;
  bool caveDropFallAgain = true;

  drawCave(caveCollapse, caveYPos, 0, 0);

  while (!endGame) {

    if ((interruptTimer - caveScrollTimer) > 120 && BUTTONLOW && !caveCollapse) {
      beep(100, 25);
      clearScreen(1, 2);
      drawCave(caveCollapse, caveYPos, 0, 0);
      caveScrollTimer = interruptTimer;
      if (score++, score > 120) score = 120;
    }

    if ((interruptTimer - cavesDropsTimer) > 103) { // player fire logic
      cavesDropsTimer = interruptTimer;
      oled.drawLine (caveDropXPos, caveDropYPos, 1, 0b00000000); // clear player shoot

      if (caveDropFallAgain) {
        caveDropXPos = (randomNumber + 30);
        caveDropYPos = 1;
        dropState = 7;
        caveDropFallAgain = false;
      }

      if (dropState > 7) {
        dropState = 0;

        if (caveDropYPos++, caveDropYPos > 3) caveDropFallAgain = true;
      }

      if (!caveDropFallAgain) {
        oled.drawLine (caveDropXPos, caveDropYPos, 1, (0b00000001 << dropState));
        dropState++;
      }
    }

    if (caveDropXPos > playerXPos && caveDropXPos < (playerXPos + 8) && caveDropYPos == 3) endGame = true; // water drops kill you

    if ((interruptTimer - caveCollapseTimer) > caveCollapseSpeed) {
      bool shakePlayer = false;
      caveCollapseTimer = interruptTimer;
      caveCollapseSpeed = 120;
      clearScreen(caveYPos, (caveYPos + 1));
      caveCollapse = true;
      caveYPosBitShiftRight --;

      if (caveYPosBitShiftLeft++, caveYPosBitShiftLeft > 7) {
        caveYPos++;
        caveYPosBitShiftLeft = 0;
        caveYPosBitShiftRight = 8;
      }

      movePlayer((shakePlayer * 2));
      shakePlayer = !shakePlayer;

      if (caveYPos > 1 && caveYPosBitShiftLeft > 2)  detectColliton = true;

      if (caveYPos > 1 && caveYPosBitShiftLeft > 3 && !endGame) {
        caveYPosBitShiftLeft = 0;
        caveYPosBitShiftRight = 8;
        clearScreen(2, 4);
        detectColliton = false;

        while (caveYPosBitShiftLeft < 8) {

          if (interruptTimer % 40 == 0) {
            movePlayer((shakePlayer * 2)); // take out control of the player while cave is reseting
            shakePlayer = !shakePlayer;
            caveYPosBitShiftLeft++;
            caveYPosBitShiftRight--;
            drawCave(caveCollapse, (caveYPos - 1), caveYPosBitShiftRight, false);
            drawCave(caveCollapse, caveYPos, caveYPosBitShiftLeft, true);
          }
        }
        caveYPosBitShiftLeft = 0;
        caveYPosBitShiftRight = 8;
        caveCollapseSpeed = 221 * randomNumber;
        caveCollapse = false;
        caveYPosBitShiftLeft = 0;
        caveYPos = 1;
      } else {
        drawCave(caveCollapse, caveYPos, caveYPosBitShiftLeft, false);
        drawCave(caveCollapse, (caveYPos + 1), caveYPosBitShiftRight, true);
      }
    }

    if ((interruptTimer - scrollRightTimer) > 30 && BUTTONLOW) {
      scrollRightTimer = interruptTimer;
      movePlayer(1);
    }

    if ((interruptTimer - scrollLeftTimer) > 60 && BUTTONHIGH) {
      scrollLeftTimer = interruptTimer;
      movePlayer(-1);
    }
  }
  detectColliton = false;
  caveCollapse = false;
  endGame = false;
  drawSprite(playerXPos, 3, playerDead, false);
  while ((interruptTimer - timer) < 2000);
  timer = interruptTimer;
}



void movePlayer (int8_t playerDirection) {
  oled.drawLine(lastPlayerXPos, 3, 8, 0x00);

  if (playerXPos % 2 == 0) {
    drawSprite(playerXPos, 3, playerA, false);
  } else {
    drawSprite(playerXPos, 3, playerB, false);
  }
  lastPlayerXPos = playerXPos;

  if (playerDirection == 0) playerXPos--; // if value is 0 it is the same as -1 or go left
  playerXPos += playerDirection;

  if (playerXPos < 1) playerXPos = 119;

  if (playerXPos > 119) playerXPos = 0;
}

void drawCave(bool caveCollapse, uint8_t yPos, uint8_t bitShift, bool bitShiftRight) {
  uint8_t lastRandomNumber = 1;
  uint8_t actualCaveArrayPosition = 0;

  for (uint8_t x = 0; x < 126; x += 7) {

    if (!caveCollapse) {
      while ((interruptTimer - timer) < 3);
      timer = interruptTimer;
      actualCave[actualCaveArrayPosition] = randomNumber;
      oled.drawLine(x, yPos, 8, actualCave[actualCaveArrayPosition]);
      actualCaveArrayPosition++;

      if (randomNumber > lastRandomNumber) {
        actualCave[actualCaveArrayPosition] = (randomNumber - lastRandomNumber);
      } else {
        actualCave[actualCaveArrayPosition] = (lastRandomNumber - randomNumber);
      }
      oled.drawLine(x, yPos, 1, actualCave[actualCaveArrayPosition]);
      actualCaveArrayPosition++;
      lastRandomNumber = randomNumber;
    } else {

      for (uint8_t y = 0; y < 2; y ++) {
       
        if (detectColliton && actualCave[actualCaveArrayPosition + y] > 8 && x > playerXPos && x < (playerXPos + 8)) endGame = true;
      }

      if (bitShiftRight) {
        beep(25, 1);
        oled.drawLine(x, yPos, 8, actualCave[actualCaveArrayPosition] >> bitShift);
        actualCaveArrayPosition++;
        oled.drawLine(x, yPos, 1, actualCave[actualCaveArrayPosition] >> bitShift);
        actualCaveArrayPosition++;
      } else {
        oled.drawLine(x, yPos, 8, actualCave[actualCaveArrayPosition] << bitShift);
        actualCaveArrayPosition++;
        oled.drawLine(x, yPos, 1, actualCave[actualCaveArrayPosition] << bitShift);
        actualCaveArrayPosition++;
      }
    }
  }
}

void intro(void) {// intro screen.
  buttonDebounce();
  interruptTimer = 0;

  for (uint8_t x = 8; x > 0 && BUTTONHIGH; x--) {
    drawTitle(12, 1, title, x);
    while ((interruptTimer - timer) < 220);
    timer = interruptTimer;
  }

  drawTitle(12, 1, title, 0);
  voltageReading();
  oled.drawLine(10, 0, actualVoltage, 0b00111111); // draws voltage meter
  drawSprite(0, 0, battIcon, 0);
  oled.drawLine(0, 3, score, 0b00111100);
  drawSprite(score, 3, playerA, 0);
  score = 0;

  while (BUTTONHIGH) {
    if (interruptTimer > 7000) {
      clearScreen(0, 4);
      goToSleep();
    }
  }
}

void clearScreen (uint8_t y1, uint8_t y2 ) {
  for ( uint8_t x = y1; x < y2; x++) {
    oled.drawLine(0, x, 128, 0x00);
  }
}

void drawTitle (uint8_t column, uint8_t page, uint8_t sprite[], uint8_t bitShift) {
  oled.setCursor(column, page);// position cursor column - page
  oled.ssd1306_send_data_start();
  for (uint8_t x = 0; x < 107; x++) {
    oled.ssd1306_send_data_byte(sprite[x] << bitShift);
  }
  oled.ssd1306_send_data_stop();
}

void drawSprite (uint8_t column, uint8_t page, uint8_t sprite[], bool digit) {
  oled.setCursor(column, page);// position cursor column - page
  oled.ssd1306_send_data_start();

  for (uint8_t x = 0; x < 8; x++) {

    if (digit && x > 5) {
      oled.ssd1306_send_data_byte(0x00);
    } else {
      oled.ssd1306_send_data_byte(sprite[x]);
    }
  }
  oled.ssd1306_send_data_stop();
}

void buttonDebounce(void) {
  timer = interruptTimer;
  while (BUTTONLOW || (interruptTimer - timer) < 150); // super simple button debounce
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
  sleep_enable();
  sleep_cpu();  // go to sleep
}

uint16_t readSupplyVoltage() { //returns value in millivolts  taken from megaTinyCore example
  ADC0.CTRLA = ADC_ENABLE_bm;// enable adc
  analogReference(VDD);
  VREF.CTRLA = VREF_ADC0REFSEL_1V5_gc;
  // there is a settling time between when reference is turned on, and when it becomes valid.
  // since the reference is normally turned on only when it is requested, this virtually guarantees
  // that the first reading will be garbage; subsequent readings taken immediately after will be fine.
  // VREF.CTRLB|=VREF_ADC0REFEN_bm;
  // delay(10);
  uint16_t reading = analogRead(ADC_INTREF);
  reading = analogRead(ADC_INTREF);
  uint32_t intermediate = 1023UL * 1500;
  reading = intermediate / reading;
  return reading;
}

uint8_t voltageReading(void) {
  actualVoltage = map(readSupplyVoltage(), MINVOLTAGE, MAXVOLTAGE, 1, 117);
  constrain(actualVoltage, 1, 117);
  return actualVoltage;
}

void beep(uint8_t freq, uint16_t delayTime) {  // sound function
  analogWrite(PIN_PC1, freq);
  delay(delayTime);
  turnOffPWM(PIN_PC1);
}

ISR(TCB0_INT_vect) {// timmer
  interruptTimer++;

  if (randomNumber *= 2, randomNumber > 64) randomNumber = 1;
  TCB0_INTFLAGS = 1; // clear flag
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
      PORTA.DIR = (1 << 6);
      interruptDebounce = 0;
    }
  }
}
