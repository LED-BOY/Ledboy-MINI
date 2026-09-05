#ifndef MINUTETIMER_H
#define MINUTETIMER_H

#include <Arduino.h> // O el framework que uses para pinMode, digitalWrite

// Define LED pins for clarity

constexpr uint8_t RED_LED_PIN   = PIN7_bm;
constexpr uint8_t BLUE_LED_PIN  = PIN6_bm;
constexpr uint8_t GREEN_LED_PIN = PIN5_bm;

class MinuteTimer {
  private:
    bool active = false;           // Temporizador activo o no
    
  public:
    uint16_t targetSeconds = 0;   // Tiempo objetivo en segundos
    uint16_t elapsedSeconds = 0;  // Segundos transcurridos
    
    void setMinutes(uint8_t minutes);

    void reset();

    void tick();

    bool isActive() const;

    bool isFinished() const;

};

extern MinuteTimer minuteTimer;

#endif // MINUTETIMER_H
