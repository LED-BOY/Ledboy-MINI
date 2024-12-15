/*SpaceInvadersLight  for game&Light and any Attiny series 0,1,2 compatible.
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

int8_t lives = 2;
int8_t actualVoltage = 0;
uint8_t gameLevel = 1;
uint8_t gameSpeed = 0;  // controls game speed lower number faster game
uint8_t tempArray[6];
uint16_t score = 0;
uint16_t timer = 0;
bool showIntro = true;
bool startGame = false;
volatile uint8_t interruptDebounce = 0;
volatile uint16_t interruptTimer = 0;
volatile bool sound = true;

void setup() {
  //_PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);// enable watchdog 4 sec. aprox.
  PORTA.DIR = 0b00000000;                   // setup ports in and out //  pin2 (GREEN) on
  PORTB.DIR = 0b00000011;                   // setup ports in and out
  PORTC.DIR = 0b00000011;                   // setup ports in and out
  PORTC.PIN3CTRL = PORT_PULLUPEN_bm;        // button pullup
  PORTC.PIN2CTRL = PORT_PULLUPEN_bm;        // sensor pullup
  PORTA.PIN4CTRL = PORT_PULLUPEN_bm;        // charge pin pullup
  PORTA.PIN4CTRL |= PORT_ISC_BOTHEDGES_gc;  //attach interrupt to portA pin 4 keeps pull up enabled

  TCB0.INTCTRL = TCB_CAPT_bm;  // Setup Timer B as compare capture mode to trigger interrupt
  TCB0_CCMP = 2000;            // CLK
  TCB0_CTRLA = (1 << 1) | TCB_ENABLE_bm;

  sei();                                             // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);               // configure sleep mode
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);  // enable watchdog 4 sec. aprox.

  // reset hi score
  while (BUTTONLOW || interruptTimer < 300) {

    if (interruptTimer > 4000) {  // clear HI score
      writeScoreToEEPROM(126, 0);
      //PORTA.DIR = (1 << 7);
    }
  }
  oled.begin();  // start oled screen
  oled.clearScreen();
  _PROTECTED_WRITE(WDT.CTRLA, 0);  // disable watchdog

  if (BATTCHR) {
    drawSprite(4, 0, battIcon);
    goToSleep();
  }
}

void loop() {

  if (showIntro) introScreen();

  if (buttonDebounce(200)) game();

  if ((interruptTimer - timer) > 1000) {
    uint8_t arrowPos = 42;
    for (uint8_t x = 0; x < (interruptTimer - timer) / 1000; x++) {  //draw player lives on screen
      drawSprite(arrowPos, 3, arrowRight);                           // draw arrow to give feedback of sleep timer
      arrowPos += 15;
    }
  }

  if ((interruptTimer - timer) > 6000) {
    goToSleep();
  }
}

void introScreen(void) {
  oled.clearScreen();
  //level indicator
  drawSprite(40, 1, letterL);  // letters L
  drawSprite(48, 1, letterE);  // letters E
  drawSprite(56, 1, letterV);  // letters v
  drawSprite(64, 1, letterE);  // letters E
  drawSprite(72, 1, letterL);  // letters L
  drawDecimal(52, 2, gameLevel);
  //sleep timer
  drawSprite(0, 3, letterS);   // letters S
  drawSprite(8, 3, letterL);   // letters L
  drawSprite(16, 3, letterE);  // letters E
  drawSprite(24, 3, letterE);  // letters E
  drawSprite(32, 3, letterP);  // letters P

  if (!startGame) {               //  HI score letters
    drawSprite(92, 0, letterH);   // letters H
    drawSprite(100, 0, letterI);  // letters I
    showScore(readScoreFromEEPROM(126));
    drawSprite(4, 0, battIcon);
    drawDecimal(0, 1, voltageReading());  // display batt %
    startGame = true;
    score = 0;
  }
  showIntro = false;
  timer = interruptTimer;
}

void game(void) {
  int8_t enemiesPosX = 0;
  int8_t lastEnemiesPosX = 0;
  uint8_t mothershipPosX = 102;
  uint8_t lastmothershipPosX = 0;
  uint8_t playerPos = 20;
  uint8_t enemiesPosY = 0;
  uint8_t enemiesSpeed = (180 - gameSpeed);
  uint8_t newEnemiesYPos = 0;
  uint8_t lastPlayerPos = 20;
  uint8_t playerShotState = 0;
  uint8_t enemiesShotState = 0;
  uint8_t playerFirePosX = 0;
  int8_t enemiesFirePosX = 0;
  int8_t playerFirePosY = 3;
  int8_t enemiesFirePosY = 0;
  uint8_t enemiesKilled[sizeof(enemiesPattern) / sizeof(uint8_t)];
  uint8_t enemiesKilledCounter = 0;
  int8_t enemiesDirection[] = { -1, 1 };
  uint16_t enemiesFireTimer = 0;
  uint16_t playerFireTimer = 0;
  uint16_t enemiesTimer = 0;
  uint16_t playerTimer = 0;
  uint16_t mothershipTimer = 0;
  bool enemiesMoveRight = true;
  bool enemiesPosXEven = true;
  bool playerFireAgain = true;
  bool enemiesFireAgain = true;
  bool enemyIsDead = false;
  bool mothershipIsDead = true;
  bool soundStatus = !sound;

  // draws some screen elements like score and batt display.
  oled.clearScreen();
  drawSprite(4, 0, battIcon);
  //drawSprite (4, 2, percentSimbol);
  drawDecimal(0, 1, voltageReading());  // display batt %
  showScore(0);

  for (uint8_t x = 0; x < 4; x++) {  // draws screen bars
    oled.drawLine(18, x, 1, 0b11111111);
    oled.drawLine(110, x, 1, 0b11111111);
  }

  for (uint8_t x = 0; x < sizeof(enemiesKilled) / sizeof(uint8_t); x++) {  // clears enemies killed array.
    enemiesKilled[x] = 0;
  }

  for (uint8_t x = 0; x < lives; x++) {  //draw player lives on screen
    drawSprite(116, x + 2, ship);
  }

  while (true) {

    if (soundStatus != sound) {  //show  sound on or off
      soundStatus = sound;
      if (sound) {
        drawSprite(2, 3, letterS);  // letters S sound on
      } else {
        drawSprite(2, 3, letterM);  // letters M mute sound off
      }
      delay(800);
      PORTA.DIR = 0b00000000;
      PORTC.PIN2CTRL |= PORT_ISC_RISING_gc;  //attach interrupt to portC pin 2 keeps pull up enabled
    }

    if ((interruptTimer - timer) > 16000) {
      timer = interruptTimer;
      for (uint8_t x = 0; x < 2; x++) {
        oled.drawLine(20, (newEnemiesYPos + x), 90, 0x00);  // clear  enemy position
      }
      newEnemiesYPos++;
      drawDecimal(0, 1, voltageReading());  // display batt %
    }

    if (enemiesFireAgain && interruptTimer % 600 == 0 && mothershipIsDead && newEnemiesYPos > 0) {  // spawn mothership
      mothershipIsDead = false;
    }

    if ((interruptTimer - mothershipTimer) > 35 && !mothershipIsDead) {  // mothership moving logic
      mothershipTimer = interruptTimer;
      oled.drawLine(mothershipPosX, 0, 8, 0x00);  // clear  mothership position

      if (mothershipPosX--, mothershipPosX < 20) {
        mothershipIsDead = true;  // mothership reaches end of screen
        mothershipPosX = 102;
      } else {
        drawSprite(mothershipPosX, 0, mothership);
      }
    }

    if (playerFirePosX > mothershipPosX && playerFirePosX < (mothershipPosX + 8) && playerFirePosY == 0 && !mothershipIsDead) {
      drawSprite(mothershipPosX, 0, explosion);  //  explosion sprite
      beep(250, 100);
      oled.drawLine(mothershipPosX, 0, 8, 0x00);  // clear  mothership position
      mothershipIsDead = true;                    // mothership reaches end of screen
      playerFireAgain = true;
      mothershipPosX = 102;
      showScore(10);

      if (lives < 2) {  // extra life
        lives++;

        for (uint8_t x = 0; x < lives; x++) {  //draw player lives on screen
          drawSprite(116, x + 2, ship);
        }
      }
    }

    if ((interruptTimer - enemiesTimer) > enemiesSpeed) {  // enemies movement and kill logic
      enemiesTimer = interruptTimer;
      enemiesPosX += enemiesDirection[enemiesMoveRight];
      beep(250, 4);

      for (uint8_t x = 0; x < sizeof(enemiesPattern) / sizeof(uint8_t); x++) {  // defines enemies positions.
        enemyIsDead = false;

        for (uint8_t y = 0; y < sizeof(enemiesKilled) / sizeof(uint8_t); y++) {  // checks if the enemy is dead

          if (enemiesKilled[y] == enemiesPattern[x]) {
            enemyIsDead = true;
          }
        }

        if (!enemyIsDead) {  // only if the enemy isn't dead continue.

          if ((enemiesPattern[x] + enemiesPosX) > 102 && enemiesMoveRight == true) {  // chesks boundaries
            enemiesMoveRight = false;
          }

          if ((enemiesPattern[x] + enemiesPosX) < 26 && enemiesMoveRight == false) {
            enemiesMoveRight = true;
          }

          if (enemiesFireAgain) {  // choose wich enemy fires, randomizer based on timer press.
            if (interruptTimer % 4 == 0) {
              enemiesFirePosX = (enemiesPattern[x] + enemiesPosX);
              enemiesFireAgain = false;
            }
          }

          if (x % 2 == 0) {  // if enemy pattern is even or odd draws it in the corresponding screen page (y Positon)
            enemiesPosY = 0;
          } else {
            enemiesPosY = 1;
          }
          enemiesPosY += newEnemiesYPos;

          if (playerFirePosX > (enemiesPattern[x] + enemiesPosX) && playerFirePosX < (enemiesPattern[x] + enemiesPosX) + 8 && playerFirePosY == enemiesPosY) {  // if palyer hits an enemy
            enemiesKilled[enemiesKilledCounter] = enemiesPattern[x];
            enemiesKilledCounter++;
            playerFireAgain = true;
            drawSprite((enemiesPattern[x] + lastEnemiesPosX), enemiesPosY, explosion);  //  explosion sprite
            beep(250, 100);
            oled.drawLine((enemiesPattern[x] + lastEnemiesPosX), enemiesPosY, 8, 0x00);  // clear  enemy position
            showScore(2);                                                                // show score and add to score

            if (enemiesKilledCounter > 3) {  // makes final enemies faster.
              enemiesSpeed = (40 - gameLevel);
            }
          } else {
            oled.drawLine((enemiesPattern[x] + lastEnemiesPosX), enemiesPosY, 8, 0x00);  // clear last enemy position

            if (enemiesPosX % 2 == 0) {                                           // choose enemies animation based on if the pos is even or odd.
              drawSprite(enemiesPattern[x] + enemiesPosX, enemiesPosY, enemy1A);  // draws the enemy on screen.
            } else {
              drawSprite(enemiesPattern[x] + enemiesPosX, enemiesPosY, enemy1B);  // draws the enemy on screen.
            }
          }
        }
      }
      lastEnemiesPosX = enemiesPosX;
      if (enemiesKilledCounter > 5) {  //advance to next level

        if (gameLevel < 20) {  // up to level 20 difficulty
          gameLevel++;
          gameSpeed += 5;
          break;  // ends game loop and advance to next level
        }
      }
    }

    if ((interruptTimer - playerFireTimer) > 120) {  // player fire logic
      playerFireTimer = interruptTimer;
      oled.drawLine(playerFirePosX, playerFirePosY, 1, 0b00000000);  // clear player shoot

      if (playerFireAgain) {  // player fire initial poss
        playerFirePosX = (playerPos + 4);
        playerFirePosY = 2;
        playerShotState = 0;
        playerFireAgain = false;
      }

      if (playerShotState > 6) {
        playerShotState = 0;
        playerFirePosY--;

        if (playerFirePosY < 0) {
          playerFireAgain = true;
        }
      }

      if (!playerFireAgain) {
        oled.drawLine(playerFirePosX, playerFirePosY, 1, (0b11000000 >> playerShotState));
        playerShotState++;
      }
    }

    if ((interruptTimer - enemiesFireTimer) > 120 && !enemiesFireAgain) {  // enemies fire logic
      enemiesFireTimer = interruptTimer;

      oled.drawLine(enemiesFirePosX, enemiesFirePosY, 1, 0b00000000);

      if (enemiesShotState > 6) {
        enemiesShotState = 0;
        enemiesFirePosY++;

        if (enemiesFirePosY > 3) {
          enemiesFirePosY = enemiesPosY;
          enemiesShotState = 6;
          enemiesFireAgain = true;
        }
      }

      if (!enemiesFireAgain) {
        oled.drawLine(enemiesFirePosX, enemiesFirePosY, 1, (0b00000011 << enemiesShotState));
        enemiesShotState++;
      }
    }

    if ((interruptTimer - playerTimer) > 50) {  // player movement logic.
      playerTimer = interruptTimer;

      if (lastPlayerPos != playerPos) {  // clear last player position
        lastPlayerPos == playerPos;
        oled.drawLine(20, 3, 90, 0x00);
      }

      if (BUTTONLOW && playerPos < 103) {
        playerPos++;
      } else if (playerPos > 20) {
        playerPos--;
      }
      drawSprite(playerPos, 3, ship);  //  ship sprite
    }

    if (enemiesFirePosX > playerPos && enemiesFirePosX < (playerPos + 8) && enemiesFirePosY == 3 || enemiesPosY > 2) {  // checks if player is hit or not.

      for (uint8_t x = 0; x < 10; x++) {
        drawSprite(playerPos, 3, explosion);  //  explosion animation
        beep(250, 60);
        oled.drawLine(playerPos, 3, 8, 0x00);
        beep(250, 60);
      }

      if (lives--, lives < 0) {
        if (score > readScoreFromEEPROM(126)) {  // write score to eeprom
          writeScoreToEEPROM(126, score);
        }
        gameLevel = 1;  // reset game to level 1
        score = 0;
        gameSpeed = 0;
        lives = 2;
      }
      break;  // ends game loop
    }
  }
  PORTC.PIN2CTRL &= ~PORT_ISC_gm;
  showIntro = true;
  timer = interruptTimer;
}

void showScore(uint16_t addScore) {  // draws all ledInvadersScore using showTime function

  if (score += addScore, score > 9999) score = 9999;
  uint16_t valueDecimals = score;

  if (score < 100) {  // functions to draw units in digit 3, decimals in digit 2 and hundredths in digit 1
    drawDecimal(112, 0, 0);
    drawDecimal(112, 1, score);
  } else {
    drawDecimal(112, 0, (score / 100));
    drawDecimal(112, 1, (valueDecimals - ((score / 100) * 100)));
  }
}

void writeScoreToEEPROM(uint8_t address, uint16_t number) {    // ledInvadersScore is an uint16_t so needs to be stored in 2 eeprom slots of 8bits
  eeprom_write_byte((uint8_t *)address, number >> 8);          // shift all the bits from the number to the right, 8 times
  eeprom_write_byte((uint8_t *)(address + 1), number & 0xFF);  // only store 8bits of the right
}

uint16_t readScoreFromEEPROM(uint8_t address) {  // reads and reconstruct the number
  byte byte1 = eeprom_read_byte((uint8_t *)address);
  byte byte2 = eeprom_read_byte((uint8_t *)address + 1);

  return (byte1 << 8) + byte2;
}

bool buttonDebounce(uint16_t debounceDelay) {

  if (BUTTONHIGH) return false;
  timer = interruptTimer;

  while ((interruptTimer - timer) < debounceDelay || BUTTONLOW)
    ;
  return true;
}

void drawSprite(uint8_t column, uint8_t page, uint8_t *sprite) {
  oled.setCursor(column, page);  // position cursor column - page
  oled.ssd1306_send_data_start();

  for (uint8_t x = 0; x < 8; x++) {
    oled.ssd1306_send_data_byte(sprite[x]);
  }
  oled.ssd1306_send_data_stop();
}

void drawDecimal(uint8_t firstPixel, uint8_t page, uint8_t value) {  // this function takes the digit and value from characters.h and draws it without the 0 to the left in hours
  uint8_t valueUnits = value;                                        // always draws 2 digits.

  if (value < 10) {
    drawSprite(firstPixel, page, numbers[0]);
    drawSprite((firstPixel + 8), page, numbers[value]);
  } else {
    value /= 10;  // some math to substract the 0 from the left in hours digits
    drawSprite(firstPixel, page, numbers[value]);
    value *= 10;
    valueUnits -= value;
    drawSprite((firstPixel + 8), page, numbers[valueUnits]);
  }
}

void beep(uint8_t freq, uint16_t delayTime) {  // sound function

  if (sound) analogWrite(PIN_PC1, freq);
  delay(delayTime);
  turnOffPWM(PIN_PC1);
}

void goToSleep(void) {

  if (!BATTCHR) {
    //oled.clearScreen();
    SCREEN_OFF;
  }
  PORTC.PIN3CTRL |= PORT_ISC_BOTHEDGES_gc;  //attach interrupt to portC pin 3 keeps pull up enabled
  //PORTC.PIN2CTRL &= ~PORT_ISC_gm; // disable sensor interrupt
  //_PROTECTED_WRITE(WDT.CTRLA, 0);
  //TCA0.SPLIT.CTRLA = 0; //disable TCA0 and set divider to 1
  //TCA0.SPLIT.CTRLESET = TCA_SPLIT_CMD_RESET_gc | 0x03; //set CMD to RESET to do a hard reset of TCA0.
  sleep_enable();
  sleep_cpu();  // go to sleep
}

uint16_t readSupplyVoltage() {  //returns value in millivolts  taken from megaTinyCore example
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
  actualVoltage = map(readSupplyVoltage(), MINVOLTAGE, MAXVOLTAGE, 0, 99);
  actualVoltage = constrain(actualVoltage, 1, 99);

  if (actualVoltage < 5) goToSleep();  // prevent battery compleate discharge
  return actualVoltage;
}

ISR(TCB0_INT_vect) {
  /* The interrupt flag has to be cleared manually */
  TCB0_INTFLAGS = 1;  // clear flag
  interruptTimer++;
  if (interruptTimer % 100 == 0) interruptDebounce = 0;
  // wdt_reset(); // reset watchdog
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
      sound = !sound;
    }
  }
}
