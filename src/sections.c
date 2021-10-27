#include "sections.h"

#include "common.h"
#include "io.h"

#define FSINFO_SECTOR       1
#define FSI_FREE_COUNT  0x1e8
#define FSI_NEXT_FREE   0x1ec

#define FAT_CLUSTER_FREE    0x0

static uint16_t fat_sector_start;
static uint32_t fat_sector_size;
static uint8_t  number_of_fats;
static uint32_t data_sector_start;
static uint8_t  sectors_per_cluster;

FFatResult sections_init(FFat32* f)
{
    FFatBPB fat_bpb;
    TRY(io_init(f, &fat_bpb))
    
    fat_sector_start = fat_bpb.reserved_sectors;
    fat_sector_size = fat_bpb.fat_size_sectors;
    number_of_fats = fat_bpb.number_of_fats;
    data_sector_start = fat_sector_start + (number_of_fats * fat_sector_size);
    
    sectors_per_cluster = fat_bpb.sectors_per_cluster;
    
    return F_OK;
}

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
