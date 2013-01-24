avrdude -c usbtiny -p m328p -e -u -U lock:w:0x3f:m -U efuse:w:0x05:m -U hfuse:w:0xD2:m -U lfuse:w:0xFF:m
avrdude -c usbtiny -p m328p -U flash:w:2boots-arduino-atmega328p-6250000L-PC1.hex -U lock:w:0x0f:m
