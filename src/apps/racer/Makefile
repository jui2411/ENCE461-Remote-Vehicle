PERIPHERALS = pwm pit adc
DRIVERS = usb_serial pacer rf ledtape button
TARGET = racer.bin

SRC = racer.c
OPT = -O2

ifneq ($(BOARD), racer)
$(error BOARD should be set to racer!)
endif

include ../../boards/board.mk
