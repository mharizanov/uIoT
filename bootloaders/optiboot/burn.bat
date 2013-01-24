avrdude -c usbtiny -p m328p -e -u -U lock:w:0x3f:m -U efuse:w:0x05:m -U hfuse:w:0xDE:m -U lfuse:w:0xFF:m
avrdude -c usbtiny -p m328p -U flash:w:optiboot_uiot.hex -U lock:w:0x0f:m
