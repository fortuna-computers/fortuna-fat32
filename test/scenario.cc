#include "scenario.hh"

#include <cstring>
#include <fstream>

#include "../src/ffat32.h"
#include "ff/ff.h"

uint8_t Scenario::image_[512 * 1024 * 1024] = { 0 };
FATFS Scenario::fatfs;

PARTITION VolToPart[FF_VOLUMES] = {
        {0, 1},
        {0, 2}
};

std::vector<Scenario> Scenario::all_scenarios()
{
    std::vector<Scenario> scenarios;
    
    scenarios.emplace_back("Standard disk with directories and files", 1, DiskState::Complete, 256, 4);
    
    scenarios.emplace_back("Standard empty disk", 1, DiskState::Empty, 256, 4);
    scenarios.emplace_back("Standard disk with files in root dir", 1, DiskState::FilesInRoot, 256, 4);
    
    scenarios.emplace_back("Raw image without partitions", 0, DiskState::Complete, 256, 4);
    scenarios.emplace_back("Image with 2 partitions", 2, DiskState::Complete, 512, 4);
    
    scenarios.emplace_back("Disk with one sector per cluster", 1, DiskState::Complete, 64, 1);
    scenarios.emplace_back("Disk with 8 sectors per cluster", 1, DiskState::Complete, 512, 8);
    
    scenarios.emplace_back("Standard disk with 300 files", 1, DiskState::Files300, 256, 4);
    
    return scenarios;
}

void Scenario::store_image_in_disk(std::string const& filename) const
{
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<char const*>(image_), (disk_size * 1024 * 1024));
}

void Scenario::prepare_scenario() const
{
    // setup globals
    extern uint8_t* diskio_image;
    extern uint64_t diskio_size;
    
    diskio_image = image_;
    diskio_size = (disk_size * 1024 * 1024) / 512;
    
    // prepare image
    // clear_disk();
    partition_disk();
    format_disk();
    switch (disk_state) {
        case DiskState::Empty:
            break;
        case DiskState::FilesInRoot:
            add_files_in_root();
            break;
        case DiskState::Complete:
            add_complete_files();
            break;
        case DiskState::Files300:
            add_many_files(300);
            break;
        case DiskState::Files512:
            add_many_files(512);
            break;
    }
}

void Scenario::end_scenario() const
{
    R(f_mount(nullptr, "", 0));
}

void Scenario::remount() const
{
    R(f_mount(nullptr, "", 0));
    
    memset(&fatfs, 0, sizeof(FATFS));
    R(f_mount(&fatfs, "", 0));
}

void Scenario::clear_disk() const {
    memset(image_, 0, sizeof image_);
}

void Scenario::format_disk() const
{
    BYTE work[FF_MAX_SS];
    
    MKFS_PARM mkfs_parm = {
            .fmt = FM_FAT32,
            .n_fat = 2,
            .align = 1,
            .n_root = 0,
            .au_size = sectors_per_cluster * 512U,
    };
    R(f_mkfs("", &mkfs_parm, work, sizeof work));
    
    memset(&fatfs, 0, sizeof(FATFS));
    R(f_mount(&fatfs, "", 0));
    
    R(f_setlabel("FORTUNA"));
}

void Scenario::partition_disk() const
{
    BYTE work[FF_MAX_SS];
    
    if (partitions == 1) {
        LBA_t lba[] = { 100, 0 };
        R(f_fdisk(0, lba, work));
    } else if (partitions == 2) {
        LBA_t lba[] = { 50, 50, 0 };
        R(f_fdisk(0, lba, work));
    }
}

void Scenario::add_files_in_root() const
{
    extern uint8_t _binary_test_TAGS_TXT_start[];
    extern uint8_t _binary_test_TAGS_TXT_size;
    
    FIL fp;
    UINT bw;
    
    R(f_open(&fp, "HELLO.TXT", FA_CREATE_NEW | FA_WRITE));
    const char* contents = "Hello world!";
    R(f_write(&fp, contents, strlen(contents), &bw));
    R(f_close(&fp));
    
    R(f_open(&fp, "TAGS.TXT", FA_CREATE_NEW | FA_WRITE));
    R(f_write(&fp, _binary_test_TAGS_TXT_start, (size_t) &_binary_test_TAGS_TXT_size, &bw));
    R(f_close(&fp));
}

void Scenario::add_complete_files() const
{
    extern uint8_t _binary_test_TAGS_TXT_start[];
    extern uint8_t _binary_test_TAGS_TXT_end[];
    
    FIL fp;
    UINT bw;
    
    R(f_mkdir("HELLO"));
    R(f_mkdir("HELLO/FORTUNA"));   // empty dir
    R(f_mkdir("HELLO/WORLD"));
    
    R(f_open(&fp, "FORTUNA.DAT", FA_CREATE_NEW | FA_WRITE));
    const char* contents = "Hello world!";
    R(f_write(&fp, contents, strlen(contents), &bw));
    R(f_close(&fp));
    
    R(f_open(&fp, "TAGS.TXT", FA_CREATE_NEW | FA_WRITE));
    R(f_write(&fp, _binary_test_TAGS_TXT_start, _binary_test_TAGS_TXT_end - _binary_test_TAGS_TXT_start, &bw));
    R(f_close(&fp));
    
    R(f_open(&fp, "HELLO/WORLD/HELLO.TXT", FA_CREATE_NEW | FA_WRITE));
    contents = "Hello world!";
    R(f_write(&fp, contents, strlen(contents), &bw));
    R(f_close(&fp));
}

void Scenario::add_many_files(size_t count) const
{
    FIL fp;
    UINT bw;
    
    for (size_t i = 1; i <= count; ++i) {
        char filename[12];
        sprintf(filename, "FILE%03zu.BIN", i);
        R(f_open(&fp, filename, FA_CREATE_NEW | FA_WRITE));
        const char* contents = "File contents";
        R(f_write(&fp, contents, strlen(contents), &bw));
        R(f_close(&fp));
    }
}

void Scenario::R(FRESULT fresult) const
{
    if (fresult != FR_OK)
        throw std::runtime_error("FatFS operation failed");
}

DWORD Scenario::get_free_space() const
{
    DWORD found;
    FATFS* f = &fatfs;
    R(f_getfree("", &found, &f));
    return found;
}
