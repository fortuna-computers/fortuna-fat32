#include "file.h"

#include "common.h"
#include "sections.h"

FFatResult file_init(FFat32* f)
{
    TRY(sections_init(f))
    return F_OK;
}
