#ifndef FORTUNA_FAT32_H_
#define FORTUNA_FAT32_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    // initialization
    F_INIT    = 0x00,
    
    // disk operations
    F_LABEL   = 0x10,
    F_FREE    = 0x11,
    F_FREE_RECALCUATE = 0x12,

    // directory operations
    F_CD      = 0x20,
    F_DIR     = 0x21,
    F_MKDIR   = 0x22,

    // file operations
    F_OPEN    = 0x30,
    F_CLOSE   = 0x31,
    F_READ    = 0x32,
    F_WRITE   = 0x33,

    // dir/file operations
    F_STAT    = 0x40,
    F_RM      = 0x41,
    F_MV      = 0x42,
} FFat32Op;

typedef struct {
    uint8_t* buffer;   // 512 bytes
    void*    data;
    bool     (*write)(uint32_t block, uint8_t const* buffer, void* data);   // implement this
    bool     (*read)(uint32_t block, uint8_t* buffer, void* data);          // implement this
} FFat32Def;

#ifdef __cplusplus
extern "C" {
#endif

uint8_t f_fat32(FFat32Def* def, FFat32Op operation);

#ifdef __cplusplus
}
#endif

#endif
