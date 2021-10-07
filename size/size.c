#include "../src/ffat32.h"

int main()
{
    f_fat32(0, F_INIT);
    f_fat32(0, F_LABEL);
    f_fat32(0, F_FREE);
    f_fat32(0, F_FREE_R);
    f_fat32(0, F_CD);
    f_fat32(0, F_DIR);
    f_fat32(0, F_MKDIR);
    f_fat32(0, F_RMDIR);
    f_fat32(0, F_OPEN);
    f_fat32(0, F_CLOSE);
    f_fat32(0, F_READ);
    f_fat32(0, F_WRITE);
    f_fat32(0, F_STAT);
    f_fat32(0, F_RM);
    f_fat32(0, F_MV);
}