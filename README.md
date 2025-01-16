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

# Assembly guide

[Ledboy mini quick assembly guide.pdf](https://github.com/user-attachments/files/18432793/Ledboy.mini.quick.assembly.guide.pdf)


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

Add the new library: tiny-i2c-master.zip

![Sin título](https://github.com/user-attachments/assets/854a74bd-942c-4249-bd1e-0438a1baa2de)


Once you have all installed intro the Arduio IDE you can start configuring the board parameters.
Com port should be the one available when you connect your ledboy mini.

DON'T FORGET TO BURN BOOTLOADER BEFORE FLASHING IF USING THIS OPTION.

![Arduino flashing](https://github.com/user-attachments/assets/aaa7249f-504b-4a52-8b88-f2495a363767)

You can check if the program compiles using the check mark, keep in mind that this MCU only has 4096 bytes of flash.

![compiling](https://github.com/user-attachments/assets/430df620-a212-444f-8175-08b5f0bdbdc2)

To upload the program to your Ledboy Mini, click the right arrow.

# Assemble the screen and sensor if needed.

![Ledboy 2 V1 4 REV B v2](https://github.com/user-attachments/assets/519a719d-0682-4ba5-9a2b-10de1a844c91)
You need to use a 4 pin header to solder the oled screen.

Photo for reference.
![20241128_201355](https://github.com/user-attachments/assets/1415ffb6-ca2e-4dc7-bcc1-2d2d48654e48)

# Strap options

22mm recommended Hook & Loop Nylon Strap

[https://www.aliexpress.com/item/1005002060362335.html?spm=a2g0o.order_list.order_list_main.5.31e7194dIRHE37&gatewayAdapt=glo2esp](https://www.aliexpress.com/item/1005005994868690.html?spm=a2g0o.productlist.main.65.7f895BIJ5BIJqm&algo_pvid=f134d148-ab03-483a-bcb3-0b9b4a0c6f23&algo_exp_id=f134d148-ab03-483a-bcb3-0b9b4a0c6f23-32&pdp_npi=4%40dis%21USD%211.99%211.79%21%21%211.99%211.79%21%40212a70c117356048127567774e2ff7%2112000035222841702%21sea%21UY%212102966712%21X&curPageLogUid=hNslSdu6VDTG&utparam-url=scene%3Asearch%7Cquery_from%3A)

![Sin título](https://github.com/user-attachments/assets/9ca4a953-21a9-4b3b-bdb0-2aa92d4c28c6)


# The battery situation

![battery](https://github.com/user-attachments/assets/2af62154-1f2b-4bde-8878-27d9f8a527b7)

The recommended battery is: 302030 LIPO 3.7v 120mhs, it gives a nice battery duration, it fit nicely, and the batt charger IC is configured to charge at 100mha aprox.
Which is adequate for this battery capacity.

Make sure to be carfull when soldering the battery, do not short the contacts, is better to solder the positive first (red wire), then negative.

## Configure the charger IC

If you whant to add a bigger battery you can change the R1 resistor to a 0603 2k ohm, that will give you the max 500 mha that the charger is capable to deliver. 
