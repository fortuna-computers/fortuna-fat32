/* LAYER 2 of FatFS -- implemented here are functions to deal with:
 *
 *   - Filesystem operations.
 *
 * All functions here are PUBLIC.
 */

#ifndef FORTUNA_FAT32_H_
#define FORTUNA_FAT32_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum FFat32Op {
    // initialization
    F_INIT           = 0x00,
    F_FSINFO_RECALC  = 0x01,
    
    // disk operations
    F_FREE    = 0x10,
    F_BOOT    = 0x11,

    // directory operations
    F_DIR     = 0x20,
    F_MKDIR   = 0x21,
    F_RMDIR   = 0x22,
    F_CD      = 0x23,

    // file operations
    F_OPEN      = 0x30,
    F_CREATE    = 0x31,
    F_CLOSE     = 0x32,
    F_SEEK      = 0x33,
    F_READ      = 0x34,
    F_OVERWRITE = 0x35,
    F_APPEND    = 0x36,

    // dir/file operations
    F_STAT    = 0x40,
    F_RM      = 0x41,
    F_MV      = 0x42,
} FFat32Op;

typedef enum FFatResult {
    F_OK                        = 0x00,
    F_MORE_DATA                 = 0x01,  // operation went OK but there's more data to fetch/save
    F_IO_ERROR                  = 0x02,  // the disk could not be accessed for some reason (error in read/write callbacks)
    F_INCORRECT_OPERATION       = 0x03,  // tried to call an operation that does not exist
    F_NOT_FAT_32                = 0x04,  // the filesystem is not FAT32
    F_BYTES_PER_SECTOR_NOT_512  = 0x05,  // BPB_BYTES_PER_SECTOR is not 512
    F_PATH_NOT_FOUND            = 0x06,  // file not found
    F_FILE_PATH_TOO_LONG        = 0x07,  // file path larger that 127
    F_INVALID_FILENAME          = 0x08,  // filename contains an invalid character
    F_DEVICE_FULL               = 0x09,  // no space left on device
    F_DIR_NOT_EMPTY             = 0x0a,  // trying to remove a non-empty directory
    F_NOT_A_DIRECTORY           = 0x0b,  // trying to remove a non-directory with rmdir
    F_FILE_ALREADY_EXISTS       = 0x0c,  // trying to create a file that already exists
    F_TOO_MANY_FILES_OPEN       = 0x0d,
    F_INVALID_FILE_INDEX        = 0x0e,
    F_FILE_NOT_OPEN             = 0x0f,
    F_SEEK_PAST_EOF             = 0x10,
} FFatResult;

typedef enum FContinuation {
    F_START_OVER = 0,
    F_CONTINUE   = 1,
} FContinuation;

typedef struct __attribute__((__packed__)) FFatRegisters {
    uint32_t   F_SZ;
    FFatResult F_RSLT : 8;
    uint8_t    F_FLN;
} FFatRegisters;

typedef struct FFat32 {
    uint8_t*      buffer;   // 512 bytes
    void*         data;
    bool          (*write)(uint32_t block, uint8_t const* buffer, void* data);   // implement this
    bool          (*read)(uint32_t block, uint8_t* buffer, void* data);          // implement this
    FFatRegisters reg;
} FFat32;

#ifdef __cplusplus
extern "C" {
#endif

FFatResult  f_fat32(FFat32* f, FFat32Op operation, uint32_t fat_datetime);
const char* f_error(FFatResult result);

#ifdef FFAT_DEBUG
    void f_fat32_debug(FFat32* f);
#endif

#ifdef __cplusplus
}
#endif

#endif
