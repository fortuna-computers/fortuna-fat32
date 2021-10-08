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
    F_FREE_R  = 0x12,

    // directory operations
    F_DIR     = 0x20,
    F_MKDIR   = 0x21,
    F_RMDIR   = 0x22,
    F_CD      = 0x23,

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

typedef enum {
    F_OK                        = 0x0,
    F_MORE_DATA                 = 0x1,
    F_INCORRECT_OPERATION       = 0x2,
    F_NOT_FAT_32                = 0x3,
    F_BYTES_PER_SECTOR_NOT_512  = 0x4,
} FFatResult;

typedef enum {
    F_START_OVER = 0,
    F_CONTINUE   = 1,
} FContinuation;

typedef struct {
    FFatResult F_RSLT;    // result of the last operation
    uint32_t   F_NXTC;    // keeps state for multi-call operations (such as F_DIR, F_READ, F_WRITE): next cluster
    uint32_t   F_NXTS;    // keeps state for multi-call operations (such as F_DIR, F_READ, F_WRITE): next sector
} FFatRegisters;

typedef struct {
    uint8_t*      buffer;   // 512 bytes
    void*         data;
    bool          (*write)(uint32_t block, uint8_t const* buffer, void* data);   // implement this
    bool          (*read)(uint32_t block, uint8_t* buffer, void* data);          // implement this
    FFatRegisters reg;
} FFat32;

#ifdef __cplusplus
extern "C" {
#endif

FFatResult f_fat32(FFat32* f, FFat32Op operation);

#ifdef __cplusplus
}
#endif

#endif
