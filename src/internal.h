#ifndef INTERNAL_H_
#define INTERNAL_H_

#include <stdint.h>

typedef struct {
    uint32_t partition_start;
    uint8_t  sectors_per_cluster;
    uint32_t fat_sector_start;
    uint32_t fat_size_sectors;
    uint32_t data_start_cluster;
    uint32_t root_dir_cluster;
    uint32_t current_dir_cluster;
} FFat32Variables;

#endif