#include "file.h"

#include "common.h"
#include "io.h"
#include "sections.h"

uint32_t current_dir_cluster = 0;

uint32_t next_cluster = 0;
uint16_t next_sector = 0;

FFatResult file_init(FFat32* f)
{
    TRY(sections_init(f, &current_dir_cluster))
    return F_OK;
}

FFatResult file_list_dir(FFat32* f, uint32_t initial_cluster, FContinuation continuation)
{
    // load sector into buffer
    if (continuation == F_START_OVER) {
        next_cluster = initial_cluster;
        next_sector = 0;
    }
    TRY(sections_load_data_cluster(f, next_cluster, next_sector))
    
    // check if directory ends in this sector
    FDirEntry* dir_entry = (FDirEntry *) f->buffer;
    for (uint16_t i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
        if (dir_entry[i].name[0] == DIR_ENTRY_EOF)
            return F_OK;
    }
    
    // since the directory continues in the next sector, store the next sector/cluster and return F_MORE_DATA
    TRY(sections_fat_calculate_next_cluster_sector(f, &next_cluster, &next_sector))
    return F_MORE_DATA;
}

FFatResult file_list_current_dir(FFat32* f, FContinuation continuation)
{
    return file_list_dir(f, current_dir_cluster, continuation);
}

#ifdef FFAT_DEBUG
void file_debug(FFat32* f)
{
}
#endif
