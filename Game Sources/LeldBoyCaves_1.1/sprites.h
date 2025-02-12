/*
    This file is part of LedBoyCaves x.x.

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
*/

const  uint8_t title[] = {
  0xff, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0xff, 0x81, 0x89, 0x89, 0x89, 0x81, 0x00,
  0x00, 0x00, 0xff, 0x81, 0x81, 0x81, 0x81, 0x00, 0x66, 0x3c, 0x00, 0x00, 0x00, 0xff, 0x81,
  0x81, 0x81, 0x89, 0x9e, 0x70, 0x00, 0x00, 0x7c, 0x66, 0x80, 0x81, 0x81, 0x81, 0x81, 0x01,
  0x66, 0x3e, 0x00, 0x00, 0x01, 0x06, 0x0c, 0xf0, 0x0c, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x7e, 0x00, 0x81, 0x81, 0x81, 0x81, 0x00, 0x00, 0x80, 0xc0, 0x70, 0x2e,
  0x23, 0x21, 0x2e, 0x38, 0xe0, 0x80, 0x01, 0x07, 0x0e, 0x70, 0xc0, 0xc0, 0x70, 0x0c, 0x07,
  0x01, 0x00, 0x00, 0xff, 0x81, 0x89, 0x89, 0x89, 0x81, 0x00, 0x00, 0x06, 0x8d, 0x89, 0x81,
  0x91, 0x70
};

const  uint8_t battIcon[] = {
  0x18, 0x3c, 0x24, 0x24, 0x24, 0x24, 0x24, 0x3c
};

const  uint8_t playerA[] = {
  0b00010001, 0b11111001, 0b11101111, 0b00111000, 0b00111000, 0b11101111, 0b11111001, 0b00010001
};

const  uint8_t playerB[] = {
  0b00001010, 0b00111010, 0b11101110, 0b11111000, 0b11111000, 0b11101110, 0b00111010, 0b00100010
};

const  uint8_t playerDead [] = {
 0x29,0xd9,0xdb,0x77,0x77,0xdb,0xd9,0x29
};
