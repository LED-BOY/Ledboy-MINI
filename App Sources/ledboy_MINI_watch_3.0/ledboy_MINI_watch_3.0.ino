/*
  Game&Light Clock for GAME&LIGHT and any Attiny series 1.
  Flash CPU Speed: 5MHz.
  This code is released under GPL v3.
*/

#include "tinyOLED.h"
#include "sprites.h"
#include "digits.h"
#include "MinuteTimer.h"

#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// =====================================================
// CONSTANTS
// =====================================================

// Battery
constexpr uint16_t MAX_VOLTAGE = 4100u;
constexpr uint16_t MIN_VOLTAGE = 3100u;
constexpr uint8_t LOW_BATT = 140;

// CPU / Timer
constexpr uint32_t CPU_FREQ = 5000000UL;
constexpr uint16_t TCA_PRESCALER = 64;
constexpr uint16_t TCA_PER_VALUE =
  ((CPU_FREQ / TCA_PRESCALER) / 1000) - 1;

// Modes
constexpr uint8_t STANDBY = 0;
constexpr uint8_t ON = 1;
constexpr uint8_t IDLE = 2;

// Sensor
constexpr uint8_t sensorSensitivity = 3;

// Options
constexpr uint8_t OPTION_COUNT = 5;

// =====================================================
// HARDWARE HELPERS
// =====================================================

inline void oledPowerOff() __attribute__((always_inline));
inline void oledPowerOn() __attribute__((always_inline));
inline bool buttonLow() __attribute__((always_inline));
inline bool buttonHigh() __attribute__((always_inline));
inline bool batteryCharging() __attribute__((always_inline));

inline void oledPowerOff() {
  PORTC.OUTSET = PIN0_bm;
}

inline void oledPowerOn() {
  PORTC.OUTCLR = PIN0_bm;
}

inline bool buttonLow() {
  return !(PORTC.IN & PIN3_bm);
}

inline bool buttonHigh() {
  return PORTC.IN & PIN3_bm;
}

inline bool batteryCharging() {
  return !(PORTA.IN & PIN4_bm);
}

// =====================================================
// GLOBAL VARIABLES
// =====================================================

bool needsOnRefresh = true;
uint8_t ledColor = 0;
uint8_t lastSecond = 255;

uint16_t timer = 0;
uint16_t optionsDebounce = 800;

bool awake = false;

volatile bool shouldWakeUp = true;

volatile uint8_t seconds = 0;
volatile uint8_t minutes = 0;
volatile uint8_t hours = 0;
volatile uint8_t mode = ON;

volatile uint8_t interruptDebounce = 0;
volatile uint8_t interruptSensitivity = 0;
volatile uint16_t interruptTimer = 0;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================

void setup();
void loop();

void initTCA();

void displayTime(uint8_t hourPos,
                 uint8_t minutesPos);

void displayBatt();
void displayMode();

void ledON(bool optionSelected);

void wakeUpFromSleep();

void options();

uint8_t setNumber(uint8_t sprite,
                  uint8_t stepSize,
                  uint8_t limit);

void setTime();

void drawTime(uint8_t firstPixel,
              int8_t value);

inline bool buttonDebounce(uint16_t debounceDelay);

void goToSleep();

uint16_t readSupplyVoltage();

// =====================================================
// INTERRUPTS
// =====================================================

ISR(PORTA_PORT_vect);
ISR(PORTC_PORT_vect);
ISR(TCA0_OVF_vect);
ISR(TCB0_INT_vect);
ISR(RTC_PIT_vect);

// =====================================================
// SETUP
// =====================================================

void setup() {

  // Configure Ports
  PORTA.DIR = 0b00000000;
  PORTB.DIR = 0b00000011;
  oledPowerOff();
  PORTC.DIR = 0b00000011;

  // Pullups / Interrupts
  PORTC.PIN2CTRL = PORT_PULLUPEN_bm;

  PORTA.PIN4CTRL =
    PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

  PORTC.PIN3CTRL =
    PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

  // RTC Setup
  CCP = 0xD8;

  CLKCTRL.XOSC32KCTRLA =
    CLKCTRL_ENABLE_bm;

  while (RTC.STATUS > 0)
    ;

  RTC.CTRLA =
    RTC_PRESCALER_DIV1_gc | RTC_RUNSTDBY_bm;

  RTC.CLKSEL =
    RTC_CLKSEL_TOSC32K_gc;

  while (RTC.PITSTATUS > 0)
    ;

  RTC.PITCTRLA =
    RTC_PERIOD_CYC32768_gc | RTC_PITEN_bm;

  RTC.PITINTCTRL =
    RTC_PI_bm;

  initTCA();

  sei();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // OLED init
  wakeUpFromSleep();
  oled.drawSprite(48, 1, rstSprite, 16, 2, false);
  _delay_ms(1500);
  oled.clearScreen();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  wakeUpFromSleep();

  displayMode();

  // If minute timer finished,
  if (minuteTimer.isFinished() && minuteTimer.isActive() && buttonLow()) {

    minuteTimer.reset();
    oled.begin();
    oled.clearScreen();
    needsOnRefresh = true;
  }

  // Button
  if (buttonDebounce(optionsDebounce)) {

    optionsDebounce = 50;

    if (readSupplyVoltage() > MIN_VOLTAGE) {
      options();
    }
  }

  // Auto sleep
  const uint16_t elapsed =
    (uint16_t)(interruptTimer - timer);

  if (elapsed > 4000) {

    timer = interruptTimer;

    optionsDebounce = 800;

    needsOnRefresh = true;

    oled.clearScreen();

    goToSleep();
  }
}

// =====================================================
// INIT TCA
// =====================================================

void initTCA() {

  TCA0.SINGLE.CTRLB =
    TCA_SINGLE_WGMODE_NORMAL_gc;

  TCA0.SINGLE.PER =
    TCA_PER_VALUE;

  TCA0.SINGLE.INTCTRL =
    TCA_SINGLE_OVF_bm;

  TCA0.SINGLE.CTRLA =
    TCA_SINGLE_CLKSEL_DIV64_gc | TCA_SINGLE_ENABLE_bm;

  timer = interruptTimer;
}

// =====================================================
// DISPLAY MODE
// =====================================================

void displayMode() {

  if (!awake) return;

  const uint8_t currentMode = mode;
  const uint8_t currentSecond = seconds;

  // Mode changed
  if (currentMode == ON && needsOnRefresh) {

    oled.brightScreen();

    displayBatt();

    needsOnRefresh = false;

    ledColor = 0;

    PORTA.DIRCLR =
      RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;

  } else if (currentMode == STANDBY) {

    oled.dimScreen();
  }

  // Update only if needed
  if (currentMode != IDLE || currentSecond != lastSecond) {

    lastSecond = currentSecond;

    mode = IDLE;

    const bool timerActive =
      minuteTimer.isActive();

    const bool timerFinished =
      minuteTimer.isFinished();

    if (timerActive && timerFinished) {

      oled.sleep();

      return;
    }

    displayTime(64, 0);

    if (timerActive && !timerFinished) {

      const uint32_t target =
        minuteTimer.targetSeconds;

      if (target) {

        const uint32_t remaining =
          target - minuteTimer.elapsedSeconds;

        const uint8_t timerLine =
          ((uint32_t)remaining * 127u) / target;

        oled.drawLine(
          0,
          0,
          timerLine,
          0b00000101);
      }
    }
  }
}

inline void displayTime(uint8_t hourPos,
                        uint8_t minutesPos) {

  drawTime(hourPos, hours);
  drawTime(minutesPos, minutes);

  _PROTECTED_WRITE(WDT.CTRLA, 0);
}

// =====================================================
// DISPLAY BATTERY
// =====================================================

void displayBatt() {

  uint16_t actualVoltage =
    constrain(readSupplyVoltage(),
              MIN_VOLTAGE,
              MAX_VOLTAGE);

  actualVoltage -= MIN_VOLTAGE;

  actualVoltage =
    ((uint32_t)actualVoltage * 117u) / (MAX_VOLTAGE - MIN_VOLTAGE);

  for (uint8_t x = 224;
       x > 159;
       x -= 64) {

    oled.drawLine(
      10,
      3,
      actualVoltage,
      x);

    if (actualVoltage > 20) {
      actualVoltage = 20;
    }
  }

  oled.drawSprite(
    0,
    3,
    batteryCharging()
      ? chargingSprite
      : battSprite,
    8,
    1,
    false);
}

// =====================================================
// LED CONTROL
// =====================================================

void ledON(bool optionSelected) {

  while ((uint16_t)(interruptTimer - timer) < 2000) {

    if (buttonDebounce(50) || optionSelected) {

      optionSelected = false;

      ledColor++;

      if (ledColor > 3) {
        ledColor = 0;
      }

      PORTA.DIRCLR =
        RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;

      PORTA.OUTSET =
        RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;

      switch (ledColor) {

        case 1:
          PORTA.OUTCLR =
            RED_LED_PIN;
          PORTA.DIRSET =
            RED_LED_PIN;
          break;

        case 2:
          PORTA.OUTCLR =
            GREEN_LED_PIN;
          PORTA.DIRSET =
            GREEN_LED_PIN;
          break;

        case 3:
          PORTA.OUTCLR =
            BLUE_LED_PIN;
          PORTA.DIRSET =
            BLUE_LED_PIN;
          break;
      }
    }
  }
}

// =====================================================
// WAKEUP
// =====================================================

void wakeUpFromSleep() {

  if (!shouldWakeUp || awake) {
    return;
  }

  sleep_disable();

  initTCA();

  oledPowerOn();
  _delay_ms(150);

  // Restore I2C
  PORTB.DIRCLR =
    PIN0_bm | PIN1_bm;

  PORTB.PIN0CTRL =
    PORT_PULLUPEN_bm;

  PORTB.PIN1CTRL =
    PORT_PULLUPEN_bm;

  _delay_us(100);

  oled.begin();
  oled.clearScreen();

  awake = true;
}

// =====================================================
// OPTIONS
// =====================================================

void options() {

  uint8_t option = 0;

  constexpr uint8_t spriteOptionPos[] = { 102, 80, 58, 36, 14 };

  oled.clearScreen();

  for (uint8_t x = 0;
       x < OPTION_COUNT;
       x++) {

    oled.drawSprite(
      spriteOptionPos[x],
      1,
      optionSprites[x],
      16,
      2,
      false);
  }

  while (true) {

    if (buttonDebounce(25) || option == 0) {

      if (option >= OPTION_COUNT) {
        break;
      }

      oled.drawLine(
        0,
        0,
        127,
        0x00);

      oled.drawSprite(
        spriteOptionPos[option],
        0,
        arrow,
        8,
        1,
        true);

      option++;

      if (option > OPTION_COUNT) {
        break;
      }
    }

    if ((uint16_t)(interruptTimer - timer) > 2000) {

      timer = interruptTimer;

      switch (option) {

        case 1:
          ledON(true);
          break;

        case 2:
          minuteTimer.setMinutes(
            setNumber(1, 5, 65));
          break;

        case 3:
          setTime();
          break;

        case 4:
          interruptSensitivity =
            setNumber(3, 1, 2) * sensorSensitivity;
          break;

        case 5:
          _PROTECTED_WRITE(
            RSTCTRL.SWRR,
            1);
          break;
      }

      break;
    }
  }

  oled.clearScreen();
}

// =====================================================
// SET NUMBER
// =====================================================

uint8_t setNumber(uint8_t spriteIndex,
                  uint8_t stepSize,
                  uint8_t limit) {

  uint8_t value = 0;

  oled.clearScreen();

  oled.drawSprite(
    90,
    1,
    optionSprites[spriteIndex],
    16,
    2,
    false);

  drawTime(0, value);

  while (true) {

    if (buttonDebounce(50)) {

      value += stepSize;

      if (value >= limit) {
        value = 0;
      }

      drawTime(0, value);
    }

    if ((uint16_t)(interruptTimer - timer) > 2000) {
      break;
    }
  }

  return value;
}

// =====================================================
// SET TIME
// =====================================================

void setTime() {

  RTC.PITINTCTRL = 0;

  uint8_t arrowPos = 86;

  for (uint8_t x = 0; x < 3; x++) {

    oled.clearScreen();

    oled.drawSprite(
      arrowPos,
      0,
      arrow,
      8,
      1,
      true);

    arrowPos = 22;

    if (x < 2) {
      displayTime(64, 0);
    } else {
      drawTime(0, seconds);
    }

    while (true) {

      if ((uint16_t)(interruptTimer - timer) > 285 && buttonLow()) {

        timer = interruptTimer;

        switch (x) {

          case 0:
            if (++hours > 23) {
              hours = 0;
            }
            break;

          case 1:
            if (++minutes > 59) {
              minutes = 0;
            }
            break;

          case 2:
            if (++seconds > 59) {
              seconds = 0;
            }
            break;
        }

        drawTime(
          (x == 0 ? 64 : 0),
          (x == 0
             ? hours
             : (x == 1
                  ? minutes
                  : seconds)));
      }

      if ((uint16_t)(interruptTimer - timer) > 2500) {

        timer = interruptTimer;
        break;
      }
    }
  }

  RTC.PITINTCTRL =
    RTC_PI_bm;
}

// =====================================================
// DRAW TIME
// =====================================================

void drawTime(uint8_t firstPixel,
              int8_t value) {

  if (value < 0) {
    value = -value;
  }

  const uint8_t units = value % 10;
  const uint8_t tens = value / 10;

  if (value < 10) {

    if (firstPixel == 64) {

      oled.drawLine(96, 1, 32, 0x00);
      oled.drawLine(96, 2, 32, 0x00);

    } else {

      oled.drawSprite(
        firstPixel + 32,
        1,
        numbers[0],
        14,
        2,
        true);
    }

  } else {

    oled.drawSprite(
      firstPixel + 32,
      1,
      numbers[tens],
      14,
      2,
      true);
  }

  oled.drawSprite(
    firstPixel,
    1,
    numbers[units],
    14,
    2,
    true);
}

// =====================================================
// BUTTON DEBOUNCE
// =====================================================

inline bool buttonDebounce(uint16_t debounceDelay) {

  static bool state = true;
  static uint16_t lastChange = 0;

  const bool reading = buttonHigh();

  // No change
  if (reading == state) {

    lastChange = interruptTimer;
    return false;
  }

  // Debounce time
  if ((uint16_t)(interruptTimer - lastChange)
      <= debounceDelay) {

    return false;
  }

  // Update state
  lastChange = interruptTimer;
  state = reading;

  // Button pressed
  if (!reading) {
    timer = interruptTimer;
    return true;
  }

  return false;
}

// =====================================================
// SLEEP
// =====================================================

void goToSleep() {

  const bool charging =
    batteryCharging();

  if (!charging && (readSupplyVoltage() > (MIN_VOLTAGE + LOW_BATT))) {

    PORTC.PIN2CTRL =
      (PORTC.PIN2CTRL & ~PORT_ISC_gm) | (interruptSensitivity ? PORT_ISC_RISING_gc : PORT_ISC_INTDISABLE_gc);

  } else if (charging) {

    oled.drawSprite(
      0,
      3,
      chargingSprite,
      8,
      1,
      false);
  }

  if (!charging) {
    oled.sleep();

    PORTB.DIRCLR =
      PIN0_bm | PIN1_bm;

    PORTB.PIN0CTRL &=
      ~PORT_PULLUPEN_bm;

    PORTB.PIN1CTRL &=
      ~PORT_PULLUPEN_bm;

    oledPowerOff();
  }

  ADC0.CTRLA = 0;

  TCA0.SINGLE.CTRLA = 0;

  RSTCTRL.RSTFR =
    RSTCTRL_PORF_bm;

  awake = false;

  shouldWakeUp = false;

  sleep_enable();

  sleep_cpu();
}

// =====================================================
// READ VCC
// =====================================================

uint16_t readSupplyVoltage() {

  ADC0.CTRLA = ADC_ENABLE_bm;

  VREF.CTRLA = VREF_ADC0REFSEL_1V5_gc;

  ADC0.MUXPOS = ADC_MUXPOS_INTREF_gc;

  ADC0.COMMAND = ADC_STCONV_bm;

  while (!(ADC0.INTFLAGS & ADC_RESRDY_bm))
    ;

  ADC0.INTFLAGS = ADC_RESRDY_bm;

  const uint16_t reading = ADC0.RES;

  ADC0.CTRLA = 0;

  return 1534500UL / reading;
}

// =====================================================
// TCA INTERRUPT
// =====================================================

ISR(TCA0_OVF_vect) {

  interruptTimer++;

  TCA0.SINGLE.INTFLAGS =
    TCA_SINGLE_OVF_bm;
}

// =====================================================
// PORTA INTERRUPT
// =====================================================

ISR(PORTA_PORT_vect) {

  const uint8_t flags =
    PORTA.INTFLAGS;

  PORTA.INTFLAGS = flags;

  if (flags & PIN4_bm) {

    _PROTECTED_WRITE(
      WDT.CTRLA,
      WDT_PERIOD_4KCLK_gc);
  }

  if (mode != ON) {
    mode = ON;
  }

  if (!shouldWakeUp) {

    PORTC.PIN2CTRL &=
      ~PORT_ISC_gm;

    shouldWakeUp = true;
  }
}

// =====================================================
// PORTC INTERRUPT
// =====================================================

ISR(PORTC_PORT_vect) {

  const uint8_t flags =
    PORTC.INTFLAGS;

  PORTC.INTFLAGS = flags;

  if (flags & PIN2_bm) {

    interruptDebounce++;

    if (interruptDebounce > interruptSensitivity) {

      PORTC.PIN2CTRL &=
        ~PORT_ISC_gm;

      interruptDebounce = 0;

      mode = STANDBY;

      shouldWakeUp = true;

    } else {

      sleep_cpu();
    }
  }

  if (flags & PIN3_bm) {

    if (mode != ON) {
      mode = ON;
    }

    if (!shouldWakeUp) {

      PORTC.PIN2CTRL &=
        ~PORT_ISC_gm;

      shouldWakeUp = true;
    }
  }
}

// =====================================================
// RTC INTERRUPT
// =====================================================

ISR(RTC_PIT_vect) {

  RTC.PITINTFLAGS =
    RTC_PI_bm;

  interruptDebounce = 0;

  uint8_t s = seconds + 1;

  if (s > 59) {

    s = 0;

    uint8_t m = minutes + 1;

    if (m > 59) {

      m = 0;

      uint8_t h = hours + 1;

      if (h > 23) {
        h = 0;
      }

      hours = h;
    }

    minutes = m;
  }

  seconds = s;

  minuteTimer.tick();

  if (!shouldWakeUp) {
    sleep_cpu();
  }
}
