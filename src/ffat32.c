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
    TRY(sections_fsinfo_get(f, &fsinfo))
    if (fsinfo.free_cluster_count == 0xffffffff)
        TRY(sections_fsinfo_recalculate(f, &fsinfo))
    BUF_SET32(f, 0, fsinfo.free_cluster_count)
    return F_OK;
}

FFatResult f_fat32(FFat32* f, FFat32Op operation, uint32_t fat_datetime)
{
    switch (operation) {
        
        case F_INIT:          f->reg.last_operation_result = f_init(f); break;
        case F_BOOT:          f->reg.last_operation_result = f_boot(f); break;
        case F_FREE:          f->reg.last_operation_result = f_free(f); break;
        case F_FSINFO_RECALC: break;
        case F_DIR:           break;
        case F_CD:            break;
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

// http://elm-chan.org/docs/fat_e.html
