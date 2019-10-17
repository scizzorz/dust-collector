ARDUINO_DIR = /usr/share/arduino
ARDMK_DIR = /usr/share/arduino
AVR_TOOLS_DIR = /usr
AVRDUDE_CONF = /etc/avrdude.conf
ARDUINO_CORE_PATH = /usr/share/arduino/hardware/archlinux-arduino/avr/cores/arduino
BOARDS_TXT = /usr/share/arduino/hardware/archlinux-arduino/avr/boards.txt
ARDUINO_VAR_PATH = /usr/share/arduino/hardware/archlinux-arduino/avr/variants
BOOTLOADER_PARENT = /usr/share/arduino/hardware/archlinux-arduino/avr/bootloaders
ARDUINO_LIB_PATH = /usr/share/arduino/hardware/archlinux-arduino/avr/libraries


BOARD_TAG    = micro
ARDUINO_LIBS = Servo Adafruit_SSD1306 EEPROM Wire SPI Adafruit_GFX_Library

include /usr/share/arduino/Arduino.mk
