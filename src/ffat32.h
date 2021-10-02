#ifndef FORTUNA_FAT32_H_
#define FORTUNA_FAT32_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    // disk operations
    F_FREE    = 0x00,
    F_LABEL   = 0x01,

    // directory operations
    F_CD      = 0x10,
    F_DIR     = 0x11,
    F_MKDIR   = 0x12,

    // file operations
    F_OPEN    = 0x20,
    F_CLOSE   = 0x21,
    F_READ    = 0x22,
    F_WRITE   = 0x23,

    // dir/file operations
    F_STAT    = 0x30,
    F_RM      = 0x31,
    F_MV      = 0x32,
} FFat32Op;

typedef struct {
    uint8_t* buffer;
    bool     (*write)(uint32_t block, uint8_t* buffer);
    bool     (*read)(uint32_t block, uint8_t const* buffer);
} FFat32Def;

uint8_t f_fat32(FFat32Def* def, FFat32Op operation);

#endif
