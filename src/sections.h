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

#define FAT_EOF    0x0fffffff
#define FAT_EOC    0x0ffffff8
#define FAT_CLUSTER_FREE    0x0

FFatResult sections_init(FFat32* f, uint32_t* root_dir_cluster);
FFatResult sections_load_boot_sector(FFat32* f);

typedef struct {
    uint32_t next_free_cluster;
    uint32_t free_cluster_count;
} FSInfo;

FFatResult sections_fsinfo_read(FFat32* f, FSInfo* fsinfo);
FFatResult sections_fsinfo_recalculate(FFat32* f, FSInfo* fsinfo);

FFatResult sections_fat_find_following_cluster(FFat32* f, uint32_t current_cluster, uint32_t* next_cluster);
FFatResult sections_fat_calculate_next_cluster_sector(FFat32* f, uint32_t* cluster, uint16_t* sector);
FFatResult sections_fat_reserve_cluster_for_new_file(FFat32* f, uint32_t* new_file_cluster_number);

FFatResult sections_load_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector);

#ifdef FFAT_DEBUG
void sections_debug(FFat32* f);
#endif

#endif //FORTUNA_FAT32_SECTIONS_H
