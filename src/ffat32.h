#ifndef FORTUNA_FAT32_H_
#define FORTUNA_FAT32_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum FFat32Op {
    // initialization
    F_INIT    = 0x00,
    
    // disk operations
    F_FREE    = 0x10,
    F_FREE_R  = 0x11,
    F_BOOT    = 0x12,

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

typedef enum FFatResult {
    F_OK                        = 0x0,
    F_MORE_DATA                 = 0x1,  // operation went OK but there's more data to fetch/save
    F_IO_ERROR                  = 0x2,  // the disk could not be accessed for some reason (error in read/write callbacks)
    F_INCORRECT_OPERATION       = 0x3,  // tried to call an operation that does not exist
    F_NOT_FAT_32                = 0x4,  // the filesystem is not FAT32
    F_BYTES_PER_SECTOR_NOT_512  = 0x5,  // BPB_BYTES_PER_SECTOR is not 512
    F_PATH_NOT_FOUND    = 0x6,  // file not found
    F_FILE_PATH_TOO_LONG        = 0x7,  // file path larger that 127
    F_INVALID_FILENAME          = 0x8,  // filename contains an invalid character
    F_DEVICE_FULL               = 0x9,  // no space left on device
} FFatResult;

typedef enum FContinuation {
    F_START_OVER = 0,
    F_CONTINUE   = 1,
} FContinuation;

typedef struct FFatRegisters {
    FFatResult last_operation_result : 8;
    uint8_t    sectors_per_cluster;
    uint8_t    number_of_fats;
    uint8_t    _unused;    // TODO
    
    uint32_t   partition_start;
    uint32_t   fat_sector_start;
    uint32_t   fat_size_sectors;
    uint32_t   data_sector_start;
    uint32_t   root_dir_cluster;
    uint32_t   current_dir_cluster;
    
    uint32_t   state_next_cluster;
    uint32_t   state_next_sector;
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

FFatResult f_fat32(FFat32* f, FFat32Op operation, uint32_t fat_datetime);

#ifdef __cplusplus
}
#endif

#endif
