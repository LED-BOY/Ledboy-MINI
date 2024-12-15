
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
uint16_t totalSecondsTimer = 0;
uint16_t totalSeconds = 0;
volatile bool shouldWakeUp = true;
volatile bool resumeOperation = true;
volatile uint8_t mode = 0;
volatile uint8_t ledColor = 0;
volatile uint8_t interruptDebounce = 0;
volatile uint8_t interruptSensitivity = 0;
volatile uint16_t interruptTimer = 0;
volatile uint16_t timer = 0;

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
  wakeUpFromSleep(resumeOperation);// restart display after wake up
  countdownTimer(); // countdowntimer logic

  if (buttonDebounce(300)) { // dimm screen or enter options menu if batt is too low can not enter the menu

    if (!oled.screenDimmed() && displayBatt() != 0) {// only enter menu if batt is above 0
      options(); // enter options
    } else {
      oled.brightScreen();
    }
  }

  if ((interruptTimer - timer) > 4000) { // prepare to sleep or dimm screen
    timer = interruptTimer;
    oled.dimScreen();

    if (totalSeconds == 0) { // only if the screen is on and the counter is off continue
      oled.clearScreen();

      if (BATTCHR) {
        drawSprite(0, 3, chargingSprite, 8, 1);
      } else {
        SCREEN_OFF
      }
      goToSleep();//mdoe 4 on on screen prevents go to sleep
    }
  }
}

void displayTime(uint8_t hourPos, uint8_t minutesPos) { // draws time in display
  drawTime(hourPos, 1, 24, hours);
  drawTime(minutesPos, 1, 24, minutes);
}

uint8_t displayBatt (void) {
  uint16_t actualVoltage = constrain(readSupplyVoltage (), MINVOLTAGE, MAXVOLTAGE);// read batt voltage and discards values outside range
  actualVoltage = map(actualVoltage, MINVOLTAGE, MAXVOLTAGE, 0, 117);

  for (uint8_t x = 224; x > 159; x -= 64) { // draw a line in bytes 10100000 or dec. 160 for low batt and 11100000 dec. 224 for normal batt level
    oled.drawLine (10, 3, actualVoltage, x); // indicates batt and low batt on screen

    if (actualVoltage > 20) actualVoltage = 20;// number to display low batt from pixel 0 to pixel 20 of the batt bar
  }

  if (!BATTCHR) drawSprite(0, 3, battSprite, 8, 1);// draws battery or charging icon if connected to usb, doesn´t draw any icon if batt is too low

  if (BATTCHR) drawSprite(0, 3, chargingSprite, 8, 1);
  return actualVoltage;
}

void ledON (bool optionSelected) { // leds on or off

  while ((interruptTimer - timer) < 2000) {

    if (buttonDebounce(250) || optionSelected) {
      timer = interruptTimer;
      optionSelected = false;
      if (ledColor++, ledColor > 2) ledColor = 0;

      switch (ledColor) { // option select menu.
        case 0:
          PORTA.DIR = 0b00000000;
          break;
        case 1:
          PORTA.DIR = 0b10000000;// pin2 (REDLED) as output
          break;
        case 2:
          PORTA.DIR = 0b01100000;//Green and blue leds as outputs
          break;
      }
    }
  }
}

void countdownTimer(void) {

  if (totalSeconds == 0) return; // timer logic

  if (seconds != lastSecond) {
    lastSecond = seconds;
    uint8_t timerBarValue = map(totalSeconds, 0 , totalSecondsTimer, 0, 126);
    oled.drawLine (timerBarValue, 0, 2, 0B00000000);
    oled.drawLine (0, 0, timerBarValue, 0B00000011);

    if (totalSeconds--, totalSeconds < 1) {
      oled.clearScreen();
      PORTA.DIR = (1 << 7);
      enableTca0();// enable TCA timer interrupt
      while (BUTTONHIGH);
      disableTca0();// disable TCA timer and hard reset
    }

    if (PORTA.DIR != 0b00000000) {
      ledColor = 0;
      PORTA.DIR = 0b00000000;
    }
    displayBatt();
    displayTime(64, 0);
  }
}

void wakeUpFromSleep(bool activateScreen) {

  if (!activateScreen || readSupplyVoltage () <= MINVOLTAGE) return;// if voltage is too low do not show time
  resumeOperation = false;
  interruptDebounce = 0; // reset debounce

  while ((interruptTimer - timer) < 200);//whait some time before initializing screen
  oled.begin();// start oled screen
  oled.clearScreen();
  _PROTECTED_WRITE(WDT.CTRLA, 0);// disable watchdog

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
      switch (option) { // option select menu.
        case 1:// led on
          ledON(true);
          break;
        case 2:// timer
          totalSeconds = (setNumber((option - 1), 5, 60) * 60); // set minutes for timer and multiply to get seconds
          totalSecondsTimer = totalSeconds;
          break;
        case 3:// set time
          setTime();
          break;
        case 4:
          interruptSensitivity = setNumber((option - 1), 1, 4); //sets sensitivity of the sensor, if sensitivity is > 0 sensor is enable otherwise is off
          break;
      }
      setOptions = false;
    }
  }
  resumeOperation = true;
}

uint8_t setNumber(uint8_t sprite, uint8_t base, uint8_t total) { // set timer and sensor sensitivity for always on function
  oled.clearScreen();
  drawSprite(90, 1, optionSprites[sprite], 16, 2);
  drawTime(0, 1, 24, 0);
  uint8_t result = 0;
  bool setCurrentNumber = true;

  while (setCurrentNumber) {

    if (BUTTONLOW) {
      timer = interruptTimer;

      if (result += base, result > total) result = 0;
      drawTime(0, 1, 24, result);
      buttonDebounce(300);
    }

    if ((interruptTimer - timer) > 2000) {
      setCurrentNumber = false;
    }
  }
  return result;
}

void setTime(void) {
  uint8_t arrowPos = 94;

  for (uint8_t x = 0; x < 3 ; x++) {
    bool setCurrentTime = true;
    oled.clearScreen();
    drawSprite(arrowPos, 0, arrow, 8, 1); // arrow

    if (x < 2) {
      drawTime(76, 1, 24, hours);
      drawTime(24, 1, 24, minutes);
    } else {
      drawTime(0, 1, 24, seconds);
    }
    while (setCurrentTime) {

      if ((interruptTimer - timer) > 250 && BUTTONLOW) {
        timer = interruptTimer;

        switch (x) {
          case 0:

            if (hours++, hours > 23) hours = 0;
            drawTime(76, 1, 24, hours);
            break;
          case 1:

            if (minutes++, minutes > 59) minutes = 0;
            drawTime(24, 1, 24, minutes);
            break;

          case 2:

            if (seconds++, seconds > 59) seconds = 0;
            drawTime(0, 1, 24, seconds);
            break;
        }
      }

      if ((interruptTimer - timer) > 2500) {
        setCurrentTime = false;
        timer = interruptTimer;
        arrowPos /= 2;
      }
    }
  }
}

void drawTime (uint8_t firstPixel, uint8_t page, uint8_t digitHeight, int8_t value) {// this function takes the digit and value from digits.h and draws it without the 0 to the left in hours
  if (value < 0) value = (~value) + 1; // xor value and add one to make it posite
  // always draws 2 digits ej: 01.
  if (value < 10) {

    if (firstPixel == 64 || firstPixel == 76) {
      for (uint8_t x = 1; x < 3; x++) {
        oled.drawLine (100, x, 32, 0B00000000);
      }
    } else {
      drawSprite((firstPixel + (digitHeight + 2)), page, numbers[0], 24, 2);
    }
    drawSprite(firstPixel, page, numbers[value], 24, 2);
  } else {
    drawSprite((firstPixel + (digitHeight + 2)), page, numbers[value / 10], 24, 2);
    drawSprite(firstPixel, page, numbers[value - ((value / 10) * 10)], 24, 2);
  }
}

void drawSprite(uint8_t column, uint8_t page, uint8_t *sprite, uint8_t SpritePixelsHeight, uint8_t SpritePagesWidth) {
  uint8_t i = 0;

  for (uint8_t y = 0; y < SpritePagesWidth; y++) {
    oled.setCursor(column, (page + y));
    oled.ssd1306_send_data_start();
    for (uint8_t x = (0 + i); x < (SpritePixelsHeight + i) && x < 128; x++) {
      oled.ssd1306_send_data_byte(sprite[x]);
    }
    oled.ssd1306_send_data_stop();
    i += SpritePixelsHeight;
  }
}

bool buttonDebounce(uint16_t debounceDelay) {

  if (BUTTONHIGH) return false;
  timer = interruptTimer;

  while ((interruptTimer - timer) < debounceDelay || BUTTONLOW) { // super simple button debounce

    if ((interruptTimer - timer) > 6000) {
      SCREEN_OFF
      //PORTA.DIR = (1 << 6);// pin1 (BLUELED) as output
      _PROTECTED_WRITE(RSTCTRL.SWRR, 1); //reset device
    }
  }
  return true;
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

void goToSleep (void) {

  if (!BATTCHR && readSupplyVoltage() > (MINVOLTAGE + LOWBATT) && interruptSensitivity != 0) PORTC.PIN2CTRL  |= PORT_ISC_FALLING_gc; // Attach interrupt to portC pin 2 keeps pull up enabled (sensor)// if voltage is below minvoltage + 200mha it disables allways on display.
  PORTC.PIN3CTRL  |= PORT_ISC_BOTHEDGES_gc; //attach interrupt to portA pin 5 keeps pull up enabled
  ADC0.CTRLA = 0; // disable adc
  shouldWakeUp = false;
  sleep_enable();
  sleep_cpu();// go to sleep
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

ISR(PORTA_PORT_vect) {
  PORTC.PIN2CTRL &= ~PORT_ISC_gm;//disable sensor interrupt
  VPORTC.INTFLAGS = (1 << 2); // clear interrupts flag
  VPORTA.INTFLAGS = (1 << 4); // clear interrupts flag
  shouldWakeUp = true;
  resumeOperation = true; // if screen always on is off reset screen after sleep
  timer = interruptTimer;
  mode = 0;
  SCREEN_ON
  sleep_disable();
  _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_4KCLK_gc);// enable watchdog 4 sec. aprox.
}

ISR(PORTC_PORT_vect) {

  if (PORTC.INTFLAGS & PIN2_bm) {
    VPORTC.INTFLAGS |= (1 << 2); // clear interrupts flag

    if (interruptDebounce < interruptSensitivity) {// interrupt debounce
      interruptDebounce++;
      sleep_cpu();
    } else {
      mode = 1;
      resumeOperation = true;
    }
  }

  if (PORTC.INTFLAGS & PIN3_bm) {
    VPORTC.INTFLAGS |= (1 << 3);//clear button interrupt flag
    PORTC.PIN3CTRL &= ~PORT_ISC_gm;//disable sensor interrupt
    PORTA.DIR = 0b00000000;// reset led
    ledColor = 0;
    mode = 0;
    resumeOperation = true;
  }

  if (resumeOperation) {
    PORTC.PIN2CTRL &= ~PORT_ISC_gm;//disable sensor interrupt
    SCREEN_ON
    timer = interruptTimer;
    shouldWakeUp = true;
    sleep_disable();
  }
}

ISR(TCA0_OVF_vect) {
  /* The interrupt flag has to be cleared manually */
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  PORTC.OUTTGL = PIN1_bm;
}

ISR(TCB0_INT_vect) {
  /* The interrupt flag has to be cleared manually */
  TCB0_INTFLAGS = 1; // clear flag
  interruptTimer++;
}

ISR(RTC_PIT_vect) {
  /* Clear flag by writing '1': */
  RTC.PITINTFLAGS = RTC_PI_bm;

  if (seconds++, seconds > 59) {// acutal time keeping
    seconds = 0;

    if (minutes++, minutes > 59) {
      minutes = 0;

      if (hours++, hours > 23) {
        hours = 0;
      }
    }
  }

  if (!shouldWakeUp) sleep_cpu();// beacause of interrupt only if true wake up
}
