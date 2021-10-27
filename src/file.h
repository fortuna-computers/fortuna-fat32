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

#define DIR_ENTRIES_PER_SECTOR  ((uint16_t) (BYTES_PER_SECTOR / sizeof(FDirEntry)))   /* 64 */
#define DIR_ENTRY_EOF   0x00
#define DIR_ENTRY_FREE  0xe5

FFatResult file_init(FFat32* f);

FFatResult file_list_current_dir(FFat32* f, FContinuation continuation);

#ifdef FFAT_DEBUG
void file_debug(FFat32* f);
#endif

#endif //FORTUNA_FAT32_FILE_H
