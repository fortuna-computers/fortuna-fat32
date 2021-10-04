FORTUNA_FAT32 = src/ffat32.o
TEST_OBJ = test/test.o test/scenario.o \
	test/0.o test/1.o test/2.o test/3.o test/4.o test/5.o test/6.o
CFLAGS = -std=c11
CPPFLAGS = -Wall -Wextra -O3
CXXFLAGS = -std=c++17
MCU = atmega16

all: ftest

#ftest: CPPFLAGS += -g -O0
ftest: ${FORTUNA_FAT32} ${TEST_OBJ}
	g++ $^ -o $@ `pkg-config --libs libbrotlicommon libbrotlidec`
.PHONY: ftest

test/%.o: test/imghdr/%.img.br
	objcopy --input binary --output pe-x86-64 --binary-architecture i386:x86-64 $^ $@

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
