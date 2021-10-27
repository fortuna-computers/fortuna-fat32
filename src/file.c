#include "file.h"

#include <string.h>

#include "common.h"
#include "io.h"
#include "sections.h"

static uint32_t current_dir_cluster = 0;
static uint32_t root_dir_cluster = 0;

static uint32_t next_cluster = 0;
static uint16_t next_sector = 0;

FFatResult file_init(FFat32* f)
{
    TRY(sections_init(f, &root_dir_cluster))
    current_dir_cluster = root_dir_cluster;
    return F_OK;
}

FFatResult file_list_dir(FFat32* f, uint32_t initial_cluster, FContinuation continuation)
{
    // load sector into buffer
    if (continuation == F_START_OVER) {
        next_cluster = initial_cluster;
        next_sector = 0;
    }
    if (next_cluster == FAT_EOF || next_cluster == FAT_EOC) {
        memset(f->buffer, 0, 512);
        return F_OK;
    }
    
    // since the directory continues in the next sector, store the next sector/cluster and return F_MORE_DATA
    uint32_t cluster = next_cluster;
    uint16_t sector = next_sector;
    TRY(sections_fat_calculate_next_cluster_sector(f, &next_cluster, &next_sector))
    
    // load the cluster
    TRY(sections_load_data_cluster(f, cluster, sector))
    
    // check if directory ends in this sector
    FDirEntry* dir_entry = (FDirEntry *) f->buffer;
    for (uint16_t i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
        if (dir_entry[i].name[0] == DIR_ENTRY_EOF)
            return F_OK;
    }
    
    return F_MORE_DATA;
}

FFatResult file_list_current_dir(FFat32* f, FContinuation continuation)
{
    return file_list_dir(f, current_dir_cluster, continuation);
}

#ifdef FFAT_DEBUG
#include <stdio.h>
#include <string.h>

void print_dir(FFat32* f, uint32_t cluster, const char* path)
{
    FContinuation continuation = F_START_OVER;
    FFatResult result;
    
    do {
        result = file_list_dir(f, cluster, continuation);
    
        FDirEntry entries[16];
        memcpy(entries, f->buffer, 512);
        for (size_t i = 0; i < 16; ++i) {
            if ((uint8_t) entries[i].name[0] == DIR_ENTRY_FREE) {
                continue;
            }
            if ((uint8_t) entries[i].name[0] == DIR_ENTRY_EOF)
                break;
            printf("%s%.8s.%.3s, A:%02X, size: %d, clusters: ", path, entries[i].name, &entries[i].name[8], entries[i].attrib, entries[i].file_size);
            uint32_t data_cluster = (((uint32_t) entries[i].cluster_high) << 8) | entries[i].cluster_low;
            if (data_cluster != 0) {
                do {
                    printf("%X ", data_cluster);
                    sections_fat_find_following_cluster(f, data_cluster, &data_cluster);
                } while (data_cluster != FAT_EOF && data_cluster != FAT_EOC);
            }
            printf("\n");
        }
        
        continuation = F_CONTINUE;
    } while (result == F_MORE_DATA);
}

void file_debug(FFat32* f)
{
    printf("Files in root:\n");
    print_dir(f, root_dir_cluster, "/");
}
#endif
