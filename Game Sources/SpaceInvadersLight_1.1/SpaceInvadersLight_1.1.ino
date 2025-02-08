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
    // Display the intro screen if the `showIntro` flag is set
    if (showIntro) {
        introScreen();
    }

    // Check for button press to start the game
    if (buttonDebounce(200)) {
        game();
    }

    // Handle sleep timer feedback
    if ((interruptTimer - timer) > 1000) {
        uint8_t arrowPos = 42;  // Initial position for the first arrow

        // Draw arrows to indicate the elapsed time towards sleep mode
        for (uint8_t x = 0; x < (interruptTimer - timer) / 1000; x++) {
            drawSprite(arrowPos, 3, arrowRight);  // Draw an arrow
            arrowPos += 15;  // Move the position for the next arrow
        }
    }

    // Enter sleep mode after 6 seconds of inactivity
    if ((interruptTimer - timer) > 6000) {
        goToSleep();
    }
}

void introScreen(void) {
    // Clear the screen
    oled.clearScreen();

    // Draw level indicator
    drawSprite(40, 1, letterL);  // Letter 'L'
    drawSprite(48, 1, letterE);  // Letter 'E'
    drawSprite(56, 1, letterV);  // Letter 'V'
    drawSprite(64, 1, letterE);  // Letter 'E'
    drawSprite(72, 1, letterL);  // Letter 'L'
    drawDecimal(52, 2, gameLevel);  // Display current game level

    // Draw sleep timer label
    drawSprite(0, 3, letterS);   // Letter 'S'
    drawSprite(8, 3, letterL);   // Letter 'L'
    drawSprite(16, 3, letterE);  // Letter 'E'
    drawSprite(24, 3, letterE);  // Letter 'E'
    drawSprite(32, 3, letterP);  // Letter 'P'

    // Display high score and battery status if the game hasn't started yet
    if (!startGame) {
        // Draw high score label
        drawSprite(92, 0, letterH);   // Letter 'H'
        drawSprite(100, 0, letterI);  // Letter 'I'
        showScore(readScoreFromEEPROM(126));  // Display high score from EEPROM

        // Draw battery icon and percentage
        drawSprite(4, 0, battIcon);  // Battery icon
        drawDecimal(0, 1, voltageReading());  // Display battery percentage

        // Reset game state
        startGame = true;
        score = 0;
    }

    // Reset intro screen flag and update timer
    showIntro = false;
    timer = interruptTimer;
}

// Main game function
void game() {
    int8_t enemiesPosX = 0;
    int8_t lastEnemiesPosX = 0;
    uint8_t mothershipPosX = 102;
    //uint8_t lastMothershipPosX = 0;
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
   // bool enemiesPosXEven = true;
    bool playerFireAgain = true;
    bool enemiesFireAgain = true;
    bool enemyIsDead = false;
    bool mothershipIsDead = true;
    bool soundStatus = !sound;

    // Draw initial screen elements
    oled.clearScreen();
    drawSprite(4, 0, battIcon);
    drawDecimal(0, 1, voltageReading());  // Display battery percentage
    showScore(0);

    // Draw screen bars
    for (uint8_t x = 0; x < 4; x++) {
        oled.drawLine(18, x, 1, 0b11111111);
        oled.drawLine(110, x, 1, 0b11111111);
    }

    // Clear enemies killed array
    for (uint8_t x = 0; x < sizeof(enemiesKilled) / sizeof(uint8_t); x++) {
        enemiesKilled[x] = 0;
    }

    // Draw player lives on screen
    for (uint8_t x = 0; x < lives; x++) {
        drawSprite(116, x + 2, ship);
    }

    // Main game loop
    while (true) {
        // Handle sound status
        if (soundStatus != sound) {
            soundStatus = sound;
            if (sound) {
                drawSprite(2, 3, letterS);  // Show 'S' for sound on
            } else {
                drawSprite(2, 3, letterM);  // Show 'M' for mute
            }
            delay(800);
            PORTA.DIR = 0b00000000;
            PORTC.PIN2CTRL |= PORT_ISC_RISING_gc;  // Attach interrupt to portC pin 2
        }

        // Handle battery display update
        if ((interruptTimer - timer) > 16000) {
            timer = interruptTimer;
            for (uint8_t x = 0; x < 2; x++) {
                oled.drawLine(20, (newEnemiesYPos + x), 90, 0x00);  // Clear enemy position
            }
            newEnemiesYPos++;
            drawDecimal(0, 1, voltageReading());  // Update battery percentage
        }

        // Spawn mothership
        if (enemiesFireAgain && interruptTimer % 600 == 0 && mothershipIsDead && newEnemiesYPos > 0) {
            mothershipIsDead = false;
        }

        // Mothership movement logic
        if ((interruptTimer - mothershipTimer) > 35 && !mothershipIsDead) {
            mothershipTimer = interruptTimer;
            oled.drawLine(mothershipPosX, 0, 8, 0x00);  // Clear mothership position

            if (mothershipPosX--, mothershipPosX < 20) {
                mothershipIsDead = true;  // Mothership reaches end of screen
                mothershipPosX = 102;
            } else {
                drawSprite(mothershipPosX, 0, mothership);
            }
        }

        // Check if player hits mothership
        if (playerFirePosX > mothershipPosX && playerFirePosX < (mothershipPosX + 8) && playerFirePosY == 0 && !mothershipIsDead) {
            drawSprite(mothershipPosX, 0, explosion);  // Draw explosion sprite
            beep(250, 100);
            oled.drawLine(mothershipPosX, 0, 8, 0x00);  // Clear mothership position
            mothershipIsDead = true;
            playerFireAgain = true;
            mothershipPosX = 102;
            showScore(10);

            // Grant extra life if applicable
            if (lives < 2) {
                lives++;
                for (uint8_t x = 0; x < lives; x++) {
                    drawSprite(116, x + 2, ship);
                }
            }
        }

        // Enemies movement and kill logic
        if ((interruptTimer - enemiesTimer) > enemiesSpeed) {
            enemiesTimer = interruptTimer;
            enemiesPosX += enemiesDirection[enemiesMoveRight];
            beep(250, 4);

            for (uint8_t x = 0; x < sizeof(enemiesPattern) / sizeof(uint8_t); x++) {
                enemyIsDead = false;

                for (uint8_t y = 0; y < sizeof(enemiesKilled) / sizeof(uint8_t); y++) {
                    if (enemiesKilled[y] == enemiesPattern[x]) {
                        enemyIsDead = true;
                    }
                }

                if (!enemyIsDead) {
                    if ((enemiesPattern[x] + enemiesPosX) > 102 && enemiesMoveRight) {
                        enemiesMoveRight = false;
                    }

                    if ((enemiesPattern[x] + enemiesPosX) < 26 && !enemiesMoveRight) {
                        enemiesMoveRight = true;
                    }

                    if (enemiesFireAgain && interruptTimer % 4 == 0) {
                        enemiesFirePosX = (enemiesPattern[x] + enemiesPosX);
                        enemiesFireAgain = false;
                    }

                    enemiesPosY = (x % 2) + newEnemiesYPos;

                    if (playerFirePosX > (enemiesPattern[x] + enemiesPosX) && playerFirePosX < (enemiesPattern[x] + enemiesPosX) + 8 && playerFirePosY == enemiesPosY) {
                        enemiesKilled[enemiesKilledCounter] = enemiesPattern[x];
                        enemiesKilledCounter++;
                        playerFireAgain = true;
                        drawSprite((enemiesPattern[x] + lastEnemiesPosX), enemiesPosY, explosion);
                        beep(250, 100);
                        oled.drawLine((enemiesPattern[x] + lastEnemiesPosX), enemiesPosY, 8, 0x00);
                        showScore(2);

                        if (enemiesKilledCounter > 3) {
                            enemiesSpeed = (40 - gameLevel);
                        }
                    } else {
                        oled.drawLine((enemiesPattern[x] + lastEnemiesPosX), enemiesPosY, 8, 0x00);

                        if (enemiesPosX % 2 == 0) {
                            drawSprite(enemiesPattern[x] + enemiesPosX, enemiesPosY, enemy1A);
                        } else {
                            drawSprite(enemiesPattern[x] + enemiesPosX, enemiesPosY, enemy1B);
                        }
                    }
                }
            }
            lastEnemiesPosX = enemiesPosX;

            if (enemiesKilledCounter > 5) {
                if (gameLevel < 20) {
                    gameLevel++;
                    gameSpeed += 5;
                    break;  // Advance to next level
                }
            }
        }

        // Player fire logic
        if ((interruptTimer - playerFireTimer) > 120) {
            playerFireTimer = interruptTimer;
            oled.drawLine(playerFirePosX, playerFirePosY, 1, 0b00000000);

            if (playerFireAgain) {
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

        // Enemies fire logic
        if ((interruptTimer - enemiesFireTimer) > 120 && !enemiesFireAgain) {
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

        // Player movement logic
        if ((interruptTimer - playerTimer) > 50) {
            playerTimer = interruptTimer;

            if (lastPlayerPos != playerPos) {
                lastPlayerPos = playerPos;
                oled.drawLine(20, 3, 90, 0x00);
            }

            if (BUTTONLOW && playerPos < 103) {
                playerPos++;
            } else if (playerPos > 20) {
                playerPos--;
            }
            drawSprite(playerPos, 3, ship);
        }

        // Check if player is hit
        if ((enemiesFirePosX > playerPos && enemiesFirePosX < (playerPos + 8) && enemiesFirePosY == 3) || enemiesPosY > 2) {
            for (uint8_t x = 0; x < 10; x++) {
                drawSprite(playerPos, 3, explosion);
                beep(250, 60);
                oled.drawLine(playerPos, 3, 8, 0x00);
                beep(250, 60);
            }

            if (lives--, lives < 0) {
                if (score > readScoreFromEEPROM(126)) {
                    writeScoreToEEPROM(126, score);
                }
                gameLevel = 1;
                score = 0;
                gameSpeed = 0;
                lives = 2;
            }
            break;  // End game loop
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

  while ((interruptTimer - timer) < debounceDelay || BUTTONLOW);
  return true;
}

void drawSprite(uint8_t column, uint8_t page, const uint8_t* sprite) {
  oled.setCursor(column, page);  // position cursor column - page
  oled.ssd1306_send_data_start();

  for (uint8_t x = 0; x < 8; x++) {
    oled.ssd1306_send_data_byte(sprite[x]);
  }
  oled.ssd1306_send_data_stop();
}

void drawDecimal(uint8_t firstPixel, uint8_t page, uint8_t value) {
    // Always draw 2 digits
    uint8_t tensDigit = value / 10;  // Extract the tens digit
    uint8_t unitsDigit = value % 10; // Extract the units digit

    // Draw the tens digit (or 0 if value is less than 10)
    drawSprite(firstPixel, page, numbers[tensDigit]);
    
    // Draw the units digit
    drawSprite(firstPixel + 8, page, numbers[unitsDigit]);
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
