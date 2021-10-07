#include "ffat32.h"

#include <string.h>

#include "internal.h"

FFat32Variables var = { 0 };

/***********************/
/*  LOCATIONS ON DISK  */
/***********************/

#define MBR               0
#define BOOT_SECTOR       0
#define FSINFO_SECTOR     1

#define PARTITION_TABLE_1 0x1c6

#define BPB_BYTES_PER_SECTOR      0xb
#define BPB_SECTORS_PER_CLUSTER   0xd
#define BPB_RESERVED_SECTORS      0xe
#define BPB_NUMBER_OF_FATS       0x10
#define BPB_TOTAL_SECTORS_16     0x13
#define BPB_TOTAL_SECTORS        0x20
#define BPB_FAT_SIZE_SECTORS     0x24
#define BPB_ROOT_DIR_CLUSTER     0x2c
#define FSI_FREE_COUNT          0x1e8

#define BYTES_PER_SECTOR  512

/**********************/
/*  HELPER FUNCTIONS  */
/**********************/

//region ...

static uint16_t from_16(uint8_t const* buffer, uint16_t pos)
{
    return *(uint16_t *) &buffer[pos];
}

static uint32_t from_32(uint8_t const* buffer, uint16_t pos)
{
    return *(uint32_t *) &buffer[pos];
}

static void to_32(uint8_t* buffer, uint16_t pos, uint32_t value)
{
    *(uint32_t *) &buffer[pos] = value;
}

static void load_sector(FFat32* f, uint32_t sector)
{
    f->read(sector + var.partition_start, f->buffer, f->data);
}

static void load_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    load_sector(f, cluster * var.sectors_per_cluster + sector);
}

static void write_sector(FFat32* f, uint32_t sector)
{
    f->write(sector + var.partition_start, f->buffer, f->data);
}

static uint32_t find_next_cluster_on_fat(FFat32* f, uint32_t cluster)
{
    uint32_t sector = var.fat_sector_start + (cluster / 128);
    load_sector(f, sector);
    return from_32(f->buffer, cluster % 128);
}

//endregion

/********************/
/*  INITIALIZATION  */
/********************/

//region ...

static FFatResult f_init(FFat32* f)
{
    // check partition location
    f->read(MBR, f->buffer, f->data);
    if (f->buffer[0] == 0xfa) {  // this is a MBR
        var.partition_start = from_32(f->buffer, PARTITION_TABLE_1);
        load_sector(f, BOOT_SECTOR);
    } else {
        var.partition_start = 0;
    }
    
    // fill out fields
    var.sectors_per_cluster = f->buffer[BPB_SECTORS_PER_CLUSTER];
    var.fat_sector_start = from_16(f->buffer, BPB_RESERVED_SECTORS);
    var.fat_size_sectors = from_32(f->buffer, BPB_FAT_SIZE_SECTORS);
    
    uint8_t number_of_fats = f->buffer[BPB_NUMBER_OF_FATS];
    var.data_start_cluster = (var.fat_sector_start + (number_of_fats * var.fat_size_sectors)) / var.sectors_per_cluster;
    
    // check if bytes per sector is correct
   uint16_t bytes_per_sector = from_16(f->buffer, BPB_BYTES_PER_SECTOR);
    if (bytes_per_sector != 512)
        return F_BYTES_PER_SECTOR_NOT_512;
    
    // check if it is FAT32
    uint16_t total_sectors_16 = from_16(f->buffer, BPB_TOTAL_SECTORS_16);
    uint32_t total_sectors_32 = from_32(f->buffer, BPB_TOTAL_SECTORS);
    if (total_sectors_16 != 0 || total_sectors_32 == 0)
        return F_NOT_FAT_32;
    
    // find root directory
    uint32_t root_dir_cluster_ptr = from_32(f->buffer, BPB_ROOT_DIR_CLUSTER) - 2;
    var.root_dir_cluster = var.data_start_cluster + root_dir_cluster_ptr;
    var.current_dir_cluster = var.root_dir_cluster;
    
    return F_OK;
}

//endregion

/*********************/
/*  DISK OPERATIONS  */
/*********************/

//region ...

static FFatResult f_label(FFat32* f)
{
    load_sector(f, BOOT_SECTOR);
    memcpy(f->buffer, &f->buffer[0x47], 11);
    for (int8_t i = 10; i >= 0; --i) {
        if (f->buffer[i] == ' ')
            f->buffer[i] = '\0';
        else
            break;
    }
    
    return F_OK;
}

static FFatResult f_free_r(FFat32* f)
{
    uint32_t total = 0;
    
    // count free sectors on FAT
    uint32_t pos = var.fat_sector_start;
    for (uint32_t i = 0; i < var.fat_size_sectors; ++i) {
        load_sector(f, pos + i);
        for (uint32_t j = 0; j < BYTES_PER_SECTOR / 4; ++j) {
            uint32_t pointer = from_32(f->buffer, j * 4);
            if (pointer == 0x0)
                ++total;
        }
    }
    
    // TODO - update value in FSInfo
    load_sector(f, FSINFO_SECTOR);
    to_32(f->buffer, FSI_FREE_COUNT, total);
    write_sector(f, FSINFO_SECTOR);
    
    to_32(f->buffer, 0, total);
    
    return F_OK;
}

static FFatResult f_free(FFat32* f)
{
    load_sector(f, FSINFO_SECTOR);
    uint32_t free_ = from_32(f->buffer, FSI_FREE_COUNT);
    if (free_ == 0xffffffff)
        f_free_r(f);
    else
        to_32(f->buffer, 0, free_);
    
    return F_OK;
}

//endregion

/**************************/
/*  DIRECTORY OPERATIONS  */
/**************************/

// region ...

static FFatResult f_dir(FFat32* f)
{
    uint32_t cluster = var.current_dir_cluster;
    load_cluster(f, cluster, 0);  // TODO
    
    // TODO - continue
    
    return F_OK;
}

// endregion

/*****************/
/*  MAIN METHOD  */
/*****************/

FFatResult f_fat32(FFat32* f, FFat32Op operation)
{
    switch (operation) {
        case F_INIT:   return f_init(f);
        case F_FREE:   return f_free(f);
        case F_FREE_R: return f_free_r(f);
        case F_LABEL:  return f_label(f);
        case F_DIR:    return f_dir(f);
        case F_CD: break;
        case F_MKDIR: break;
        case F_RMDIR: break;
        case F_OPEN: break;
        case F_CLOSE: break;
        case F_READ: break;
        case F_WRITE: break;
        case F_STAT: break;
        case F_RM: break;
        case F_MV: break;
    }
    return F_INCORRECT_OPERATION;
}