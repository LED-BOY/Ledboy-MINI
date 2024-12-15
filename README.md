# Ledboy-MINI
Ledboy  mini game console and watch, plus a attiny 416 dev board.

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
![20241124_192409](https://github.com/user-attachments/assets/6db445f6-dd00-4e11-9ec9-f807e5c56194)
![20241124_192634](https://github.com/user-attachments/assets/98457841-556e-4789-9c1f-1a8ea2f5075d)
![20241012_212736](https://github.com/user-attachments/assets/9099f9ec-bdde-4ca3-9121-9837cd252f21)

# Ledboy MINI schematics 

[Ledboy mini 1.4 rev b schematic.pdf](https://github.com/user-attachments/files/17895039/Ledboy.mini.1.4.rev.b.schematic.pdf)

# Board Layout

![PCB layout](https://github.com/user-attachments/assets/4d0872dd-545f-4ed2-84ed-03da116033b0)

# How to Flash:

NOTICE:
IF USING ONLY THE USB CABLE WITHOUT A BATTERY, HOW THE CHARGER IC IS CONFIGURED IT ONLY DELIVERS  100MHA, IT IS ENOUGHT FOR FLASHING TURNING ON THE MCU AND THE LED, BUT THE OLED SCREEN WILL NOT WORK PROPERLY.
ADD A BATTERY IF YOU INTEND TO USE THE OLED SCEEN WITH THE DEVICE, OR CHANGE THE R1 RESISTOR VALUE TO A 0603 2K OHM.

## Windows 10/11 tutorial:

*If you have Avrdudess working under linux or Mac, it also should work, CH340E drivers are required.

## Drivers:
Download CH341SER.EXE windows drivers from: https://www.wch-ic.com/products/CH340.html?

## Avrdudess method (recomended):

Download all apps and games precompiled from releases: https://github.com/LED-BOY/Ledboy-MINI/releases

Download Avrdudess: https://github.com/ZakKemble/AVRDUDESS

Make sure all your settings are the same:

The only thing it may be different is your COM port.
If you don't have any other serial device, the only com port available should be your Ledboy 2,
if not, unplug your Ledboy 2 and see what port is missing, then reconnect you Ledboy 2, that is yours Ledboy 2 COM port.
Make sure you select the .hex file in the Flash slot.

![Arduboy Flash](https://github.com/user-attachments/assets/a127568a-98b8-4a76-89c8-585375451b3f)

## Flash from source:
To Flash from source and make changes to the code you need to install a few things.

1- Legacy arduino software 1.8.19 works well: https://www.arduino.cc/en/software

2- Follow instll instruction to add attiny boards to Arduino IDE: https://github.com/SpenceKonde/megaTinyCore

3- Istall Tiny i2c to librarys, needed to compile: https://github.com/technoblogy/tiny-i2c

Once you have all installed intro the Arduio IDE you can start configuring the board parameters.
Com port should be the one available when you connect your ledboy 2.

![Arduino flashing](https://github.com/user-attachments/assets/aaa7249f-504b-4a52-8b88-f2495a363767)

You can check if the program compiles using the check mark, keep in mind that this MCU only has 4096 bytes of flash.

![compiling](https://github.com/user-attachments/assets/430df620-a212-444f-8175-08b5f0bdbdc2)

To upload the program to your Ledboy Mini, click the right arrow.

# The battery situation

![battery](https://github.com/user-attachments/assets/2af62154-1f2b-4bde-8878-27d9f8a527b7)

The recommended battery is: 302030 LIPO 3.7v 120mhs, it gives a nice battery duration, it fit nicely, and the batt charger IC is configured to charge at 100mha aprox.
Which is adequate for this battery capacity.

Make sure to be carfull when soldering the battery, to not short the contacts, is better to solder the positive first then negative.

## Configure the charger IC

If you whant to add a bigger battery you can change the R1 resistor to a 0603 2k ohm, that will give you the max 500 mha that the charger is capable to deliver. 
