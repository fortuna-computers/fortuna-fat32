#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error Sorry, only little endian platforms are supported right now.
#endif

#include "ffat32.h"

#include <stdint.h>
#include <string.h>

/*************/
/*  GLOBALS  */
/*************/

#define MAX_FILE_PATH 128
static char global_file_path[MAX_FILE_PATH];

/***********************/
/*  LOCATIONS ON DISK  */
/***********************/

#define MBR_SECTOR        0
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
#define FSI_FREE_COUNT          0x1e8
#define FSI_NEXT_FREE           0x1ec

#define BYTES_PER_SECTOR  512
#define DIR_ENTRY_SZ       32
#define FILENAME_SZ        11
#define FAT_ENTRIES_PER_SECTOR (BYTES_PER_SECTOR / sizeof(uint32_t))  /* = 128 */

#define FAT_EOF    0x0fffffff
#define FAT_EOC    0x0ffffff8
#define FAT_FREE   0x0

#define FSI_NO_VALUE      0x0fffffff

#define DIR_FILENAME      0x0
#define DIR_ATTR          0xb
#define DIR_CLUSTER_HIGH 0x14
#define DIR_CLUSTER_LOW  0x1a

#define DIR_ENTRY_FREE    0x00
#define DIR_ENTRY_UNUSED  0xe5

#define ATTR_DIR         0x10
#define ATTR_ARCHIVE     0x20


/************/
/*  MACROS  */
/************/

#define RETURN_UNLESS_F_OK(expr) { FFatResult r = (expr); if (r != F_OK) return r; }
#define TRY_IO(expr) { if (!(expr)) return F_IO_ERROR; }


/**********************/
/*  DATA CONVERSION   */
/**********************/

// region ...

static inline uint16_t from_16(uint8_t const* buffer, uint16_t pos)
{
    return *(uint16_t *) &buffer[pos];
}

static inline uint32_t from_32(uint8_t const* buffer, uint16_t pos)
{
    return *(uint32_t *) &buffer[pos];
}

static inline void to_32(uint8_t* buffer, uint16_t pos, uint32_t value)
{
    *(uint32_t *) &buffer[pos] = value;
}

// endregion

/**********************/
/*  SECTOR LOAD/SAVE  */
/**********************/

// region ...

static inline bool load_sector(FFat32* f, uint32_t sector)
{
    sector += f->reg.partition_start;
    return f->read(sector, f->buffer, f->data);
}

static inline bool load_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    return load_sector(f, (cluster - 2) * f->reg.sectors_per_cluster + f->reg.data_sector_start + sector - f->reg.partition_start);
}

static inline bool write_sector(FFat32* f, uint32_t sector)
{
    sector += f->reg.partition_start;
    return f->write(sector, f->buffer, f->data);
}

static inline bool write_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    return write_sector(f, (cluster - 2) * f->reg.sectors_per_cluster + f->reg.data_sector_start + sector - f->reg.partition_start);
}

//endregion

/***********************/
/*  FSINFO MANAGEMENT  */
/***********************/

// region ...

typedef struct {
    uint32_t next_free_cluster;
    uint32_t free_cluster_count;
} FSInfo;

static FFatResult fat_find_first_free_cluster(FFat32* f, uint32_t fat_cluster_start_at, uint32_t* first_free_cluster_number);

// Ignore FSINFO and calculate next free cluster from FAT. Updates FSINFO.
static FFatResult fsinfo_recalculate_next_free_cluster(FFat32* f, uint32_t* next_free_cluster)
{
    uint32_t first_free_cluster;
    RETURN_UNLESS_F_OK(fat_find_first_free_cluster(f, 0, &first_free_cluster))
    
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_NEXT_FREE, first_free_cluster);
    TRY_IO(write_sector(f, FSINFO_SECTOR))
    
    *next_free_cluster = first_free_cluster;
    return F_OK;
}

// Recalculate FSINFO values (next free cluster and total free clusters). Updates FSINFO.
static FFatResult fsinfo_recalculate(FFat32* f, FSInfo* fs_info)
{
    fs_info->free_cluster_count = 0;
    fs_info->next_free_cluster = 0;
    
    // free_cluster_count free clusters on FAT
    uint32_t free_cluster_count = 0;
    for (uint32_t fat_sector = 0; fat_sector < f->reg.fat_size_sectors; ++fat_sector) {   // iterate over all sectors that make up the FAT
        TRY_IO(load_sector(f, f->reg.fat_sector_start + fat_sector))
        for (uint32_t fat_entry_ptr = 0; fat_entry_ptr < BYTES_PER_SECTOR / 4; ++fat_entry_ptr) {   // iterate over all cluster pointers
            uint32_t data_cluster = from_32(f->buffer, fat_entry_ptr * 4);
            if (data_cluster == FAT_FREE) {
                if (fs_info->next_free_cluster == 0)
                    fs_info->next_free_cluster = data_cluster;
                ++fs_info->free_cluster_count;
            }
            ++free_cluster_count;
        }
    }
    
    fs_info->free_cluster_count -= (f->reg.data_sector_start * f->reg.sectors_per_cluster);
    
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_FREE_COUNT, fs_info->free_cluster_count);
    to_32(f->buffer, FSI_NEXT_FREE, fs_info->next_free_cluster);
    TRY_IO(write_sector(f, FSINFO_SECTOR))
    
    return F_OK;
}

// Read FSINFO values from disk.
static FFatResult fsinfo_get(FFat32* f, FSInfo* fs_info)
{
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    fs_info->free_cluster_count = from_32(f->buffer, FSI_FREE_COUNT);
    fs_info->next_free_cluster = from_32(f->buffer, FSI_NEXT_FREE);
    return F_OK;
}

// Update FSINFO values to disk.
static FFatResult fsinfo_update(FFat32* f, FSInfo const* fs_info)
{
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_FREE_COUNT, fs_info->free_cluster_count);
    to_32(f->buffer, FSI_NEXT_FREE, fs_info->next_free_cluster);
    TRY_IO(write_sector(f, FSINFO_SECTOR))
    return F_OK;
}

// endregion

/********************/
/*  FAT MANAGEMENT  */
/********************/

// region ...

// Get the data cluster from the FAT based on the cluster number.
static FFatResult fat_get_data_cluster(FFat32* f, uint32_t cluster_number_in_fat, uint32_t* data_cluster)
{
    uint32_t cluster_ptr = cluster_number_in_fat * 4;
    uint32_t sector_to_load = cluster_ptr / BYTES_PER_SECTOR;
    
    TRY_IO(load_sector(f, f->reg.fat_sector_start + sector_to_load))
    
    *data_cluster = from_32(f->buffer, cluster_ptr % BYTES_PER_SECTOR);
    
    return F_OK;
}

// Update the data cluster pointer in the FAT.
static FFatResult fat_update_data_cluster(FFat32* f, uint32_t cluster_number_in_fat, uint32_t ptr)
{
    uint32_t cluster_ptr = cluster_number_in_fat * 4;
    uint32_t sector_to_update = cluster_ptr / BYTES_PER_SECTOR;
    
    // read FAT and replace cluster_number_in_fat
    TRY_IO(load_sector(f, f->reg.fat_sector_start + sector_to_update))
    to_32(f->buffer, cluster_ptr % BYTES_PER_SECTOR, ptr);
    
    // write to all FAT copies
    uint32_t fat_sector = f->reg.fat_sector_start + sector_to_update;
    for (uint8_t i = 0; i < f->reg.number_of_fats; ++i) {
        TRY_IO(write_sector(f, fat_sector))
        fat_sector += f->reg.fat_size_sectors;
    }
    
    return F_OK;
}

// Find the first free cluster on FAT.
static FFatResult fat_find_first_free_cluster(FFat32* f, uint32_t fat_cluster_start_at, uint32_t* first_free_cluster_number)
{
    uint32_t starting_sector = fat_cluster_start_at / FAT_ENTRIES_PER_SECTOR;
    
    uint32_t current_cluster_number = starting_sector * FAT_ENTRIES_PER_SECTOR;
    
    for (uint32_t sector = starting_sector; sector < f->reg.fat_size_sectors; ++sector) {
        TRY_IO(load_sector(f, f->reg.fat_sector_start + sector))
        for (uint32_t fat_entry_ptr = 0; fat_entry_ptr < FAT_ENTRIES_PER_SECTOR; ++fat_entry_ptr) {
            uint32_t pointer = from_32(f->buffer, fat_entry_ptr * sizeof(uint32_t));
            if (pointer == FAT_FREE) {
                *first_free_cluster_number = current_cluster_number;
                return F_OK;
            }
            ++current_cluster_number;
        }
    }
    
    // not found
    return F_DEVICE_FULL;
}

// Create a new data cluster in the FAT.
static FFatResult fat_append_cluster(FFat32* f, uint32_t continue_from_cluster, uint32_t* next_free_cluster)
{
    // get next cluster from FSINFO
    FSInfo fs_info;
    RETURN_UNLESS_F_OK(fsinfo_get(f, &fs_info))
    
    // find next free cluster (cluster F)
    RETURN_UNLESS_F_OK(fat_find_first_free_cluster(f, fs_info.next_free_cluster, next_free_cluster))
    
    // point the previous cluster to cluster F
    RETURN_UNLESS_F_OK(fat_update_data_cluster(f, continue_from_cluster, *next_free_cluster))
    
    // set the cluster F as EOC
    RETURN_UNLESS_F_OK(fat_update_data_cluster(f, *next_free_cluster, FAT_EOC))
    
    // update FSINFO
    fs_info.next_free_cluster = *next_free_cluster;
    RETURN_UNLESS_F_OK(fsinfo_update(f, &fs_info))
    
    return F_OK;
}

// Remove a file from FAT (follows linked list deleting one by one)
static FFatResult fat_remove_file(FFat32* f, uint32_t cluster_number, uint32_t* cluster_count)
{
    int64_t last_fat_sector_loaded = -1;
    uint32_t next_cluster_to_delete = cluster_number;
    *cluster_count = 0;
    
    do {
        // find which sector to load
        uint32_t cluster_ptr = next_cluster_to_delete * 4;
        uint32_t sector_to_load = cluster_ptr / BYTES_PER_SECTOR;
        if (sector_to_load != last_fat_sector_loaded) {
            if (last_fat_sector_loaded != -1) // save previous iteration
                TRY_IO(write_sector(f, f->reg.fat_sector_start + last_fat_sector_loaded))
            TRY_IO(load_sector(f, f->reg.fat_sector_start + sector_to_load))
            last_fat_sector_loaded = sector_to_load;
        }
        
        // find next cluster
        next_cluster_to_delete = from_32(f->buffer, cluster_ptr % BYTES_PER_SECTOR);
    
        // clear cluster in FAT
        to_32(f->buffer, cluster_ptr % BYTES_PER_SECTOR, FAT_FREE);
        ++(*cluster_count);
        
    } while (next_cluster_to_delete != FAT_EOC && next_cluster_to_delete != FAT_EOF);
    
    // save last iteration
    if (last_fat_sector_loaded != -1)
        TRY_IO(write_sector(f, f->reg.fat_sector_start + last_fat_sector_loaded))
    
    return F_OK;
}

// endregion

/***************/
/*  FIND FILE  */
/***************/

// region ...

typedef struct FDirResult {
    uint32_t   next_cluster;
    uint16_t   next_sector;
} FDirResult;

// Load directory entries sector into the buffer. If it returns F_MORE_DATA, it can be called again with continuation == F_CONTINUE
// and the last returned `dir_result` to load the whole entry list until it returns F_OK.
static FFatResult dir(FFat32* f, uint32_t dir_cluster, FContinuation continuation, uint32_t continue_on_cluster, uint16_t continue_on_sector, FDirResult* dir_result)
{
    uint32_t cluster;
    uint16_t sector;
    
    dir_result->next_cluster = dir_result->next_sector = 0;
    
    // find current cluster and sector
    if (continuation == F_START_OVER) {   // user has requested to start reading a new directory
        cluster = dir_cluster;
        sector = 0;
    } else {   // user is continuing to read a directory that was started in a previous call
        cluster = continue_on_cluster;
        sector = continue_on_sector;
    }
    
    // move to next cluster and/or sector
    uint32_t next_cluster, next_sector;
    if (sector >= (f->reg.sectors_per_cluster - 1U)) {
        RETURN_UNLESS_F_OK(fat_get_data_cluster(f, cluster, &next_cluster))
        next_sector = 0;
    } else {
        next_cluster = cluster;
        next_sector = sector + 1;
    }
    
    // check if more data is needed
    FFatResult result;
    if (next_cluster == FAT_EOC || next_cluster == FAT_EOF) {
        result = F_OK;
    } else {
        dir_result->next_cluster = next_cluster;
        dir_result->next_sector = next_sector;
        result = F_MORE_DATA;
    }
    
    // load directory data form sector into buffer
    TRY_IO(load_data_cluster(f, cluster, sector))
    
    // check if we *really* have more data to read (the last dir in array is not null)
    if (result == F_MORE_DATA && f->buffer[BYTES_PER_SECTOR - DIR_ENTRY_SZ] == '\0') {
        return F_OK;
    }
    
    return result;
}

// Convert filename to FAT format.
static void parse_filename(char result[11], char const* filename, size_t filename_sz)
{
    memset(result, ' ', FILENAME_SZ);
    
    if (strcmp(result, ".") == 0) {
        result[0] = '.';
        return;
    } else if (strcmp(result, "..") == 0) {
        strcpy(result, "..");
        return;
    }
    
    uint8_t pos = 0, i = 0;
    while (pos < FILENAME_SZ) {
        if (filename[i] == 0 || i == filename_sz) {
            return;
        } else if (filename[i] == '.') {
            pos = FILENAME_SZ - 3;
            ++i;
        } else {
            result[pos++] = filename[i++];
        }
    }
}

typedef struct FPathLocation {
    uint32_t data_cluster;
    uint32_t parent_dir_cluster;
    uint16_t parent_dir_sector;
    uint16_t file_entry_in_parent_dir;
} FPathLocation;

// Search a directory entry sector for a file, and return its data cluster if found.
static FFatResult find_file_cluster_in_dir_entries_sector(FFat32* f, const char* filename, FPathLocation* path_location)
{
    for (uint16_t entry_number = 0; entry_number < (BYTES_PER_SECTOR / DIR_ENTRY_SZ); ++entry_number) {   // iterate through each entry
        uint16_t entry_ptr = entry_number * DIR_ENTRY_SZ;
        
        if (f->buffer[entry_ptr + DIR_FILENAME] == 0)  // no more files
            break;
        
        uint8_t attr = f->buffer[entry_ptr + DIR_ATTR];   // attribute should be 0x10 (directory)
        
        // if file/directory is found
        if (((attr & ATTR_DIR) || (attr & ATTR_ARCHIVE))
            && strncmp(filename, (const char *) &f->buffer[entry_ptr + DIR_FILENAME], FILENAME_SZ) == 0) {
            
            // return file/directory data_cluster
            path_location->file_entry_in_parent_dir = entry_ptr;
            path_location->data_cluster = from_16(f->buffer, entry_ptr + DIR_CLUSTER_LOW) | ((uint32_t) from_16(f->buffer, entry_ptr + DIR_CLUSTER_HIGH) << 8);
            return F_OK;
        }
    }
    
    return F_PATH_NOT_FOUND;
}

// Load cluster containing dir entries from a specific directory cluster and try to find the entry with the specific filename.
static FFatResult find_file_cluster_in_dir_entries_cluster(FFat32* f, const char* filename, size_t filename_sz, uint32_t dir_entries_cluster,
                                                           FPathLocation* path_location)
{
    // convert filename to FAT format
    char parsed_filename[FILENAME_SZ];
    parse_filename(parsed_filename, filename, filename_sz);
    
    // load current directory
    FFatResult result;
    FDirResult dir_result = { dir_entries_cluster, 0 };
    FContinuation continuation = F_START_OVER;
    
    do {   // each iteration looks to one sector in the cluster
    
        path_location->parent_dir_cluster = dir_result.next_cluster;
        path_location->parent_dir_sector = dir_result.next_sector;
    
        // read directory
        result = dir(f, dir_entries_cluster, continuation, dir_result.next_cluster, dir_result.next_sector, &dir_result);
        if (result != F_OK && result != F_MORE_DATA)
            return result;
        
        // iterate through files in directory sector
        if (find_file_cluster_in_dir_entries_sector(f, parsed_filename, path_location) == F_OK)
            return F_OK;
        
        continuation = F_CONTINUE;  // in next fetch, continue the previous one
        
    } while (result == F_MORE_DATA);   // if that was the last sector in the directory cluster containing files, exit loop
    
    // the directory was not found
    return F_PATH_NOT_FOUND;
}

// Crawl directories until it finds the data index cluster_number for a given path.
static FFatResult find_path_location(FFat32* f, const char* path, FPathLocation* path_location)
{
    // copy file path to a global variable, so we can change it
    size_t len = strlen(path);
    if (len >= MAX_FILE_PATH)
        return F_FILE_PATH_TOO_LONG;
    strcpy(global_file_path, path);
    char* file = global_file_path;
    
    // find starting cluster_number
    uint32_t current_cluster;
    if (file[0] == '/') {   // absolute path
        current_cluster = f->reg.root_dir_cluster;
        ++file;  // skip intitial '/'
    } else {
        current_cluster = f->reg.current_dir_cluster;
    }
    
    while (1) {   // each iteration will crawl one directory
        
        len = strlen(file);   // is this the last directory?
        if (len == 0) {
            path_location->data_cluster = current_cluster;
            return F_OK;
        }
        
        char* end = strchr(file, '/');
        
        if (end != NULL) {   // this is a directory: find dir cluster_number and continue crawling
            RETURN_UNLESS_F_OK(find_file_cluster_in_dir_entries_cluster(f, file, end - file, current_cluster, path_location))
            current_cluster = path_location->data_cluster;
            file = end + 1;  // skip to next
            
        } else {  // this is the final file/dir in path: find cluster_number and return it
            RETURN_UNLESS_F_OK(find_file_cluster_in_dir_entries_cluster(f, file, len, current_cluster, path_location))
            return F_OK;
        }
    }
}

// endregion

/*****************/
/*  CREATE FILE  */
/*****************/

// region ...

typedef struct __attribute__((__packed__)) {
    char     name[11];
    uint8_t  attrib;
    uint8_t  nt_res;
    uint8_t  time_tenth;
    uint32_t crt_datetime;
    uint16_t last_acc_time;
    uint16_t cluster_high;
    uint32_t wrt_datetime;
    uint16_t cluster_low;
    uint32_t file_size;
} FDirEntry;

typedef struct {
    uint32_t cluster;
    uint16_t sector;
    uint16_t entry_ptr;
} FileEntry;

static void split_path_and_filename(char* file_path, char filename[FILENAME_SZ])
{
    size_t len = strlen(file_path);
    if (file_path[len - 1] == '/')   // remove trailing slash
        file_path[len - 1] = '\0';
    
    char* slash = strrchr(file_path, '/');
    if (slash == NULL) {  // no slash in filename
        parse_filename(filename, file_path, len);
        file_path[0] = '\0';
    } else {
        parse_filename(filename, slash + 1, slash + 1 - filename + len);
        *slash = '\0';
    }
}

static bool validate_filename(const char filename[FILENAME_SZ])
{
    static const char* invalid_chars = "\\/:*?\"<>|";
    
    for (uint8_t i = 0; i < FILENAME_SZ; ++i) {
        if (filename[i] < 32)
            return false;
        const char* ichr = invalid_chars;
        do {
            if (filename[i] == *ichr)
                return false;
        } while (*(++ichr));
    }
    return true;
}

static FFatResult find_next_free_cluster(FFat32* f, uint32_t* next_free_cluster)
{
    // load next free cluster from FSINFO
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    uint32_t hint_next_free_cluster = from_32(f->buffer, FSI_NEXT_FREE);
    
    // check if value is valid
    bool recalculate = false;
    if (hint_next_free_cluster == FSI_NO_VALUE) {
        recalculate = true;
    } else {
        FFatResult result = fat_find_first_free_cluster(f, hint_next_free_cluster, next_free_cluster);
        if (result != F_OK)
            recalculate = true;
    }
    
    // recalculate next free cluster in FSINFO, if not valid
    if (recalculate)
        RETURN_UNLESS_F_OK(fsinfo_recalculate_next_free_cluster(f, next_free_cluster))
        
    return F_OK;
}

static FFatResult find_next_free_directory_entry(FFat32* f, uint32_t path_cluster, FileEntry* file_entry)
{
    *file_entry = (FileEntry) {
            .cluster   = path_cluster,
            .sector    = 0,
            .entry_ptr = 0,
    };
    
    // check all dir entries in the cluster
search_cluster:
    for (file_entry->sector = 0; file_entry->sector < f->reg.sectors_per_cluster; ++file_entry->sector) {
        TRY_IO(load_data_cluster(f, file_entry->cluster, file_entry->sector))
        for (file_entry->entry_ptr = 0; file_entry->entry_ptr < BYTES_PER_SECTOR; file_entry->entry_ptr += DIR_ENTRY_SZ) {
            uint8_t first_chr = f->buffer[file_entry->entry_ptr];
            if (first_chr == DIR_ENTRY_FREE || first_chr == DIR_ENTRY_UNUSED)
                return F_OK;
        }
    }
    
    // if not found, go to next cluster until EOC
    uint32_t next_cluster;
    RETURN_UNLESS_F_OK(fat_get_data_cluster(f, file_entry->cluster, &next_cluster))
    if (next_cluster != FAT_EOC && next_cluster != FAT_EOF) {
        file_entry->cluster = next_cluster;
        goto search_cluster;
    }
    
    // not found
    return F_PATH_NOT_FOUND;
}

static FFatResult create_entry_in_directory(FFat32* f, uint32_t parent_dir_data_cluster, char filename[FILENAME_SZ],
                                            uint8_t attrib, uint16_t fat_datetime, uint32_t data_cluster)
{
    // find next free directory entry
    FileEntry file_entry;
    FFatResult result = find_next_free_directory_entry(f, parent_dir_data_cluster, &file_entry);
    
    // if no directory free entry, append cluster
    if (result == F_PATH_NOT_FOUND) {
        RETURN_UNLESS_F_OK(fat_append_cluster(f, parent_dir_data_cluster, &parent_dir_data_cluster))
        file_entry = (FileEntry) {
            .cluster = parent_dir_data_cluster,
            .sector = 0,
            .entry_ptr = 0,
        };
    } else if (result != F_OK) {
        return result;
    }
    
    // create entry
    FDirEntry dir_entry = {
            .name = { 0 },
            .attrib = attrib,
            .nt_res = 0,
            .time_tenth = 0,
            .crt_datetime = fat_datetime,
            .last_acc_time = fat_datetime & 0xff,
            .cluster_high = (data_cluster >> 8),
            .wrt_datetime = fat_datetime,
            .cluster_low = data_cluster & 0xff,
            .file_size = 0
    };
    memcpy(dir_entry.name, filename, FILENAME_SZ);
    memcpy(&f->buffer[file_entry.entry_ptr], &dir_entry, sizeof(FDirEntry));
    
    TRY_IO(write_data_cluster(f, file_entry.cluster, file_entry.sector))
    
    return F_OK;
}

static FFatResult update_fsinfo(FFat32* f, uint32_t last_cluster, int64_t change_in_size)
{
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_NEXT_FREE, last_cluster);
    uint32_t current_count = from_32(f->buffer, FSI_FREE_COUNT);
    to_32(f->buffer, FSI_FREE_COUNT, (int64_t) current_count + change_in_size);
    TRY_IO(write_sector(f, FSINFO_SECTOR))
    return F_OK;
}

static FFatResult create_file_entry(FFat32* f, char* file_path, uint8_t attrib, uint32_t fat_datetime, uint32_t* data_cluster, uint32_t* parent_dir)
{
    // parse filename and find parent directory cluster
    char filename[FILENAME_SZ];
    split_path_and_filename(file_path, filename);
    FPathLocation path_location;
    RETURN_UNLESS_F_OK(find_path_location(f, file_path, &path_location))
    *parent_dir = path_location.data_cluster;
    
    if (!validate_filename(filename))
        return F_INVALID_FILENAME;
    
    // create a new cluster
    RETURN_UNLESS_F_OK(find_next_free_cluster(f, data_cluster))
    RETURN_UNLESS_F_OK(fat_update_data_cluster(f, *data_cluster, FAT_EOF))
    
    // create directory entry in parent directory
    RETURN_UNLESS_F_OK(create_entry_in_directory(f, path_location.data_cluster, filename, attrib, fat_datetime, *data_cluster))
    
    // update FSINFO
    RETURN_UNLESS_F_OK(update_fsinfo(f, *data_cluster, +1))
    
    return F_OK;
}

// endregion

/*****************/
/*  REMOVE FILE  */
/*****************/

// region ...

static FFatResult mark_file_entry_as_removed(FFat32* f, FPathLocation const* path_location)
{
    TRY_IO(load_data_cluster(f, path_location->parent_dir_cluster, path_location->parent_dir_sector))
    f->buffer[path_location->file_entry_in_parent_dir] = DIR_ENTRY_UNUSED;
    TRY_IO(write_data_cluster(f, path_location->parent_dir_cluster, path_location->parent_dir_sector))
    return F_OK;
}

static FFatResult remove_file(FFat32* f, FPathLocation const* path_location)
{
    // go through linked list in FAT, removing all entries
    // (at the same time, count the number of clusters)
    uint32_t cluster_count;
    RETURN_UNLESS_F_OK(fat_remove_file(f, path_location->data_cluster, &cluster_count))
    
    // update directory entry in parent
    RETURN_UNLESS_F_OK(mark_file_entry_as_removed(f, path_location))
    
    // update FSINFO
    FSInfo fs_info;
    RETURN_UNLESS_F_OK(fsinfo_get(f, &fs_info))
    fs_info.next_free_cluster = path_location->data_cluster;
    fs_info.free_cluster_count -= cluster_count;
    RETURN_UNLESS_F_OK(fsinfo_update(f, &fs_info))
    
    return F_OK;
}

// endregion

/********************/
/*  INITIALIZATION  */
/********************/

//region ...

static FFatResult f_init(FFat32* f)
{
    // check partition location
    if (!f->read(MBR_SECTOR, f->buffer, f->data))
        return F_IO_ERROR;
    if (f->buffer[0] == 0xeb) {  // this is a FAT partition
        f->reg.partition_start = 0;
    } else {
        f->reg.partition_start = from_32(f->buffer, PARTITION_TABLE_1);
        TRY_IO(load_sector(f, BOOT_SECTOR))
    }
    
    // fill out fields
    uint32_t reserved_sectors = from_16(f->buffer, BPB_RESERVED_SECTORS);
    f->reg.fat_sector_start = reserved_sectors;
    f->reg.fat_size_sectors = from_32(f->buffer, BPB_FAT_SIZE_SECTORS);
    
    f->reg.sectors_per_cluster = f->buffer[BPB_SECTORS_PER_CLUSTER];
    if (f->reg.sectors_per_cluster == 0)
        return F_NOT_FAT_32;
    
    f->reg.number_of_fats = f->buffer[BPB_NUMBER_OF_FATS];
    uint32_t root_dir_sector = reserved_sectors + (f->reg.number_of_fats * f->reg.fat_size_sectors);
    f->reg.data_sector_start = root_dir_sector + f->reg.partition_start;
    
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

static FFatResult f_fsinfo_recalc(FFat32* f)
{
    FSInfo fs_info;
    RETURN_UNLESS_F_OK(fsinfo_recalculate(f, &fs_info))
    return F_OK;
}

static FFatResult f_free(FFat32* f)
{
    TRY_IO(load_sector(f, FSINFO_SECTOR))
    uint32_t free_ = from_32(f->buffer, FSI_FREE_COUNT);
    if (free_ == 0xffffffff)
        f_fsinfo_recalc(f);
    else
        to_32(f->buffer, 0, free_);
    
    return F_OK;
}

static FFatResult f_boot(FFat32* f)
{
    TRY_IO(load_sector(f, BOOT_SECTOR))
    return F_OK;
}

//endregion

/**************************/
/*  DIRECTORY OPERATIONS  */
/**************************/

// region ...

static FFatResult is_directory_empty(FFat32* f, FPathLocation const* path_location, bool* is_empty)
{
    uint32_t count = 0;
    FFatResult result;
    FDirResult dir_result = { 0, 0 };
    FContinuation continuation = F_START_OVER;
    
    do {   // each iteration looks to one sector in the cluster
        
        // read directory
        result = dir(f, path_location->data_cluster, continuation, dir_result.next_cluster, dir_result.next_sector, &dir_result);
        if (result != F_OK && result != F_MORE_DATA)
            return result;
        
        // count entries in directory
        for (uint16_t entry_number = 0; entry_number < (BYTES_PER_SECTOR / DIR_ENTRY_SZ); ++entry_number) {   // iterate through each entry
            uint16_t entry_ptr = entry_number * DIR_ENTRY_SZ;
        
            uint8_t file_indicator = f->buffer[entry_ptr];
            if (file_indicator == DIR_ENTRY_FREE)
                break;
            if (file_indicator == DIR_ENTRY_UNUSED)
                continue;
            
            ++count;
        }
        
        continuation = F_CONTINUE;  // in next fetch, continue the previous one
        
    } while (result == F_MORE_DATA);   // if that was the last sector in the directory cluster containing files, exit loop
    
    return count == 2 ? F_OK : F_DIR_NOT_EMPTY;
}

static FFatResult f_dir(FFat32* f)
{
    FDirResult dir_result;
    FFatResult result = dir(f, f->reg.current_dir_cluster, f->buffer[0],
                            f->reg.state_next_cluster, f->reg.state_next_sector, &dir_result);
    f->reg.state_next_cluster = dir_result.next_cluster;
    f->reg.state_next_sector = dir_result.next_sector;
    return result;
}

static FFatResult f_cd(FFat32* f)
{
    FPathLocation path_location;
    RETURN_UNLESS_F_OK(find_path_location(f, (const char*) f->buffer, &path_location))
    f->reg.current_dir_cluster = path_location.data_cluster;
    return F_OK;
}

static FFatResult f_mkdir(FFat32* f, uint32_t fat_datetime)
{
    uint32_t parent_dir_cluster = 0;
    
    // create file entry
    uint32_t cluster_self;
    RETURN_UNLESS_F_OK(create_file_entry(f, (char *) f->buffer, ATTR_DIR, fat_datetime, &cluster_self, &parent_dir_cluster))
    
    // create empty directory structure ('.' and '..')
    char filename[FILENAME_SZ]; memset(filename, ' ', FILENAME_SZ);
    filename[0] = '.';
    RETURN_UNLESS_F_OK(create_entry_in_directory(f, cluster_self, filename, ATTR_DIR, fat_datetime, cluster_self))
    filename[1] = '.';
    RETURN_UNLESS_F_OK(create_entry_in_directory(f, cluster_self, filename, ATTR_DIR, fat_datetime, parent_dir_cluster))
    
    return F_OK;
}

static FFatResult f_rmdir(FFat32* f)
{
    // find directory
    FPathLocation path_location;
    RETURN_UNLESS_F_OK(find_path_location(f, (const char *) f->buffer, &path_location))
    
    // check if "file" is directory
    uint8_t attr = f->buffer[path_location.file_entry_in_parent_dir + DIR_ATTR];    // buffer already contains the dir entry
    if (!(attr & ATTR_DIR))
        return F_NOT_A_DIRECTORY;
    
    // check if directory is empty
    bool is_empty;
    RETURN_UNLESS_F_OK(is_directory_empty(f, &path_location, &is_empty))
    
    // remove directory "file"
    RETURN_UNLESS_F_OK(remove_file(f, &path_location))
    
    return F_OK;
}

// endregion

/************************/
/* FILE/DIR OPERATIONS  */
/************************/

// region ...

static FFatResult f_stat(FFat32* f)
{
    FPathLocation path_location;
    RETURN_UNLESS_F_OK(find_path_location(f, (const char*) f->buffer, &path_location))
    
    // set first 32 bits to file stat
    uint16_t addr = path_location.file_entry_in_parent_dir;
    if (addr != 0)
        memcpy(f->buffer, &f->buffer[addr], DIR_ENTRY_SZ);
    
    // the rest of the bits are zeroed
    memset(&f->buffer[DIR_ENTRY_SZ], 0, BYTES_PER_SECTOR - DIR_ENTRY_SZ);
    
    return F_OK;
}

// endregion

/*****************/
/*  MAIN METHOD  */
/*****************/

FFatResult f_fat32(FFat32* f, FFat32Op operation, uint32_t fat_datetime)
{
    switch (operation) {
        case F_INIT:          f->reg.last_operation_result = f_init(f);   break;
        case F_FREE:          f->reg.last_operation_result = f_free(f);   break;
        case F_FSINFO_RECALC: f->reg.last_operation_result = f_fsinfo_recalc(f); break;
        case F_BOOT:          f->reg.last_operation_result = f_boot(f);   break;
        case F_DIR:           f->reg.last_operation_result = f_dir(f);    break;
        case F_CD:            f->reg.last_operation_result = f_cd(f);     break;
        case F_MKDIR:         f->reg.last_operation_result = f_mkdir(f, fat_datetime); break;
        case F_RMDIR:         f->reg.last_operation_result = f_rmdir(f);  break;
        case F_OPEN:          break;
        case F_CLOSE:         break;
        case F_READ:          break;
        case F_WRITE:         break;
        case F_STAT:          f->reg.last_operation_result = f_stat(f);   break;
        case F_RM:            break;
        case F_MV:            break;
        default:              f->reg.last_operation_result = F_INCORRECT_OPERATION;
    }
    return f->reg.last_operation_result;
}

// http://elm-chan.org/docs/fat_e.html
