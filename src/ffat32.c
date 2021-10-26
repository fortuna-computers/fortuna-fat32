#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error Sorry, only little endian platforms are supported right now.
#endif

#include "ffat32.h"

#include <stdint.h>
#include <string.h>

/*************/
/*  GLOBALS  */
/*************/

#define MAX_FILE_PATH 256
static char global_file_path[MAX_FILE_PATH];

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
#define FSI_FREE_COUNT          0x1e8
#define FSI_NEXT_FREE           0x1ec

#define BYTES_PER_SECTOR  512
#define DIR_ENTRY_SZ       32
#define FILENAME_SZ        11

#define FAT_EOF    0x0fffffff
#define FAT_EOC    0x0ffffff8
#define FAT_FREE   0x0

#define FSI_NO_VALUE      0x0fffffff

#define DIR_FILENAME      0x0
#define DIR_ATTR          0xb
#define DIR_CLUSTER_HIGH 0x14
#define DIR_CLUSTER_LOW  0x1a

#define DIR_ENTRY_FREE1  0x00
#define DIR_ENTRY_FREE2  0xe5

#define ATTR_DIR     0x10
#define ATTR_ARCHIVE 0x20

#define RETURN_UNLESS_F_OK(expr) { if ((expr) != F_OK) return expr; }
#define IO(expr) { if (!(expr)) return F_IO_ERROR; }


/**********************/
/*  DATA CONVERSION   */
/**********************/

// region ...

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

// endregion

/**********************/
/*  SECTOR LOAD/SAVE  */
/**********************/

// region ...

static bool load_sector(FFat32* f, uint32_t sector)
{
    sector += f->reg.partition_start;
    return f->read(sector, f->buffer, f->data);
}

static bool load_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    return load_sector(f, (cluster - 2) * f->reg.sectors_per_cluster + f->reg.data_start_sector + sector - f->reg.partition_start);
}

static bool write_sector(FFat32* f, uint32_t sector)
{
    sector += f->reg.partition_start;
    return f->write(sector, f->buffer, f->data);
}

static bool write_data_cluster(FFat32* f, uint32_t cluster, uint16_t sector)
{
    return write_sector(f, (cluster - 2) * f->reg.sectors_per_cluster + f->reg.data_start_sector + sector - f->reg.partition_start);
}

//endregion

/***********************/
/*  FSINFO MANAGEMENT  */
/***********************/

// region ...

typedef struct {
    uint32_t last_used_cluster;
    uint32_t free_cluster_count;
} FSInfo;

static FFatResult fat_find_first_free_cluster(FFat32* f, uint32_t start_at, uint32_t* first_free_cluster);

// Ignore FSINFO and calculate next free cluster from FAT. Updates FSINFO.
static FFatResult fsinfo_recalculate_next_free_cluster(FFat32* f, uint32_t* next_free_cluster)
{
    uint32_t first_free_cluster;
    RETURN_UNLESS_F_OK(fat_find_first_free_cluster(f, 0, &first_free_cluster))
    
    IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_NEXT_FREE, first_free_cluster);
    IO(write_sector(f, FSINFO_SECTOR))
    
    *next_free_cluster = first_free_cluster;
    return F_OK;
}

// Recalculate FSINFO values (next free cluster and total free clusters). Updates FSINFO.
static FFatResult fsinfo_recalculate_values(FFat32* f, FSInfo* fs_info)
{
    fs_info->free_cluster_count = 0;
    fs_info->last_used_cluster = 0;
    
    // count free clusters on FAT
    uint32_t pos = f->reg.fat_sector_start;
    uint32_t count = 0;
    for (uint64_t i = 0; i < f->reg.fat_size_sectors; ++i) {
        IO(load_sector(f, pos + i))
        for (uint32_t j = 0; j < BYTES_PER_SECTOR / 4; ++j) {
            uint32_t pointer = from_32(f->buffer, j * 4);
            if (pointer == FAT_FREE) {
                if (fs_info->last_used_cluster == 0)
                    fs_info->last_used_cluster = count;
                ++fs_info->free_cluster_count;
            }
            ++count;
        }
    }
    
    fs_info->free_cluster_count -= (f->reg.data_start_sector * f->reg.sectors_per_cluster);
    
    IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_FREE_COUNT, fs_info->free_cluster_count);
    to_32(f->buffer, FSI_NEXT_FREE, fs_info->last_used_cluster);
    IO(write_sector(f, FSINFO_SECTOR))
    
    return F_OK;
}

// Read FSINFO values from disk.
static FFatResult fsinfo_values(FFat32* f, FSInfo* fs_info)
{
    IO(load_sector(f, FSINFO_SECTOR))
    fs_info->free_cluster_count = from_32(f->buffer, FSI_FREE_COUNT);
    fs_info->last_used_cluster = from_32(f->buffer, FSI_NEXT_FREE);
    return F_OK;
}

// Update FSINFO values to disk,
static FFatResult fsinfo_update(FFat32* f, FSInfo const* fs_info)
{
    IO(load_sector(f, FSINFO_SECTOR))
    to_32(f->buffer, FSI_FREE_COUNT, fs_info->free_cluster_count);
    to_32(f->buffer, FSI_NEXT_FREE, fs_info->last_used_cluster);
    IO(write_sector(f, FSINFO_SECTOR))
    return F_OK;
}

// endregion

/********************/
/*  FAT MANAGEMENT  */
/********************/

// region ...

// Get the data cluster pointer from the FAT.
static int64_t fat_data_cluster_ptr(FFat32* f, uint32_t cluster)
{
    uint32_t cluster_loc = cluster * 4;
    uint32_t sector_to_load = cluster_loc / BYTES_PER_SECTOR;
    
    load_sector(f, f->reg.fat_sector_start + sector_to_load);
    
    return from_32(f->buffer, cluster_loc % BYTES_PER_SECTOR);
}

// Update the data cluster pointer in the FAT.
static FFatResult fat_update_data_cluster_ptr(FFat32* f, uint32_t cluster, uint32_t ptr)
{
    uint32_t cluster_loc = cluster * 4;
    uint32_t sector_to_load = cluster_loc / BYTES_PER_SECTOR;
    
    // read FAT and replace cluster
    IO(load_sector(f, f->reg.fat_sector_start + sector_to_load))
    to_32(f->buffer, cluster_loc % BYTES_PER_SECTOR, ptr);
    
    // write FAT and FAT copies
    uint32_t fat_sector = f->reg.fat_sector_start + sector_to_load;
    for (uint8_t i = 0; i < f->reg.number_of_fats; ++i) {
        write_sector(f, fat_sector);
        fat_sector += f->reg.fat_size_sectors;
    }
    
    return F_OK;
}

// Find the first free cluster on FAT.
static FFatResult fat_find_first_free_cluster(FFat32* f, uint32_t start_at, uint32_t* first_free_cluster)
{
    uint32_t starting_sector = start_at / 128;
    
    // count free sectors on FAT
    uint32_t pos = f->reg.fat_sector_start;
    uint32_t count = starting_sector * 128;
    for (uint32_t i = starting_sector; i < f->reg.fat_size_sectors; ++i) {
        IO(load_sector(f, pos + i))
        for (uint32_t j = 0; j < BYTES_PER_SECTOR / 4; ++j) {
            uint32_t pointer = from_32(f->buffer, j * 4);
            if (pointer == FAT_FREE) {
                *first_free_cluster = count;
                return F_OK;
            }
            ++count;
        }
    }
    
    // not found
    return F_DEVICE_FULL;
}

static FFatResult fat_append_cluster(FFat32* f, uint32_t continue_from_cluster, uint32_t* next_free_cluster)
{
    // get next cluster from FSINFO
    FSInfo fs_info;
    RETURN_UNLESS_F_OK(fsinfo_values(f, &fs_info))
    
    // find next free cluster (cluster F)
    RETURN_UNLESS_F_OK(fat_find_first_free_cluster(f, fs_info.last_used_cluster, next_free_cluster))
    
    // point the previous cluster to cluster F
    fat_update_data_cluster_ptr(f, continue_from_cluster, *next_free_cluster);
    
    // set the cluster F as EOC
    fat_update_data_cluster_ptr(f, *next_free_cluster, FAT_EOC);
    
    // update FSINFO
    fs_info.last_used_cluster = *next_free_cluster;
    RETURN_UNLESS_F_OK(fsinfo_update(f, &fs_info))
    
    return F_OK;
}

// endregion

/***************/
/*  FIND FILE  */
/***************/

// region ...

typedef struct {
    uint32_t   next_cluster;
    uint16_t   next_sector;
} FDirResult;

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
        next_cluster = fat_data_cluster_ptr(f, cluster);
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
    IO(load_data_cluster(f, cluster, sector))
    
    // check if we *really* have more data to read (the last dir in array is not null)
    if (result == F_MORE_DATA && f->buffer[BYTES_PER_SECTOR - DIR_ENTRY_SZ] == '\0') {
        return F_OK;
    }
    
    return result;
}

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

// Search a directory entry sector for a file, and return its data data_cluster if found.
static bool find_file_cluster_in_dir_entry(FFat32* f, const char* filename, uint32_t* data_cluster, uint16_t* file_entry_ptr)
{
    for (uint16_t i = 0; i < (BYTES_PER_SECTOR / DIR_ENTRY_SZ); ++i) {   // iterate through each entry
        uint16_t addr = i * DIR_ENTRY_SZ;
        
        if (f->buffer[addr + DIR_FILENAME] == 0)  // no more files
            break;
        
        uint8_t attr = f->buffer[addr + DIR_ATTR];   // attribute should be 0x10 (directory)
        
        // if file/directory is found
        if (((attr & ATTR_DIR) || (attr & ATTR_ARCHIVE))
            && strncmp(filename, (const char *) &f->buffer[addr + DIR_FILENAME], FILENAME_SZ) == 0) {
            
            // return file/directory data_cluster
            if (file_entry_ptr)
                *file_entry_ptr = addr;
            *data_cluster = *(uint16_t *) &f->buffer[addr + DIR_CLUSTER_LOW]
                            | ((uint32_t) (*(uint16_t *) &f->buffer[addr + DIR_CLUSTER_HIGH]) << 8);
            return true;
        }
    }
    
    return false;
}

// Load cluster containing dir entries from a specific directory cluster and try to find the entry with the specific filename.
static int64_t find_file_cluster_in_dir_cluster(FFat32* f, const char* filename, size_t filename_sz, uint32_t dir_entries_cluster, uint16_t* file_entry_ptr)
{
    // convert filename to FAT format
    char parsed_filename[FILENAME_SZ];
    parse_filename(parsed_filename, filename, filename_sz);
    
    // load current directory
    FFatResult result;
    FDirResult dir_result = { 0, 0 };
    FContinuation continuation = F_START_OVER;
    
    do {   // each iteration looks to one sector in the cluster
        
        // read directory
        result = dir(f, dir_entries_cluster, continuation, dir_result.next_cluster, dir_result.next_sector, &dir_result);
        if (result != F_OK && result != F_MORE_DATA)
            return -result;
        
        // iterate through files in directory sector
        uint32_t cluster;
        if (find_file_cluster_in_dir_entry(f, parsed_filename, &cluster, file_entry_ptr))
            return cluster;
        
        continuation = F_CONTINUE;  // in next fetch, continue the previous one
        
    } while (result == F_MORE_DATA);   // if that was the last sector in the directory cluster containing files, exit loop
    
    // the directory was not found
    return -F_INEXISTENT_FILE_OR_DIR;
}

// Crawl directories until if finds the data index cluster for a given path.
static int64_t find_path_cluster(FFat32* f, const char* path, uint16_t* file_struct_ptr)
{
    // copy file path to a global variable, so we can change it
    size_t len = strlen(path);
    if (len >= MAX_FILE_PATH)
        return -F_FILE_PATH_TOO_LONG;
    strcpy(global_file_path, path);   // TODO - do we really need the global?
    char* file = global_file_path;
    
    // find starting cluster
    int64_t current_cluster;
    if (file[0] == '/') {   // absolute path
        current_cluster = f->reg.root_dir_cluster;
        ++file;  // skip intitial '/'
    } else {
        current_cluster = f->reg.current_dir_cluster;
    }
    
    while (1) {   // each iteration will crawl one directory
        
        len = strlen(file);   // is this the last directory?
        if (len == 0)
            return current_cluster;
        
        char* end = strchr(file, '/');
        
        if (end != NULL) {   // this is a directory: find dir cluster and continue crawling
            current_cluster = find_file_cluster_in_dir_cluster(f, file, end - file, current_cluster, file_struct_ptr);
            if (current_cluster < 0)   // file not found
                return current_cluster;
            file = end + 1;  // skip to next
            
        } else {  // this is the final file/dir in path: find cluster and return it
            return find_file_cluster_in_dir_cluster(f, file, len, current_cluster, file_struct_ptr);
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
    bool     found;
} DirEntryPtr;

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

static int64_t find_next_free_cluster(FFat32* f)
{
    uint32_t cluster_ptr = 0;
    
    // load next free cluster from FSINFO
    load_sector(f, FSINFO_SECTOR);
    int64_t hint_next_free_cluster = from_32(f->buffer, FSI_NEXT_FREE);
    
    // check if value is valid
    bool recalculate = false;
    if (hint_next_free_cluster == FSI_NO_VALUE) {
        recalculate = true;
    } else {
        FFatResult result = fat_find_first_free_cluster(f, hint_next_free_cluster, &cluster_ptr);
        if (result != F_OK)
            recalculate = true;
    }
    
    // recalculate next free cluster in FSINFO, if not valid
    if (recalculate)
        RETURN_UNLESS_F_OK(fsinfo_recalculate_next_free_cluster(f, &cluster_ptr))
    
    return cluster_ptr;
}

static DirEntryPtr find_next_free_directory_entry(FFat32* f, uint32_t path_cluster)
{
    DirEntryPtr dir_entry_ptr = {
            .cluster = path_cluster,
            .sector = 0,
            .entry_ptr = 0,
            .found = true,
    };
    
    // check all dir entries in the cluster
search_cluster:
    for (dir_entry_ptr.sector = 0; dir_entry_ptr.sector < f->reg.sectors_per_cluster; ++dir_entry_ptr.sector) {
        load_data_cluster(f, dir_entry_ptr.cluster, dir_entry_ptr.sector);
        for (dir_entry_ptr.entry_ptr = 0; dir_entry_ptr.entry_ptr < BYTES_PER_SECTOR; dir_entry_ptr.entry_ptr += DIR_ENTRY_SZ) {
            uint8_t first_chr = f->buffer[dir_entry_ptr.entry_ptr];
            if (first_chr == DIR_ENTRY_FREE1 || first_chr == DIR_ENTRY_FREE2)
                return dir_entry_ptr;
        }
    }
    
    // if not found, go to next cluster until EOC
    uint32_t next_cluster = fat_data_cluster_ptr(f, dir_entry_ptr.cluster);
    if (next_cluster != FAT_EOC && next_cluster != FAT_EOF) {
        dir_entry_ptr.cluster = next_cluster;
        goto search_cluster;
    }
    
    // not found
    dir_entry_ptr.found = false;
    return dir_entry_ptr;
}

static int64_t create_entry_in_directory(FFat32* f, uint32_t parent_dir_cluster, char filename[FILENAME_SZ],
                                         uint8_t attrib, uint16_t fat_datetime, uint32_t file_content_cluster)
{
    // find next free directory entry
    DirEntryPtr dir_entry_ptr = find_next_free_directory_entry(f, parent_dir_cluster);
    
    // if no directory free entry, append cluster
    if (!dir_entry_ptr.found) {
        RETURN_UNLESS_F_OK(fat_append_cluster(f, parent_dir_cluster, &parent_dir_cluster))
        dir_entry_ptr = (DirEntryPtr) {
            .cluster = parent_dir_cluster,
            .sector = 0,
            .entry_ptr = 0,
            .found = true,
        };
    }
    
    // create entry
    FDirEntry dir_entry = {
            .name = { 0 },
            .attrib = attrib,
            .nt_res = 0,
            .time_tenth = 0,
            .crt_datetime = fat_datetime,
            .last_acc_time = fat_datetime & 0xff,
            .cluster_high = (file_content_cluster >> 8),
            .wrt_datetime = fat_datetime,
            .cluster_low = file_content_cluster & 0xff,
            .file_size = 0
    };
    memcpy(dir_entry.name, filename, FILENAME_SZ);
    memcpy(&f->buffer[dir_entry_ptr.entry_ptr], &dir_entry, sizeof(FDirEntry));
    
    write_data_cluster(f, dir_entry_ptr.cluster, dir_entry_ptr.sector);
    
    return F_OK;
}

static void update_fsinfo(FFat32* f, uint32_t last_cluster, int64_t change_in_size)
{
    load_sector(f, FSINFO_SECTOR);
    to_32(f->buffer, FSI_NEXT_FREE, last_cluster);
    uint32_t current_count = from_32(f->buffer, FSI_FREE_COUNT);
    to_32(f->buffer, FSI_FREE_COUNT, (int64_t) current_count + change_in_size);
    write_sector(f, FSINFO_SECTOR);
}

static int64_t create_file_entry(FFat32* f, char* file_path, uint8_t attrib, uint32_t fat_datetime, uint32_t* parent_dir)
{
    // parse filename and find parent directory cluster
    char filename[FILENAME_SZ];
    split_path_and_filename(file_path, filename);
    int64_t path_cluster = find_path_cluster(f, file_path, NULL);
    if (path_cluster < 0)
        return path_cluster;
    *parent_dir = path_cluster;
    
    if (!validate_filename(filename))
        return -F_INVALID_FILENAME;
    
    // create a new cluster
    int64_t file_contents_cluster = find_next_free_cluster(f);
    if (file_contents_cluster < 0)
        return file_contents_cluster;
    fat_update_data_cluster_ptr(f, file_contents_cluster, FAT_EOF);
    
    // create directory entry in parent directory
    int64_t r;
    if ((r = create_entry_in_directory(f, path_cluster, filename, attrib, fat_datetime, file_contents_cluster)) < 0)
        return -r;
    
    // update FSINFO
    update_fsinfo(f, file_contents_cluster, +1);
    
    return file_contents_cluster;
}

// endregion

/********************/
/*  INITIALIZATION  */
/********************/

//region ...

static FFatResult f_init(FFat32* f)
{
    // check partition location
    f->read(MBR, f->buffer, f->data);
    if (f->buffer[0] == 0xeb) {  // this is a FAT partition
        f->reg.partition_start = 0;
    } else {
        f->reg.partition_start = from_32(f->buffer, PARTITION_TABLE_1);
        load_sector(f, BOOT_SECTOR);
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
    f->reg.data_start_sector = root_dir_sector + f->reg.partition_start;
    
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

static FFatResult f_free_r(FFat32* f)
{
    FSInfo fs_info;
    RETURN_UNLESS_F_OK(fsinfo_recalculate_values(f, &fs_info))
    uint32_t total = fs_info.free_cluster_count;
    
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

static FFatResult f_boot(FFat32* f)
{
    load_sector(f, BOOT_SECTOR);
    return F_OK;
}

//endregion

/**************************/
/*  DIRECTORY OPERATIONS  */
/**************************/

// region ...

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
    int64_t cluster = find_path_cluster(f, (const char*) f->buffer, NULL);
    if (cluster < 0) {
        return cluster * (-1);
    } else {
        f->reg.current_dir_cluster = cluster;
        return F_OK;
    }
}

static FFatResult f_mkdir(FFat32* f, uint32_t fat_datetime)
{
    uint32_t parent_dir_cluster;
    
    // create file entry
    int64_t cluster_self;
    if ((cluster_self = create_file_entry(f, (char *) f->buffer, ATTR_DIR, fat_datetime, &parent_dir_cluster)) < 0)
        return -cluster_self;
    
    // create empty directory structure ('.' and '..')
    char filename[FILENAME_SZ]; memset(filename, ' ', FILENAME_SZ);
    filename[0] = '.';
    create_entry_in_directory(f, cluster_self, filename, ATTR_DIR, fat_datetime, cluster_self);
    filename[1] = '.';
    create_entry_in_directory(f, cluster_self, filename, ATTR_DIR, fat_datetime, parent_dir_cluster);
    
    return F_OK;
}


// endregion

/************************/
/* FILE/DIR OPERATIONS  */
/************************/

// region ...

static FFatResult f_stat(FFat32* f)
{
    uint16_t addr;
    int64_t cluster = find_path_cluster(f, (const char*) f->buffer, &addr);
    if (cluster < 0)
        return -cluster;
    
    // set first 32 bits to file stat
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
        case F_INIT:   f->reg.last_operation_result = f_init(f);   break;
        case F_FREE:   f->reg.last_operation_result = f_free(f);   break;
        case F_FREE_R: f->reg.last_operation_result = f_free_r(f); break;
        case F_BOOT:   f->reg.last_operation_result = f_boot(f);   break;
        case F_DIR:    f->reg.last_operation_result = f_dir(f);    break;
        case F_CD:     f->reg.last_operation_result = f_cd(f);     break;
        case F_MKDIR:  f->reg.last_operation_result = f_mkdir(f, fat_datetime); break;
        case F_RMDIR:  break;
        case F_OPEN:   break;
        case F_CLOSE:  break;
        case F_READ:   break;
        case F_WRITE:  break;
        case F_STAT:   f->reg.last_operation_result = f_stat(f);   break;
        case F_RM:     break;
        case F_MV:     break;
        // TODO - find file
        default: f->reg.last_operation_result = F_INCORRECT_OPERATION;
    }
    return f->reg.last_operation_result;
}

// http://elm-chan.org/docs/fat_e.html
