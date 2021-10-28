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
    TRY(file_list_current_dir(f, continuation))
    return F_OK;
}

static FFatResult f_cd(FFat32* f)
{
    TRY(file_change_current_dir(f, (const char*) f->buffer))
    return F_OK;
}

FFatResult f_fat32(FFat32* f, FFat32Op operation, uint32_t fat_datetime)
{
    switch (operation) {
        
        case F_INIT:          f->reg.last_operation_result = f_init(f);          break;
        case F_BOOT:          f->reg.last_operation_result = f_boot(f);          break;
        case F_FREE:          f->reg.last_operation_result = f_free(f);          break;
        case F_FSINFO_RECALC: f->reg.last_operation_result = f_fsinfo_recalc(f); break;
        case F_DIR:           f->reg.last_operation_result = f_dir(f);           break;
        case F_CD:            f->reg.last_operation_result = f_cd(f);            break;
        case F_MKDIR:         break;
        case F_RMDIR:         break;
        case F_OPEN:          break;
        case F_CLOSE:         break;
        case F_READ:          break;
        case F_WRITE:         break;
        case F_STAT:          break;
        case F_RM:            break;
        case F_MV:            break;
        default:              f->reg.last_operation_result = F_INCORRECT_OPERATION;
    }
    return f->reg.last_operation_result;
}

#ifdef FFAT_DEBUG
#include <stdio.h>

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
