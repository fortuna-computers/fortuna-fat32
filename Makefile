FORTUNA_FAT32 = src/io.o src/sections.o src/file.o src/ffat32.o
TEST_OBJ = test/main.o test/tests.o test/helper.o test/scenario.o test/diskio.o test/ff/ff.o \
	test/tags.o
CFLAGS = -std=c11
CPPFLAGS = -Wall -Wextra
CXXFLAGS = -std=c++17
MCU = atmega16
MAX_CODE_SIZE=8192

all: ftest

ftest: CPPFLAGS += -g -O0
ftest: ${FORTUNA_FAT32} ${TEST_OBJ}
	g++ $^ -o $@ `pkg-config --libs libbrotlicommon libbrotlidec`
.PHONY: ftest

test: ftest
	./ftest
.PHONY: ftest

test/tags.o: test/TAGS.TXT
	objcopy --input binary --output pe-x86-64 --binary-architecture i386:x86-64 $^ $@

size: CC=avr-gcc
size: CPPFLAGS += -Os -mmcu=${MCU} -ffunction-sections -fdata-sections -mcall-prologues
size: ${FORTUNA_FAT32} size/size.o
	avr-gcc -mmcu=${MCU} -o $@.elf $^ -Wl,--gc-sections
	avr-size -C --mcu=${MCU} $@.elf

check-code-size: size
	if [ "`avr-size -B --mcu=atmega16 size.elf | tail -n1 | tr -s ' ' | cut -d ' ' -f 2`" -gt "${MAX_CODE_SIZE}" ]; then \
		>&2 echo "Code is too big.";  \
		false; \
	fi

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
	rm -f ${FORTUNA_FAT32} ${TEST_OBJ} ftest size.elf size/size.o
.PHONY: clean

# vim: ts=8:sts=8:sw=8:noexpandtab
