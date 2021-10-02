FORTUNA_FAT32 = src/ffat32.o
CPPFLAGS = -std=c11 -g -O0

all: ftest

ftest: ${FORTUNA_FAT32} test/test.o
		gcc $^ -o $@

size:

clean:
	rm **/*.o

# vim: ts=8:sts=8:sw=8:noexpandtab
