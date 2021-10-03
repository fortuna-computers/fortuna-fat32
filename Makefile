FORTUNA_FAT32 = src/ffat32.o
CFLAGS = -std=c11
CPPFLAGS = -Wall -Wextra
CXXFLAGS = -std=c++17
MCU = atmega16

all: ftest

ftest: CPPFLAGS += -g -O0
ftest: ${FORTUNA_FAT32} test/test.o test/scenario.o
	g++ $^ -o $@
.PHONY: ftest

size: CC=avr-gcc
size: CPPFLAGS += -Os -mmcu=${MCU} -ffunction-sections -fdata-sections -mcall-prologues
size: ${FORTUNA_FAT32} size/size.o
	avr-gcc -mmcu=${MCU} -o $@.elf $^ -Wl,--gc-sections
	avr-size -C --mcu=${MCU} $@.elf

clean:
	rm -f **/*.o ftest size.elf
.PHONY: clean

# vim: ts=8:sts=8:sw=8:noexpandtab
