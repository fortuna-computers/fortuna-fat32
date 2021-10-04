FORTUNA_FAT32 = src/ffat32.o
CFLAGS = -std=c11
CPPFLAGS = -Wall -Wextra -O2
CXXFLAGS = -std=c++17
MCU = atmega16

all: ftest

#ftest: CPPFLAGS += -g -O0
ftest: ${FORTUNA_FAT32} test/test.o test/scenario.o test/scenario_images.o test/minilzo-2.10/minilzo.c
	g++ $^ -o $@
.PHONY: ftest

size: CC=avr-gcc
size: CPPFLAGS += -Os -mmcu=${MCU} -ffunction-sections -fdata-sections -mcall-prologues
size: ${FORTUNA_FAT32} size/size.o
	avr-gcc -mmcu=${MCU} -o $@.elf $^ -Wl,--gc-sections
	avr-size -C --mcu=${MCU} $@.elf

gen-headers: ftest
	./ftest -g > gen-headers.sh && \
	chmod +x gen-headers.sh && \
	./gen-headers.sh && \
	rm gen-headers.sh
.PHONY: gen-headers

clean-headers:
	rm -rf test/imghdr
.PHONY: clean-headers

clean:
	rm -f **/*.o ftest size.elf
.PHONY: clean

# vim: ts=8:sts=8:sw=8:noexpandtab
