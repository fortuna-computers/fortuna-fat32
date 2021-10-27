/* LAYER 1 of FatFS -- implemented here are functions to deal with:
 *
 *   - reading/writing FSInfo
 *   - reading/writing FAT
 *   - reading/writing data clusters
 *
 * All functions here are PRIVATE.
 */

#ifndef FORTUNA_FAT32_SECTIONS_H
#define FORTUNA_FAT32_SECTIONS_H

#include "ffat32.h"

FFatResult sections_init(FFat32* f);
FFatResult sections_load_boot_sector(FFat32* f);

typedef struct {
    uint32_t next_free_cluster;
    uint32_t free_cluster_count;
} FSInfo;

FFatResult sections_fsinfo_get(FFat32* f, FSInfo* fsinfo);
FFatResult sections_fsinfo_recalculate(FFat32* f, FSInfo* fsinfo);

#endif //FORTUNA_FAT32_SECTIONS_H
