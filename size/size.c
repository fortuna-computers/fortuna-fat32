#include "../src/ffat32.h"

int main()
{
    FFat32 f;
    f_fat32(&f, F_INIT, 0);
    f_fat32(&f, F_FREE, 0);
    f_fat32(&f, F_FREE_R, 0);
    f_fat32(&f, F_CD, 0);
    f_fat32(&f, F_DIR, 0);
    f_fat32(&f, F_MKDIR, 0);
    f_fat32(&f, F_RMDIR, 0);
    f_fat32(&f, F_OPEN, 0);
    f_fat32(&f, F_CLOSE, 0);
    f_fat32(&f, F_READ, 0);
    f_fat32(&f, F_WRITE, 0);
    f_fat32(&f, F_STAT, 0);
    f_fat32(&f, F_RM, 0);
    f_fat32(&f, F_MV, 0);
}