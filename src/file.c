#include "file.h"

#include <string.h>

#include "common.h"
#include "sections.h"

#define MAX_PATH_SZ 128

static uint32_t current_dir_cluster = 0;
static uint32_t root_dir_cluster = 0;

static uint32_t next_cluster = 0;
static uint16_t next_sector = 0;

static char path_copy[MAX_PATH_SZ] = {0};

typedef struct FPathLocation {
    uint32_t data_cluster;
    uint32_t parent_dir_cluster;
    uint16_t parent_dir_sector;
    uint16_t file_entry_in_parent_dir;
} FPathLocation;

FFatResult file_init(FFat32* f)
{
    TRY(sections_init(f, &root_dir_cluster))
    current_dir_cluster = root_dir_cluster;
    return F_OK;
}

FFatResult file_list_dir(FFat32* f, uint32_t initial_cluster, FContinuation continuation, FCurrentSector* current_sector)
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
    if (current_sector)
        *current_sector = (FCurrentSector) { cluster, sector };
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
    return file_list_dir(f, current_dir_cluster, continuation, NULL);
}

static FFatResult file_convert_filename_to_fat(const char* filename_start, const char* filename_end, char* result)
{
    memset(result, ' ', FILENAME_SZ);
    
    if (strcmp(result, ".") == 0) {
        result[0] = '.';
        return F_OK;
    } else if (strcmp(result, "..") == 0) {
        strcpy(result, "..");
        return F_OK;
    }
    
    uint8_t pos = 0, i = 0;
    while (pos < FILENAME_SZ) {
        if (filename_start[i] == 0 || i == filename_end - filename_start) {
            return F_OK;
        } else if (filename_start[i] == '.') {
            pos = FILENAME_SZ - 3;
            ++i;
        } else {
            result[pos++] = filename_start[i++];
        }
        // TODO - check for invalid chars
    }
    return F_OK;
}

static FFatResult find_file_in_dir(FFat32* f, const char* path_start, const char* path_end, uint32_t dir_cluster, uint8_t attrib_mask, FPathLocation* location)
{
    char fat_filename[11];
    TRY(file_convert_filename_to_fat(path_start, path_end, fat_filename))
    
    FContinuation continuation = F_START_OVER;
    FFatResult result;
    do {
        FCurrentSector current_sector;
        result = file_list_dir(f, dir_cluster, continuation, &current_sector);
        if (result != F_OK && result != F_MORE_DATA)
            return result;
    
        FDirEntry* entries = (FDirEntry *) f->buffer;
        for (uint16_t i = 0; i < DIR_ENTRIES_PER_SECTOR; ++i) {
            if ((entries[i].attrib & attrib_mask) && (strncmp(entries[i].name, fat_filename, FILENAME_SZ) == 0)) {
                if (location) {
                    *location = (FPathLocation) {
                        .parent_dir_cluster = current_sector.cluster,
                        .parent_dir_sector = current_sector.sector,
                        .file_entry_in_parent_dir = i * DIR_ENTRIES_PER_SECTOR,
                        .data_cluster = (((uint32_t) entries[i].cluster_high) << 8) | entries[i].cluster_low,
                    };
                }
                return F_OK;
            }
        }
        
        continuation = F_CONTINUE;
    } while (result == F_MORE_DATA);
    
    return F_PATH_NOT_FOUND;
}

static FFatResult file_find_path(FFat32* f, const char* path, uint8_t attrib_mask, FPathLocation* location)
{
    if (strlen(path) >= MAX_PATH_SZ)
        return F_FILE_PATH_TOO_LONG;
    
    // find initial cluster to look for (current directory or root)
    uint32_t dir_cluster = current_dir_cluster;
    if (path[0] == '/') {
        dir_cluster = root_dir_cluster;
        ++path;
    }
    
    // since we're losing the buffer, we copy the path
    strcpy(path_copy, path);
    char* current_path = path_copy;
    
    // iterate over directory
    while (1) {
        if (current_path[0] == '\0')
            break;
        char* end = strchr(current_path, '/');
        TRY(find_file_in_dir(f, current_path, end, dir_cluster, attrib_mask, location))
        if (!end)
            break;
        dir_cluster = location->data_cluster;
        current_path = end + 1;
    }
    
    return F_OK;
}

FFatResult file_change_current_dir(FFat32* f, const char* path)
{
    // find file location
    FPathLocation path_location;
    TRY(file_find_path(f, path, ATTR_DIR, &path_location))

    // set current dir
    current_dir_cluster = path_location.data_cluster;
    return F_OK;
}

static FFatResult path_split(char* path, char** directory, char** basename)
{
    char* last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        *directory = NULL;
        *basename = path;
    } else {
        *last_slash = '\0';
        *directory = path;
        *basename = last_slash + 1;
    }
    return F_OK;
}

static FFatResult file_create_dir_at_location(FFat32* f, const char* basename, uint32_t parent_dir_cluster, uint32_t fat_datetime)
{
    return F_OK;
}

FFatResult file_create_dir(FFat32* f, char* path, uint32_t fat_datetime)
{
    if (strlen(path) >= MAX_PATH_SZ)
        return F_FILE_PATH_TOO_LONG;
    
    // split path and filename
    char *directory, *b;
    TRY(path_split(path, &directory, &b))
    char basename[FILENAME_SZ + 2] = {0};
    strncpy(basename, b, FILENAME_SZ + 2);
    
    // find path
    uint32_t parent_dir_cluster;
    if (directory) {
        FPathLocation path_location;
        TRY(file_find_path(f, directory, ATTR_DIR, &path_location))
        parent_dir_cluster = path_location.data_cluster;
    } else {
        parent_dir_cluster = current_dir_cluster;
    }
    
    // check if file/directory already exists
    FFatResult result = find_file_in_dir(f, basename, basename + strlen(basename), parent_dir_cluster, ATTR_ANY, NULL);
    if (result == F_OK)
        return F_FILE_ALREADY_EXISTS;
    else if (result != F_PATH_NOT_FOUND)
        return result;
    
    // create dir
    TRY(file_create_dir_at_location(f, basename, parent_dir_cluster, fat_datetime))
    return F_OK;
}

#ifdef FFAT_DEBUG
#include <stdio.h>
#include <string.h>

void print_dir(FFat32* f, uint32_t cluster, const char* path)
{
    FContinuation continuation = F_START_OVER;
    FFatResult result;
    
    do {
        result = file_list_dir(f, cluster, continuation, NULL);
    
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
