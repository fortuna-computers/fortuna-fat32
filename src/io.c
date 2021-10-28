#include "io.h"

#include "common.h"

#define MBR_SECTOR                  0
#define PARTITION_TABLE_1       0x1c6

#define BPB_BYTES_PER_SECTOR      0xb
#define BPB_SECTORS_PER_CLUSTER   0xd
#define BPB_RESERVED_SECTORS      0xe
#define BPB_NUMBER_OF_FATS       0x10
#define BPB_TOTAL_SECTORS_16     0x13
#define BPB_TOTAL_SECTORS        0x20
#define BPB_FAT_SIZE_SECTORS     0x24
#define BPB_ROOT_DIR_CLUSTER     0x2c

static uint32_t partition_starting_sector = 0;

FFatResult io_init(FFat32* f, FFatBPB* fat_bpb)
{
    // check partition location
    if (!f->read(MBR_SECTOR, f->buffer, f->data))
        return F_IO_ERROR;
    if (f->buffer[0] == 0xeb) {  // this is a FAT partition
        partition_starting_sector = 0;
    } else {   // this is a MBR
        partition_starting_sector = BUF_GET32(f, PARTITION_TABLE_1);
        TRY(io_read_raw_sector(f, IO_BOOT_SECTOR))
    }
    
    // check if bytes per sector is correct
    uint16_t bytes_per_sector = BUF_GET16(f, BPB_BYTES_PER_SECTOR);
    if (bytes_per_sector != BYTES_PER_SECTOR)
        return F_BYTES_PER_SECTOR_NOT_512;
    
    // check if it is FAT32
    uint16_t total_sectors_16 = BUF_GET16(f, BPB_TOTAL_SECTORS_16);
    uint32_t total_sectors_32 = BUF_GET32(f, BPB_TOTAL_SECTORS);
    if (total_sectors_16 != 0 || total_sectors_32 == 0)
        return F_NOT_FAT_32;
    
    fat_bpb->sectors_per_cluster = BUF_GET8(f, BPB_SECTORS_PER_CLUSTER);
    fat_bpb->number_of_fats = BUF_GET8(f, BPB_NUMBER_OF_FATS);
    fat_bpb->fat_size_sectors = BUF_GET32(f, BPB_FAT_SIZE_SECTORS);
    fat_bpb->reserved_sectors = BUF_GET16(f, BPB_RESERVED_SECTORS);
    fat_bpb->root_dir_cluster = BUF_GET32(f, BPB_ROOT_DIR_CLUSTER);
    
    if (fat_bpb->sectors_per_cluster == 0)
        return F_NOT_FAT_32;
    
    return F_OK;
}

FFatResult io_read_raw_sector(FFat32* f, uint64_t sector)
{
    sector += partition_starting_sector;
    return f->read(sector, f->buffer, f->data) ? F_OK : F_IO_ERROR;
}

FFatResult io_write_raw_sector(FFat32* f, uint64_t sector)
{
    sector += partition_starting_sector;
    return f->write(sector, f->buffer, f->data) ? F_OK : F_IO_ERROR;
}

#ifdef FFAT_DEBUG
#include <stdio.h>

void io_debug(FFat32* f)
{
    printf("Partition starting sector: 0x%X\n", partition_starting_sector);
}
#endif
