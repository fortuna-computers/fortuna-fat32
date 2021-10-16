#include "scenario.hh"

#include <cstring>

#include "../src/ffat32.h"
#include "ff/ff.h"

uint8_t Scenario::image_[512 * 1024 * 1024] = { 0 };

std::vector<Scenario> Scenario::all_scenarios()
{
    std::vector<Scenario> scenarios;
    
    size_t i = 0;
    
    scenarios.emplace_back(i++, "Standard disk with directories and files", 1, DiskState::Complete, 256, 4);
    
    scenarios.emplace_back(i++, "Standard empty disk", 1, DiskState::Empty, 256, 4);
    scenarios.emplace_back(i++, "Standard disk with files in root dir", 1, DiskState::FilesInRoot, 256, 4);
    
    scenarios.emplace_back(i++, "Raw image without partitions", 0, DiskState::Complete, 256, 4);
    scenarios.emplace_back(i++, "Image with 2 partitions", 2, DiskState::Complete, 256, 4);
    
    scenarios.emplace_back(i++, "Disk with one sector per cluster", 1, DiskState::Complete, 64, 1);
    scenarios.emplace_back(i++, "Disk with 8 sectors per cluster", 1, DiskState::Complete, 512, 8);
    
    scenarios.emplace_back(i++, "Standard disk with 300 files", 1, DiskState::ManyFiles, 256, 4);
    
    return scenarios;
}

PARTITION VolToPart[FF_VOLUMES] = {
        {0, 1},
        {0, 2}
};

uint8_t* Scenario::prepare_image() const
{
    extern uint8_t* diskio_image;
    extern uint64_t diskio_size;
    
    FRESULT fresult = FR_OK;
    BYTE work[FF_MAX_SS];
    
    // memset(image_, 0, sizeof image_);
    
    diskio_image = image_;
    diskio_size = disk_size * 1024 * 1024;
    
    if (partitions == 1) {
        LBA_t lba[] = { 100, 0 };
        fresult = f_fdisk(0, lba, work);
    } else if (partitions == 2) {
        LBA_t lba[] = { 50, 50, 0 };
        fresult = f_fdisk(0, lba, work);
    }
    if (fresult != FR_OK)
        throw std::runtime_error("`f_fdisk` failed");
    
    MKFS_PARM mkfs_parm = {
            .fmt = FM_FAT32,
            .n_fat = 2,
            .align = 512,
            .n_root = 0,
            .au_size = sectors_per_cluster * 512U,
    };
    fresult = f_mkfs("", &mkfs_parm, work, sizeof work);
    if (fresult != FR_OK)
        throw std::runtime_error("`f_mkfs` failed");
    
    return nullptr;
}
