#include "sections.h"

#include "common.h"
#include "io.h"

#define FSINFO_SECTOR       1
#define FSI_FREE_COUNT  0x1e8
#define FSI_NEXT_FREE   0x1ec

static uint16_t fat_sector_start = 0;
static uint32_t fat_sector_size  = 0;
static uint8_t  number_of_fats   = 0;
static uint32_t data_sector_start = 0;
static uint8_t  sectors_per_cluster = 0;

/********************/
/*  INITIALIZATION  */
/********************/

FFatResult sections_init(FFat32* f, uint32_t* root_dir_cluster)
{
    FFatBPB fat_bpb;
    TRY(io_init(f, &fat_bpb))
    
    fat_sector_start = fat_bpb.reserved_sectors;
    fat_sector_size = fat_bpb.fat_size_sectors;
    number_of_fats = fat_bpb.number_of_fats;
    data_sector_start = fat_sector_start + (number_of_fats * fat_sector_size);
    
    sectors_per_cluster = fat_bpb.sectors_per_cluster;
    
    *root_dir_cluster = fat_bpb.root_dir_cluster;
    
    return F_OK;
}

/*****************/
/*  BOOT/FSINFO  */
/*****************/

FFatResult sections_load_boot_sector(FFat32* f)
{
    TRY(io_read_raw_sector(f, IO_BOOT_SECTOR));
    return F_OK;
}

FFatResult sections_fsinfo_read(FFat32* f, FSInfo* fsinfo)
{
    TRY(io_read_raw_sector(f, FSINFO_SECTOR))
    fsinfo->free_cluster_count = BUF_GET32(f, FSI_FREE_COUNT);
    fsinfo->next_free_cluster = BUF_GET32(f, FSI_NEXT_FREE);
    return F_OK;
}

FFatResult sections_fsinfo_write(FFat32* f, FSInfo* fsinfo)
{
    TRY(io_read_raw_sector(f, FSINFO_SECTOR))
    BUF_SET32(f, FSI_FREE_COUNT, fsinfo->free_cluster_count)
    BUF_SET32(f, FSI_NEXT_FREE, fsinfo->next_free_cluster)
    TRY(io_write_raw_sector(f, FSINFO_SECTOR))
    return F_OK;
}

FFatResult sections_fsinfo_recalculate(FFat32* f, FSInfo* fsinfo)
{
    fsinfo->free_cluster_count = 0;
    fsinfo->next_free_cluster = FSI_INVALID;
    
    for (uint32_t sector = 0; sector < fat_sector_size; ++sector) {
        TRY(io_read_raw_sector(f, fat_sector_start + sector))
        uint32_t* fat = (uint32_t *) f->buffer;
        for (uint32_t i = 0; i < (BYTES_PER_SECTOR / sizeof(uint32_t)); ++i) {
            if (fat[i] == FAT_CLUSTER_FREE) {
                ++fsinfo->free_cluster_count;
                if (fsinfo->next_free_cluster == FSI_INVALID)
                    fsinfo->next_free_cluster = (sector * BYTES_PER_SECTOR) + i;
            }
        }
    }
    
    TRY(sections_fsinfo_write(f, fsinfo))
    
    return F_OK;
}

/*********/
/*  FAT  */
/*********/

FFatResult sections_fat_find_following_cluster(FFat32* f, uint32_t current_cluster, uint32_t* next_cluster)
{
    uint32_t cluster_ptr = current_cluster * 4;
    uint32_t sector_to_load = cluster_ptr / BYTES_PER_SECTOR;
    
    TRY(io_read_raw_sector(f, fat_sector_start + sector_to_load))
    
    *next_cluster = BUF_GET32(f, cluster_ptr % BYTES_PER_SECTOR);
    
    return F_OK;
}

FFatResult sections_fat_calculate_next_cluster_sector(FFat32* f, uint32_t* cluster, uint16_t* sector)
{
    ++(*sector);
    if (*sector >= sectors_per_cluster) {
        TRY(sections_fat_find_following_cluster(f, *cluster, cluster))
        *sector = 0;
    }
    return F_OK;
}

FFatResult sections_fat_reserve_cluster_for_new_file(FFat32* f, uint32_t* new_file_cluster_number)
{
    // TODO
    (void) f; (void) new_file_cluster_number;
    return F_OK;
}

/*******************/
/*  DATA CLUSTERS  */
/*******************/

FFatResult sections_load_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    TRY(io_read_raw_sector(f, data_sector_start + ((uint64_t) (cluster - 2) * sectors_per_cluster) + sector))
    return F_OK;
}

#ifdef FFAT_DEBUG
#include <stdio.h>

void sections_debug(FFat32* f)
{
    printf("Sectors per cluster: %d\n", sectors_per_cluster);
    printf("Number of FATs: %d\n", number_of_fats);
    printf("FAT sector start: 0x%X\n", fat_sector_start);
    printf("FAT sector size: 0x%X\n", fat_sector_size);
    printf("Data sector start: 0x%x\n", data_sector_start);
    printf("\n");
    printf("FAT:\n");
    for (uint32_t sector = 0; sector < fat_sector_size; ++sector) {
        io_read_raw_sector(f, fat_sector_start + sector);
        uint32_t* fat = (uint32_t *) f->buffer;
        bool all_free = true;
        for (uint32_t i = 0; i < (BYTES_PER_SECTOR / sizeof(uint32_t)); ++i) {
            if (i % 8 == 0) {
                all_free = true;
                printf("[%08X]   ", sector * 4 + i);
            }
            if (fat[i] != FAT_CLUSTER_FREE)
                all_free = false;
            printf("%08X ", fat[i]);
            if (i % 8 == 7) {
                printf("\n");
                if (all_free)
                    goto done;
            }
        }
    }
done:
    printf("\n");
}
#endif