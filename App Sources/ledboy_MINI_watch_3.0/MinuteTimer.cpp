#include "MinuteTimer.h"

extern volatile bool shouldWakeUp;  // Variable global para despertar declared elsewere

MinuteTimer minuteTimer;

void MinuteTimer::setMinutes(uint8_t minutes) {

  if (minutes < 1) {
    active = false;
    return;
  }
  targetSeconds = static_cast<uint32_t>(minutes) * 60;
  elapsedSeconds = 0;
  active = true;

  // Apagar led cuando se inicia temporizador
  PORTA.OUTCLR = RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;
  PORTA.DIRCLR = RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;
}

void MinuteTimer::reset() {
  active = false;
  elapsedSeconds = 0;

  // Apagar led
  PORTA.OUTCLR = RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;  //set pins low
  PORTA.DIRCLR = RED_LED_PIN | GREEN_LED_PIN | BLUE_LED_PIN;  // reset all pins
  turnOffPWM(PIN_PC1);
}

void MinuteTimer::tick() {
  if (!active) return;

  elapsedSeconds++;

  if (elapsedSeconds >= targetSeconds) {
    shouldWakeUp = true;
    PORTA.DIRSET = RED_LED_PIN;  // Set pin as output
    PORTA.OUTTGL = RED_LED_PIN;  // Toggle pin state

    if (elapsedSeconds & 1) {
      // Step 1: Set up analogWrite on PC1 (WOB channel)
      analogWrite(PIN_PC1, 128);  // ~50% duty to start

      // Step 2: Set period (CMPBCLR) = 625 for 4000 Hz
      TCD0.CMPBCLR = 625;

      // Step 3: Trigger update by writing again to the same pin
      analogWrite(PIN_PC1, 128);  // Required to refresh and synchronize period/duty
    } else {
      turnOffPWM(PIN_PC1);
    }
  }
}


bool MinuteTimer::isFinished() const {
  return elapsedSeconds >= targetSeconds;
}

bool MinuteTimer::isActive() const {
  return active;
}
