#include "ffat32.h"

#include <string.h>

/***********************/
/*  LOCATIONS ON DISK  */
/***********************/

#define MBR               0
#define BOOT_SECTOR       0
#define FSINFO_SECTOR     1

#define PARTITION_TABLE_1       0x1c6

#define BPB_BYTES_PER_SECTOR      0xb
#define BPB_SECTORS_PER_CLUSTER   0xd
#define BPB_RESERVED_SECTORS      0xe
#define BPB_NUMBER_OF_FATS       0x10
#define BPB_TOTAL_SECTORS_16     0x13
#define BPB_TOTAL_SECTORS        0x20
#define BPB_FAT_SIZE_SECTORS     0x24
#define BPB_ROOT_DIR_CLUSTER     0x2c
#define BPB_LABEL                0x47
#define FSI_FREE_COUNT          0x1e8

#define LABEL_SZ           11

#define BYTES_PER_SECTOR  512
#define DIR_STRUCT_SZ      32
#define FILENAME_SZ        11

#define FAT_EOF    0x0fffffff
#define FAT_EOC    0x0ffffff8

#define DIR_FILENAME      0x0
#define DIR_ATTR          0xb
#define DIR_CLUSTER_HIGH 0x14
#define DIR_CLUSTER_LOW  0x1a

#define ATTR_DIR     0x10


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
    f->read(sector + f->reg.partition_start, f->buffer, f->data);
}

static void load_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    load_sector(f, cluster * f->reg.sectors_per_cluster + sector);
}

static void write_sector(FFat32* f, uint32_t sector)
{
    f->write(sector + f->reg.partition_start, f->buffer, f->data);
}

static uint32_t find_next_cluster_on_fat(FFat32* f, uint32_t cluster)
{
    uint32_t cluster_loc = cluster * 4;
    uint32_t sector_to_load = cluster_loc / BYTES_PER_SECTOR;
    
    load_sector(f, f->reg.fat_sector_start + sector_to_load);
    
    return from_32(f->buffer, cluster_loc % BYTES_PER_SECTOR);
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
        f->reg.partition_start = from_32(f->buffer, PARTITION_TABLE_1);
        load_sector(f, BOOT_SECTOR);
    } else {
        f->reg.partition_start = 0;
    }
    
    // fill out fields
    f->reg.sectors_per_cluster = f->buffer[BPB_SECTORS_PER_CLUSTER];
    f->reg.fat_sector_start = from_16(f->buffer, BPB_RESERVED_SECTORS);
    f->reg.fat_size_sectors = from_32(f->buffer, BPB_FAT_SIZE_SECTORS);
    
    uint8_t number_of_fats = f->buffer[BPB_NUMBER_OF_FATS];
    f->reg.data_start_cluster = (f->reg.fat_sector_start + (number_of_fats * f->reg.fat_size_sectors)) / f->reg.sectors_per_cluster;
    
    // check if bytes per sector is correct
   uint16_t bytes_per_sector = from_16(f->buffer, BPB_BYTES_PER_SECTOR);
    if (bytes_per_sector != BYTES_PER_SECTOR)
        return F_BYTES_PER_SECTOR_NOT_512;
    
    // check if it is FAT32
    uint16_t total_sectors_16 = from_16(f->buffer, BPB_TOTAL_SECTORS_16);
    uint32_t total_sectors_32 = from_32(f->buffer, BPB_TOTAL_SECTORS);
    if (total_sectors_16 != 0 || total_sectors_32 == 0)
        return F_NOT_FAT_32;
    
    // find root directory
    uint32_t root_dir_cluster_ptr = from_32(f->buffer, BPB_ROOT_DIR_CLUSTER);
    f->reg.root_dir_cluster = root_dir_cluster_ptr;
    f->reg.current_dir_cluster = f->reg.root_dir_cluster;
    
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
    memcpy(f->buffer, &f->buffer[BPB_LABEL], LABEL_SZ);
    for (int8_t i = LABEL_SZ - 1; i >= 0; --i) {
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
    uint32_t pos = f->reg.fat_sector_start;
    for (uint32_t i = 0; i < f->reg.fat_size_sectors; ++i) {
        load_sector(f, pos + i);
        for (uint32_t j = 0; j < BYTES_PER_SECTOR / 4; ++j) {
            uint32_t pointer = from_32(f->buffer, j * 4);
            if (pointer == 0x0)
                ++total;
        }
    }
    
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

// region `f_dir` ...

static FFatResult f_dir(FFat32* f)
{
    FFatResult result;
    uint32_t cluster;
    uint16_t sector;
    
    // find current cluster and sector
    if (f->buffer[0] == F_START_OVER) {   // user has requested to start reading a new directory
        cluster = f->reg.current_dir_cluster;
        sector = 0;
    } else {   // user is continuing to read a directory that was started in a previous call
        cluster = f->reg.state_next_cluster;
        sector = f->reg.state_next_sector;
    }
    
    // move to next cluster and/or sector
    uint32_t next_cluster, next_sector;
    if (sector >= (f->reg.sectors_per_cluster - 1U)) {
        next_cluster = find_next_cluster_on_fat(f, cluster);
        next_sector = 0;
    } else {
        next_cluster = cluster;
        next_sector = sector + 1;
    }
    
    // feed F_NXT registers
    if (next_cluster == FAT_EOC || next_cluster == FAT_EOF) {
        f->reg.state_next_cluster = f->reg.state_next_sector = 0;
        result = F_OK;
    } else {
        f->reg.state_next_cluster = next_cluster;
        f->reg.state_next_sector = next_sector;
        result = F_MORE_DATA;
    }
    
    // load directory data form sector into buffer
    load_cluster(f, cluster + f->reg.data_start_cluster - 2, sector);
    
    // check if we really have more data to read (the last dir in array is not null)
    if (result == F_MORE_DATA && f->buffer[BYTES_PER_SECTOR - DIR_STRUCT_SZ] == '\0') {
        f->reg.state_next_cluster = f->reg.state_next_sector = 0;
        result = F_OK;
    }
    
    return result;
}

// endregion

// region `f_cd` ...

static FFatResult f_cd(FFat32* f)
{
    char filename[FILENAME_SZ + 1] = { 0 };
    strncpy(filename, (const char *) f->buffer, FILENAME_SZ);
    uint8_t file_len = strlen(filename);
    
    // load current directory
    FFatResult result;
    f->buffer[0] = F_START_OVER;
    do {
        // read directory
        result = f_dir(f);
        if (result != F_OK && result != F_MORE_DATA)
            return result;
        
        // iterate through returned files
        for (uint16_t i = 0; i < (BYTES_PER_SECTOR / DIR_STRUCT_SZ); ++i) {
            uint16_t addr = i * DIR_STRUCT_SZ;
            uint8_t attr = f->buffer[addr + DIR_ATTR];   // attribute should be 0x10
            
            // if directory is found
            if ((attr & ATTR_DIR)
            && strncmp(filename, (const char *) &f->buffer[addr + DIR_FILENAME], file_len) == 0) {
                
                // set directory cluster
                uint32_t cluster =
                      *(uint16_t *) &f->buffer[addr + DIR_CLUSTER_LOW]
                    | ((uint32_t) (*(uint16_t *) &f->buffer[addr + DIR_CLUSTER_HIGH]) << 8);
                f->reg.current_dir_cluster = cluster;
                return F_OK;
            }
        }
        
        f->buffer[0] = F_CONTINUE;  // in next fetch, continue the previous one
    
    } while (result == F_MORE_DATA);
    
    // the directory was not found
    return F_INEXISTENT_DIRECTORY;
}

// endregion

/*****************/
/*  MAIN METHOD  */
/*****************/

FFatResult f_fat32(FFat32* f, FFat32Op operation)
{
    switch (operation) {
        case F_INIT:   f->reg.last_operation_result = f_init(f);   break;
        case F_FREE:   f->reg.last_operation_result = f_free(f);   break;
        case F_FREE_R: f->reg.last_operation_result = f_free_r(f); break;
        case F_LABEL:  f->reg.last_operation_result = f_label(f);  break;
        case F_DIR:    f->reg.last_operation_result = f_dir(f);    break;
        case F_CD:     f->reg.last_operation_result = f_cd(f);     break;
        case F_MKDIR: break;
        case F_RMDIR: break;
        case F_OPEN: break;
        case F_CLOSE: break;
        case F_READ: break;
        case F_WRITE: break;
        case F_STAT: break;
        case F_RM: break;
        case F_MV: break;
        // TODO - find file
        default: f->reg.last_operation_result = F_INCORRECT_OPERATION;
    }
    return f->reg.last_operation_result;
}