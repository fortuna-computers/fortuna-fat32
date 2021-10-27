/* LAYER 0 of FatFS -- implemented here are functions to deal with:
 *
 *   - interpreting the MBR and boot sector
 *   - partitions
 *   - sector loading/writing
 *   - cluster loading/writing
 *
 * All functions here are PRIVATE.
 */

#ifndef FORTUNA_FAT32_IO_H
#define FORTUNA_FAT32_IO_H

#include <stdint.h>

#include "ffat32.h"

#define IO_BOOT_SECTOR 0

typedef struct FFatBPB {
    uint8_t    sectors_per_cluster;
    uint8_t    number_of_fats;
    uint16_t   reserved_sectors;
    uint32_t   fat_size_sectors;
    uint32_t   root_dir_cluster_ptr;
} FFatBPB;

FFatResult io_init(FFat32* f, FFatBPB* fat_bpb);

FFatResult io_read_raw_sector(FFat32* f, uint64_t sector);
FFatResult io_write_raw_sector(FFat32* f, uint64_t sector);

#endif //FORTUNA_FAT32_IO_H
