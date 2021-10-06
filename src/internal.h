#ifndef INTERNAL_H_
#define INTERNAL_H_

#include <stdint.h>

typedef struct {
    uint32_t partition_start;
    uint8_t  sectors_per_cluster;
    uint32_t root_dir;
    uint32_t fat_start;
    uint32_t data_start;
} FFat32Variables;

#define BLK_SZ 512

#endif