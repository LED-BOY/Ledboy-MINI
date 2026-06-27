/*
    This file is part of Game&Light Timer

    Foobar is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <https://www.gnu.org/licenses/>.

    To contact us: ledboy.net
    ledboyconsole@gmail.com

   SSD1306xLED - Drivers for SSD1306 controlled dot matrix OLED/PLED 128x32 displays

   @created: 2014-08-12
   @author: Neven Boyanov
   @date: 2020-04-14
   @author: Simon Merrett
   @version: 0.2

   Source code for original version available at: https://bitbucket.org/tinusaur/ssd1306xled
   Source code for ATtiny 0 and 1 series with megaTinyCore: https://github.com/SimonMerrett/tinyOLED
   megaTinyCore available at: https://github.com/SpenceKonde/megaTinyCore
   See tinyOLED.h for changelog since last version
*/

// ----------------------------------------------------------------------------

#include "tinyOLED.h"

SSD1306Device oled;

// ----------------------------------------------------------------------------

const uint8_t ssd1306_init_sequence[] = {

  0xAE,             // Display OFF
  0x20, 0b10,       // Page Addressing Mode

  0xC8,             // COM scan direction

  0x22,             // Page address range
  0,
  3,

  0x21,             // Column start/end
  0,
  127,

  0x40,             // Start line

  0x81, 0x8F,       // Contrast

  0xA1,             // Segment remap
  0xA6,             // Normal display

  0xA8, 0x1F,       // Multiplex ratio

  0xA4,             // Display follows RAM

  0xD3, 0x00,       // Display offset

  0xD5,             // Clock divide
  0xF0,

  0xD9, 0x22,       // Precharge

  0xDA, 0x02,       // COM pins config

  0xDB,
  0x20,

  0x8D, 0x14,       // Charge pump ON

  0xAF              // Display ON
};

// ----------------------------------------------------------------------------

inline void cmd(uint8_t c) {
  oled.ssd1306_send_command(c);
}

// ----------------------------------------------------------------------------

void SSD1306Device::begin(void) {

  TinyI2C.init();

  for (uint8_t i = 0; i < sizeof(ssd1306_init_sequence); i++) {
    cmd(ssd1306_init_sequence[i]);
  }
}

// ----------------------------------------------------------------------------

void SSD1306Device::sleep(void) {

  cmd(0xAE); // display OFF

  cmd(0x8D);
  cmd(0x10); // charge pump OFF
}

// ----------------------------------------------------------------------------

void SSD1306Device::ssd1306_send_command_start(void) {

  TinyI2C.start(SSD1306, 0);
  TinyI2C.write(0x00);
}

// ----------------------------------------------------------------------------

void SSD1306Device::ssd1306_send_command_stop(void) {

  TinyI2C.stop();
}

// ----------------------------------------------------------------------------

void SSD1306Device::ssd1306_send_data_byte(uint8_t byte) {

  TinyI2C.write(byte);
}

// ----------------------------------------------------------------------------

void SSD1306Device::ssd1306_send_command(uint8_t command) {

  ssd1306_send_command_start();

  TinyI2C.write(command);

  ssd1306_send_command_stop();
}

// ----------------------------------------------------------------------------

void SSD1306Device::ssd1306_send_data_start(void) {

  TinyI2C.start(SSD1306, 0);
  TinyI2C.write(0x40);
}

// ----------------------------------------------------------------------------

void SSD1306Device::ssd1306_send_data_stop(void) {

  TinyI2C.stop();
}

// ----------------------------------------------------------------------------

void SSD1306Device::setCursor(uint8_t x, uint8_t y) {

  ssd1306_send_command_start();

  TinyI2C.write(0xB0 + y);
  TinyI2C.write(((x & 0xF0) >> 4) | 0x10);
  TinyI2C.write((x & 0x0F) | 0x01);

  ssd1306_send_command_stop();
}

// ----------------------------------------------------------------------------

void SSD1306Device::drawLine(uint8_t x0,
                             uint8_t y0,
                             uint8_t lineLenght,
                             uint8_t dataValue) {

  setCursor(x0, y0);

  ssd1306_send_data_start();

  for (uint8_t x = 0; x < lineLenght; x++) {
    ssd1306_send_data_byte(dataValue);
  }

  ssd1306_send_data_stop();
}

// ----------------------------------------------------------------------------

void SSD1306Device::dimScreen(void) {

  cmd(0x81);
  cmd(0x05);
}

// ----------------------------------------------------------------------------

void SSD1306Device::brightScreen(void) {

  cmd(0x81);
  cmd(0xFF);
}

// ----------------------------------------------------------------------------

void SSD1306Device::clearScreen(void) {

  for (uint8_t x = 0; x < 4; x++) {
    drawLine(0, x, 128, 0x00);
  }
}

// ----------------------------------------------------------------------------

void SSD1306Device::drawSprite(uint8_t column,
                               uint8_t page,
                               const uint8_t* sprite,
                               uint8_t spriteHeightPixels,
                               uint8_t spriteWidthPages,
                               bool interlace) {

  for (uint8_t pageOffset = 0;
       pageOffset < spriteWidthPages;
       pageOffset++) {

    setCursor(column, page + pageOffset);

    ssd1306_send_data_start();

    const uint8_t* rowStart =
      sprite + (pageOffset * spriteHeightPixels);

    for (uint8_t i = 0; i < spriteHeightPixels; i++) {

      uint8_t byte = rowStart[i];

      ssd1306_send_data_byte(byte);

      if (interlace) {
        ssd1306_send_data_byte(byte);
      }
    }

    ssd1306_send_data_stop();
  }
}

// ----------------------------------------------------------------------------
