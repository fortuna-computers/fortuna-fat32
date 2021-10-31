/* LAYER 2 of FatFS -- implemented here are functions to deal with:
 *
 *   - Files
 *   - Directory entries
 *
 * All functions here are PRIVATE.
 */

#ifndef FORTUNA_FAT32_FILE_H
#define FORTUNA_FAT32_FILE_H

#include "ffat32.h"

typedef struct __attribute__((__packed__)) {
    char     name[11];
    uint8_t  attrib;
    uint8_t  nt_res;
    uint8_t  time_tenth;
    uint32_t crt_datetime;
    uint16_t last_acc_time;
    uint16_t cluster_high;
    uint32_t wrt_datetime;
    uint16_t cluster_low;
    uint32_t file_size;
} FDirEntry;

typedef struct FCurrentSector {
    uint32_t cluster;
    uint16_t sector;
} FCurrentSector;

#define DIR_ENTRIES_PER_SECTOR  ((uint16_t) (BYTES_PER_SECTOR / sizeof(FDirEntry)))   /* 16 */
#define DIR_ENTRY_EOF   0x00
#define DIR_ENTRY_FREE  0xe5
#define FILENAME_SZ     11

#define ATTR_DIR         0x10
#define ATTR_ANY         0xff
#define ATTR_ARCHIVE     0x20

typedef uint8_t FILE_IDX;

// initialization

FFatResult file_init(FFat32* f);

// file

FFatResult file_open(FFat32* f, char* filename, FILE_IDX* file_idx);
FFatResult file_read(FFat32* f, FILE_IDX file_idx, uint16_t* file_sector_length);
FFatResult file_seek_forward(FFat32* f, FILE_IDX file_idx, uint32_t count, uint16_t* file_sector_length);
FFatResult file_seek_end(FFat32* f, FILE_IDX file_idx, uint16_t* bytes_in_sector);
FFatResult file_append_cluster(FFat32* f, FILE_IDX file_idx);
FFatResult file_close(FFat32* f, FILE_IDX file_idx);

// directory

FFatResult file_list_current_dir(FFat32* f, FContinuation continuation);
FFatResult file_list_dir(FFat32* f, uint32_t initial_cluster, FContinuation continuation, FCurrentSector* current_sector);
FFatResult file_change_current_dir(FFat32* f, const char* path);
FFatResult file_create_dir(FFat32* f, char* path, uint32_t fat_datetime);

// debug

#ifdef FFAT_DEBUG
void file_debug(FFat32* f);
#endif

#endif //FORTUNA_FAT32_FILE_H
