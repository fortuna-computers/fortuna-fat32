#include "sections.h"

#include "common.h"
#include "io.h"

#define FSINFO_SECTOR       1
#define FSI_FREE_COUNT  0x1e8
#define FSI_NEXT_FREE   0x1ec

static uint32_t fat_sector_start;
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
    data_sector_start = fat_sector_size + (number_of_fats * fat_sector_size);
    
    sectors_per_cluster = fat_bpb.sectors_per_cluster;
    
    return F_OK;
}

FFatResult sections_load_boot_sector(FFat32* f)
{
    TRY(io_read_raw_sector(f, IO_BOOT_SECTOR));
    return F_OK;
}

FFatResult sections_fsinfo_get(FFat32* f, FSInfo* fsinfo)
{
    TRY(io_read_raw_sector(f, FSINFO_SECTOR))
    fsinfo->free_cluster_count = BUF_GET32(f, FSI_FREE_COUNT);
    fsinfo->next_free_cluster = BUF_GET32(f, FSI_NEXT_FREE);
    return F_OK;
}

FFatResult sections_fsinfo_recalculate(FFat32* f, FSInfo* fsinfo)
{
}
