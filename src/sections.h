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

FFatResult sections_init(FFat32* f, uint32_t* current_dir_cluster);
FFatResult sections_load_boot_sector(FFat32* f);

typedef struct {
    uint32_t next_free_cluster;
    uint32_t free_cluster_count;
} FSInfo;

FFatResult sections_fsinfo_read(FFat32* f, FSInfo* fsinfo);
FFatResult sections_fsinfo_recalculate(FFat32* f, FSInfo* fsinfo);

FFatResult sections_load_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector);

FFatResult sections_fat_calculate_next_cluster_sector(FFat32* f, uint32_t* cluster, uint16_t* sector);

#ifdef FFAT_DEBUG
void sections_debug(FFat32* f);
#endif

#endif //FORTUNA_FAT32_SECTIONS_H
