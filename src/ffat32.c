#include "ffat32.h"

#include "common.h"
#include "sections.h"
#include "file.h"

static FFatResult f_init(FFat32* f)
{
    TRY(file_init(f))
    return F_OK;
}

static FFatResult f_boot(FFat32* f)
{
    TRY(sections_load_boot_sector(f))
    return F_OK;
}

static FFatResult f_free(FFat32* f)
{
    FSInfo fsinfo;
    TRY(sections_fsinfo_read(f, &fsinfo))
    if (fsinfo.free_cluster_count == FSI_INVALID)
        TRY(sections_fsinfo_recalculate(f, &fsinfo))
    BUF_SET32(f, 0, fsinfo.free_cluster_count)
    return F_OK;
}

static FFatResult f_fsinfo_recalc(FFat32* f)
{
    FSInfo fsinfo;
    TRY(sections_fsinfo_recalculate(f, &fsinfo))
    return F_OK;
}

static FFatResult f_dir(FFat32* f)
{
    FContinuation continuation = BUF_GET8(f, 0);
    FFatResult result = file_list_current_dir(f, continuation);
    TRY(result)
    return result;
}

static FFatResult f_cd(FFat32* f)
{
    TRY(file_change_current_dir(f, (const char*) f->buffer))
    return F_OK;
}

static FFatResult f_mkdir(FFat32* f, uint32_t fat_datetime)
{
    TRY(file_create_dir(f, (char*) f->buffer, fat_datetime))
    return F_OK;
}

static FFatResult f_open(FFat32* f)
{
    FILE_IDX file_idx = 0;
    TRY(file_open(f, (char *) f->buffer, &file_idx))
    f->reg.F_FLN = file_idx;
    return F_OK;
}

static FFatResult f_read(FFat32* f)
{
    FILE_IDX file_idx = f->reg.F_FLN;
    uint16_t file_sector_length;
    FFatResult result = file_read(f, file_idx, &file_sector_length);
    TRY(result)
    f->reg.F_SZ = file_sector_length;
    return result;
}

static FFatResult f_close(FFat32* f)
{
    FILE_IDX file_idx = f->reg.F_FLN;
    TRY(file_close(f, file_idx))
    return F_OK;
}

static FFatResult f_seek(FFat32* f)
{
    FILE_IDX file_idx = f->reg.F_FLN;
    uint32_t count = BUF_GET32(f, 0);
    uint16_t file_sector_length;
    FFatResult result = file_seek_forward(f, file_idx, count, &file_sector_length);
    TRY(result)
    f->reg.F_SZ = file_sector_length;
    return result;
}

FFatResult f_fat32(FFat32* f, FFat32Op operation, uint32_t fat_datetime)
{
    switch (operation) {
        
        case F_INIT:          f->reg.F_RSLT = f_init(f);                break;
        case F_BOOT:          f->reg.F_RSLT = f_boot(f);                break;
        case F_FREE:          f->reg.F_RSLT = f_free(f);                break;
        case F_FSINFO_RECALC: f->reg.F_RSLT = f_fsinfo_recalc(f);       break;
        case F_DIR:           f->reg.F_RSLT = f_dir(f);                 break;
        case F_CD:            f->reg.F_RSLT = f_cd(f);                  break;
        case F_MKDIR:         f->reg.F_RSLT = f_mkdir(f, fat_datetime); break;
        case F_RMDIR:         break;
        case F_OPEN:          f->reg.F_RSLT = f_open(f);                break;
        case F_CLOSE:         f->reg.F_RSLT = f_close(f);               break;
        case F_READ:          f->reg.F_RSLT = f_read(f);                break;
        case F_SEEK:          f->reg.F_RSLT = f_seek(f);                break;
        case F_WRITE:         break;
        case F_STAT:          break;
        case F_RM:            break;
        case F_MV:            break;
        default:              f->reg.F_RSLT = F_INCORRECT_OPERATION;
    }
    return f->reg.F_RSLT;
}

const char* f_error(FFatResult result)
{
    switch (result) {
        case F_OK:                       return "Ok";
        case F_MORE_DATA:                return "Ok, more data available";
        case F_IO_ERROR:                 return "I/O error";
        case F_INCORRECT_OPERATION:      return "Incorrect operation";
        case F_NOT_FAT_32:               return "Not a Fat32 filesystem";
        case F_BYTES_PER_SECTOR_NOT_512: return "Bytes per sector is not 512 bytes";
        case F_PATH_NOT_FOUND:           return "Path not found";
        case F_FILE_PATH_TOO_LONG:       return "File path too long";
        case F_INVALID_FILENAME:         return "Invalid filename";
        case F_DEVICE_FULL:              return "Device is full";
        case F_DIR_NOT_EMPTY:            return "Directory not empty";
        case F_NOT_A_DIRECTORY:          return "Not a directory";
        case F_FILE_ALREADY_EXISTS:      return "File already exists";
        case F_TOO_MANY_FILES_OPEN:      return "Too many files open";
        case F_INVALID_FILE_INDEX:       return "Invalid file index";
        case F_FILE_NOT_OPEN:            return "File is not open";
        case F_SEEK_PAST_EOF:            return "Seeked past end-of-file";
        default:                         return "Unexpected error";
    }
}

#ifdef FFAT_DEBUG
#include <stdio.h>
#include "io.h"

void f_fat32_debug(FFat32* f)
{
    printf("\n=================================\n");
    io_debug(f);
    sections_debug(f);
    file_debug(f);
    printf("=================================\n");
}

#endif

// http://elm-chan.org/docs/fat_e.html
