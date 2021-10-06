#include "ffat32.h"

#include <string.h>

#include "internal.h"

FFat32Registers reg = { 0 };

static uint32_t from_32(uint8_t const* buffer, uint16_t pos)
{
    return *(uint32_t *) &buffer[pos];
}

static void f_init(FFat32Def* def)
{
    def->read(0, def->buffer, def->data);
    
    if (def->buffer[0] == 0xfa) {  // this is a MBR
        reg.partition_start = from_32(def->buffer, 0x1c6);
    } else {
        reg.partition_start = 0;
    }
    // TODO - fill out fields
}

static void f_label(FFat32Def* def)
{
    def->read(reg.partition_start + 0, def->buffer, def->data);
    memcpy(def->buffer, &def->buffer[0x47], 11);
    for (int8_t i = 10; i >= 0; --i) {
        if (def->buffer[i] == ' ')
            def->buffer[i] = '\0';
        else
            break;
    }
}

uint8_t f_fat32(FFat32Def* def, FFat32Op operation)
{
    switch (operation) {
        case F_INIT: f_init(def); break;
        case F_FREE: break;
        case F_LABEL: f_label(def); break;
        case F_CD: break;
        case F_DIR: break;
        case F_MKDIR: break;
        case F_OPEN: break;
        case F_CLOSE: break;
        case F_READ: break;
        case F_WRITE: break;
        case F_STAT: break;
        case F_RM: break;
        case F_MV: break;
    }
    return 0;
}