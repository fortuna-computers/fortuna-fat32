/* LAYER 2 of FatFS -- implemented here are functions to deal with:
 *
 *   - Files
 *   - Directory entries
 *
 * All functions here are PRIVATE.
 */

#ifndef FORTUNA_FAT32_FILE_H
#define FORTUNA_FAT32_FILE_H

#include "ffat32.h"

FFatResult file_init(FFat32* f);

#endif //FORTUNA_FAT32_FILE_H
