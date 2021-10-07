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

#define BPB_SECTORS_PER_CLUSTER   0xd
#define FSI_FREE_COUNT          0x1e8

#define BYTES_PER_SECTOR  512

/**********************/
/*  HELPER FUNCTIONS  */
/**********************/

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

/********************/
/*  INITIALIZATION  */
/********************/

static void f_init(FFat32* f)
{
    f->read(MBR, f->buffer, f->data);
    
    if (f->buffer[0] == 0xfa) {  // this is a MBR
        var.partition_start = from_32(f->buffer, PARTITION_TABLE_1);
        load_sector(f, BOOT_SECTOR);
    } else {
        var.partition_start = 0;
    }
    
    var.sectors_per_cluster = f->buffer[BPB_SECTORS_PER_CLUSTER];
    // TODO - fill out fields
    
    // TODO - check if bytes per sector is correct
    // TODO - check if it is FAT32
}

/*********************/
/*  DISK OPERATIONS  */
/*********************/

static void f_label(FFat32* f)
{
    load_sector(f, BOOT_SECTOR);
    memcpy(f->buffer, &f->buffer[0x47], 11);
    for (int8_t i = 10; i >= 0; --i) {
        if (f->buffer[i] == ' ')
            f->buffer[i] = '\0';
        else
            break;
    }
}

static void f_free_r(FFat32* f)
{
}

static void f_free(FFat32* f)
{
    load_sector(f, FSINFO_SECTOR);
    uint32_t free_ = from_32(f->buffer, FSI_FREE_COUNT);
    if (free_ == 0xffffffff)
        f_free_r(f);
    else
        to_32(f->buffer, 0, free_ * var.sectors_per_cluster * BYTES_PER_SECTOR);
}

/*****************/
/*  MAIN METHOD  */
/*****************/


uint8_t f_fat32(FFat32* def, FFat32Op operation)
{
    switch (operation) {
        case F_INIT: f_init(def); break;
        case F_FREE: f_free(def); break;
        case F_FREE_R: f_free_r(def); break;
        case F_LABEL: f_label(def); break;
        case F_CD: break;
        case F_DIR: break;
        case F_MKDIR: break;
        case F_OPEN: break;
        case F_CLOSE: break;
        case F_READ: break;
        case F_WRITE: break;
        case F_STAT: break;
        case F_RM: break;
        case F_MV: break;
    }
    return 0;
}