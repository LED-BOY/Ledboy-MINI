# Ledboy-2
Ledboy 2 mini game console and watch, plus a attiny 416 dev board.

Features: 

-128x32 sd1306 oled screen

-Batt charger IC

-Buzzer

-Sensor

-4k 20mhz MCU

-USB to serial CH340E

-Button input

-Micro USB port

-RGB LED

-RTC clock

-Temp. monitor

-Mosfet controlled batt ouput.

![20241013_181641](https://github.com/user-attachments/assets/4d0b056c-7846-4fc5-b9b9-d17b08134ec4)

![20241012_212736](https://github.com/user-attachments/assets/9099f9ec-bdde-4ca3-9121-9837cd252f21)

# Ledboy 2 schematics 

[Ledboy 2 1.5 schematic.pdf](https://github.com/user-attachments/files/17515040/Ledboy.2.1.5.schematic.pdf)


# How to Flash

First you need to download CH341SER.EXE windows drivers from: https://www.wch-ic.com/products/CH340.html?

## Avrdudess method (recomended):

Download Avrdudess: https://github.com/ZakKemble/AVRDUDESS

Make sure all your settings are the same:

The only thing it may be different is your COM port.
If you dont have any other serial device the only com port available should be your Ledboy 2
if not, unplug your Ledboy 2 and see what port is missing, that is yours Ledboy 2 COM port.
Make sure you select the .hex file in the Flash slot.
![Arduboy Flash](https://github.com/user-attachments/assets/cc9d610f-1d70-485a-bf29-70e3a2a77b5b)
