FORTUNA_FAT32 = src/ffat32.o
CPPFLAGS = -std=c11 -Wall -Wextra
MCU = atmega16

all: ftest

ftest: CPPFLAGS += -g -O0
ftest: ${FORTUNA_FAT32} test/test.o
	gcc $^ -o $@

size: CC=avr-gcc
size: CPPFLAGS += -Os -mmcu=${MCU} -ffunction-sections -fdata-sections -mcall-prologues
size: ${FORTUNA_FAT32} size/size.o
	avr-gcc -mmcu=${MCU} -o $@.elf $^ -Wl,--gc-sections
	avr-size -C --mcu=${MCU} $@.elf

clean:
	rm **/*.o ftest

# vim: ts=8:sts=8:sw=8:noexpandtab
