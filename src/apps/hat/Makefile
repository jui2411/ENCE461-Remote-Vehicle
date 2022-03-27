PERIPHERALS = pit adc
DRIVERS = rf pacer mpu usb_serial piezo button ledtape
TARGET = hat.bin 

SRC = hat.c
OPT = -O2

ifneq ($(BOARD), hat)
$(error BOARD should be set to hat!)
endif

include ../../boards/board.mk
