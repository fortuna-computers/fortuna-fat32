#ifndef FORTUNA_FAT32_COMMON_H
#define FORTUNA_FAT32_COMMON_H

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error Sorry, only little endian platforms are supported right now.
#endif

#define TRY(expr) { FFatResult r = (expr); if (r != F_OK) return r; }

#define BUF_GET8(f, pos)  (f->buffer[pos])
#define BUF_GET16(f, pos) (*(uint16_t *) &f->buffer[pos])
#define BUF_GET32(f, pos) (*(uint32_t *) &f->buffer[pos])

#define BUF_SET8(f, pos, value)  { f->buffer[pos] = value; }
#define BUF_SET16(f, pos, value) { *(uint16_t *) &f->buffer[pos] = value; }
#define BUF_SET32(f, pos, value) { *(uint32_t *) &f->buffer[pos] = value; }

#define BYTES_PER_SECTOR 512

#endif //FORTUNA_FAT32_COMMON_H
