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

uint8_t bitShift1 = 8;
uint8_t bitShift2 = 0;
uint8_t jumpLenght = 19;
bool soildState = false;
bool ledState = true;
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
  oled.clearScreen();// screen clear
  intro();
  oled.clearScreen();
  game();
}

// Main game function
void game() {
    uint8_t lives = 1;
    uint8_t playerXPos = 16;
    uint8_t playerYPos = 2;
    uint8_t enemiesPos[3] = {46, 80, 120};
    uint8_t score = 0;
    int8_t backgroundXPos = 0;
    const uint8_t skyElementsPos[8] = {0, 7, 10, 17, 31, 37, 60, 66};
    bool birdState = false;
    bool jump = false;

    // Wait for button press to start the game
    while (BUTTONLOW);

    // Main game loop
    while (lives > 0) {
        // Update game state every 50 milliseconds
        if ((interruptTimer - timer) > 50) {
            uint8_t playerSpriteEnlarge = 0; // Enlarge player sprite area to prevent solid animation from drawing over the player
            timer = interruptTimer;

            // Update enemy positions
            for (uint8_t x = 0; x < 3; x++) {
                if (enemiesPos[x] < 1 || enemiesPos[x] > 200) {
                    if (enemiesPos[x] == 0) { // Increase score when enemy reaches position 0
                        score += 2;
                    }
                    enemiesPos[x] = 127; // Reset enemy position
                } else {
                    // Move enemies based on frameCounter to simulate random behavior
                    if (frameCounter < 30) {
                        enemiesPos[x]--;
                    } else if (frameCounter < 40) {
                        enemiesPos[x] -= 2;
                    } else {
                        enemiesPos[x] -= 3;
                        jumpLenght = 13;
                    }
                }
            }

            // Update player position during jump
            if (playerYPos < 2) {
                playerXPos++;
            } else if (playerXPos > 16 && playerYPos == 2) {
                oled.drawLine((playerXPos + 7), playerYPos, 8, 0x00); // Clear enemy position
                playerXPos--;
                playerSpriteEnlarge = 3; // Prevent solid from drawing over player
            }

            // Draw trees and solid
            for (uint8_t x = 0; x < 127; x += 8) {
                if (x < (playerXPos - 1) || playerYPos > 1) {
                    drawSprite(x, 1, trees, !soildState);
                }
                if (x < (playerXPos - playerSpriteEnlarge) || x > ((playerXPos + 7) - (playerSpriteEnlarge / playerSpriteEnlarge)) || playerYPos < 1) {
                    drawSprite(x, 2, soild, soildState);
                    drawSprite(x, 3, soild, soildState);
                }
            }
            soildState = !soildState;
        }

        // Update background every 100 milliseconds
        if ((interruptTimer - timer2) > 100) {
            timer2 = interruptTimer;
            oled.drawLine(0, 0, 127, 0x00); // Clear sky

            // Scroll background
            if (backgroundXPos > -127) {
                backgroundXPos -= 2;
            } else {
                backgroundXPos = 0;
            }

            // Draw sky elements
            bool cloudState = false;
            for (uint8_t x = 0; x < 8; x++) {
                cloudState = !cloudState;
                drawSprite((backgroundXPos + skyElementsPos[x]), 0, cloud, cloudState);
            }
            drawSprite((backgroundXPos + 48), 0, bird, birdState);
            drawSprite((backgroundXPos + 90), 0, bird, birdState);
        }

        // Handle player jump
        if (BUTTONLOW && playerYPos == 2) {
            jump = true;
            playerYPos = 1;
            beep(250, 100);
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

            for (uint8_t x = 0; x < 3; x++) {
                drawSprite(enemiesPos[x], 3, enemy1A, false);
            }
        } else {
            drawSprite(119, 0, sun2, false);

            if (playerYPos == 2) {
                drawSprite(playerXPos, (playerYPos + 1), ledBoyBody2, 0);
            }
            for (uint8_t x = 0; x < 3; x++) {
                drawSprite(enemiesPos[x], 3, enemy1B, false);
            }
        }

        // Check for collisions with enemies
        for (uint8_t x = 0; x < 3; x++) {
            if ((enemiesPos[x] > (playerXPos - 7) && enemiesPos[x] < (playerXPos + 7) && (playerYPos + 1) == 3) || 
                (enemiesPos[x] > (playerXPos + 120) && enemiesPos[x] < (playerXPos + 128) && (playerYPos + 1) == 3)) {
                lives = 0; // End game if collision detected
            }
        }
    }

    // Game over sequence
    for (uint8_t x = 0; x < 10; x++) {
        beep(250, 100);
    }
    oled.clearScreen();
    drawSprite(score, 1, ledBoyHead, 0);
    oled.drawLine(0, 2, score, 0xFF);
    while ((interruptTimer - timer) < 2000);
}

// Intro screen function
void intro() {
    buttonDebounce();
    interruptTimer = 0;

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
            for (uint8_t x = 0; x < 127; x += 8) {
                drawSprite(x, 2, soild, soildState);
                drawSprite(x, 3, soild, soildState);
            }
            drawSprite(60, 2, ledBoyHead, false);
            drawSprite(60, 3, ledBoyBody1, false);
            soildState = !soildState;
            uint8_t voltageReading = map(readSupplyVoltage(), MINVOLTAGE, MAXVOLTAGE, 1, 117);
            constrain(voltageReading, 1, 117);
            oled.drawLine(10, 0, voltageReading, 0b00111111); // Draw voltage meter
            drawSprite(0, 0, battIcon, 0);
            timer = interruptTimer;
        }

        // Enter sleep mode if no input for 7 seconds
        if (interruptTimer > 7000) {
            oled.clearScreen();
            oled.ssd1306_send_command(0xAE);
            goToSleep();
        }
    }
}

// Bit shift function for smooth vertical scrolling of player sprite
bool bitShiftLedBoySprite(uint8_t xPos, uint8_t yPos, bool scrollUp) {
    bool finished = false;

    if (scrollUp) {
        drawLedBoy(xPos, yPos, ledBoyHead, bitShift1, scrollUp);

        if (yPos == 1) {
            drawLedBoy(xPos, (yPos + 2), ledBoyBody1, bitShift2, !scrollUp);
        }

        if (interruptTimer % 2 == 0) {
            drawLedBoy(xPos, (yPos + 1), ledBoyHead, bitShift2, !scrollUp);
        } else {
            drawLedBoy(xPos, (yPos + 1), ledBoyBody1, bitShift1, scrollUp);
        }
    } else {
        drawLedBoy(xPos, (yPos + 1), ledBoyBody1, bitShift1, scrollUp);

        if (interruptTimer % 2 == 0) {
            drawLedBoy(xPos, yPos, ledBoyBody1, bitShift2, !scrollUp);
        } else {
            drawLedBoy(xPos, yPos, ledBoyHead, bitShift1, scrollUp);
        }
    }

    // Control jump length
    if ((interruptTimer - timer3) > jumpLenght) {
        timer3 = interruptTimer;

        if (bitShift1 == 0 && bitShift2 == 8) {
            bitShift1 = 8;
            bitShift2 = 0;
            finished = true;
        } else {
            bitShift1--;
            bitShift2++;
        }
    }
    return finished;
}

// Function to draw the player sprite with bit shifting
void drawLedBoy(uint8_t column, uint8_t page, const uint8_t sprite[8], uint8_t bitShift, bool scrollUp) {
    oled.setCursor(column, page); // Position cursor
    oled.ssd1306_send_data_start();
    for (uint8_t x = 0; x < 8 && scrollUp; x++) {
        oled.ssd1306_send_data_byte(sprite[x] << bitShift);
    }
    for (uint8_t x = 0; x < 8 && !scrollUp; x++) {
        oled.ssd1306_send_data_byte(sprite[x] >> bitShift);
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
  for (uint8_t x = 0; x < 8 && !mirrored; x++) {
    oled.ssd1306_send_data_byte(sprite[x]);
  }
  for (uint8_t x = 8; x > 0 && mirrored; x--) {
    oled.ssd1306_send_data_byte(sprite[x]);
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
      PORTA.DIR = (1 << 6);
      interruptDebounce = 0;
    }
  }
}
