#include <vector>
#include <cstring>
#include <iostream>
#include <string>
#include "test.hh"

#define BYTES_PER_SECTOR 512

// region Utils

struct File {
    std::string name;
    uint8_t     attr;
    uint32_t    size;
    
    File(std::string const& name, uint8_t attr, uint32_t size) : name(name), attr(attr), size(size) {}
};

static std::vector<File> directory;
static FFatResult result;

// endregion

std::vector<Test> prepare_tests()
{
    std::vector<Test> tests;
    
    //
    // DISK OPERATIONS
    //
    
    // region ...
    
    tests.emplace_back(
            "Check label",
            
            [](FFat32* ffat, Scenario const&) {
                f_fat32(ffat, F_LABEL);
            },
            
            [](uint8_t const* buffer, Scenario const&, FATFS*) {
                char label[50];
                if (f_getlabel("", label, nullptr) != FR_OK)
                    throw std::runtime_error("f_getlabel error");
                return strcmp((char const*) buffer, label) == 0;
            }
    );
    
    tests.emplace_back(
            "Check disk space (pre-existing)",
            
            [](FFat32* ffat, Scenario const&) {
                f_fat32(ffat, F_FREE);
            },
            
            [](uint8_t const* buffer, Scenario const&, FATFS* fatfs) {
                uint32_t free_ = *(uint32_t *) buffer;
                DWORD found;
                if (f_getfree("", &found, &fatfs) != FR_OK)
                    throw std::runtime_error("f_getfree error");
                return free_ == found;
            }
    );
    
    tests.emplace_back(
            "Check disk space (calculate)",

            [](FFat32* ffat, Scenario const&) {
                f_fat32(ffat, F_FREE_R);
            },

            [](uint8_t const* buffer, Scenario const&, FATFS* fatfs) {
                uint32_t free_ = *(uint32_t *) buffer;
                DWORD found;
                if (f_getfree("", &found, &fatfs) != FR_OK)
                    throw std::runtime_error("f_getfree error");
                return abs((int) free_ - (int) found) < 1024;
            }
    );
    
    {
        uint32_t free_1st_check, free_2nd_check;
        tests.emplace_back(
                "Check disk space (calculate + read)",

                [&](FFat32* ffat, Scenario const&) {
                    f_fat32(ffat, F_FREE_R);
                    free_1st_check = *(uint32_t *) ffat->buffer;
                    f_fat32(ffat, F_FREE);
                    free_2nd_check = *(uint32_t *) ffat->buffer;
                },
                
                [&](uint8_t const*, Scenario const&, FATFS*) {
                    return free_1st_check == free_2nd_check;
                }
        );
    }
    
    // endregion
    
    //
    // DIRECTORY OPERATIONS
    //
    
    // region ...
    
    {
        auto build_name = [](const char* name) {
            std::string filename = std::string(name, 8);
            while (filename.back() == ' ')
                filename.pop_back();
                
            std::string extension = std::string(&name[8]);
            while (extension.back() == ' ')
                extension.pop_back();
            
            if (extension.empty())
                return filename;
            else
                return filename + "." + extension;
        };
        
        auto add_files_to_dir_structure = [&](uint8_t const* buffer, std::vector<File>& directory)
        {
            for (size_t i = 0; i < 512 / 32; ++i) {
                if (buffer[i * 32] == 0)
                    break;
                char buf[12] = { 0 };
                strncpy(buf, (char*) &buffer[i * 32], 11);
                std::string name = build_name(buf);
                bool is_dir = buffer[i * 32 + 0xb];
                uint32_t size = *(uint32_t *) &buffer[i * 32 + 0x1c];
                directory.emplace_back(name, is_dir, size);
            }
        };
        
        auto find_file_in_directory = [](FILINFO const* filinfo, std::vector<File>& directory)
        {
            auto it = std::find_if(directory.begin(), directory.end(),
                                   [&](File const& file) { return file.name == std::string(filinfo->fname); });
            if (it == directory.end())
                return false;
            
            return it->size == filinfo->fsize;
        };
        
        tests.emplace_back(
                "Check directories",

                [&](FFat32* ffat, Scenario const&) {
                    directory.clear();
                    FFatResult r;
                    ffat->buffer[0] = F_START_OVER;
                    do {
                        r = f_fat32(ffat, F_DIR);
                        if (r != F_OK && r != F_MORE_DATA)
                            throw std::runtime_error("F_DIR reported error " + std::to_string(r));
                        add_files_to_dir_structure(ffat->buffer, directory);
                        ffat->buffer[0] = F_CONTINUE;
                    } while (r == F_MORE_DATA);
                },
                
                [&](uint8_t const*, Scenario const&, FATFS*) {
                    DIR dp;
                    FILINFO filinfo;
                    if (f_opendir(&dp, "/") != FR_OK)
                        throw std::runtime_error("`f_opendir` reported error");
                    
                    for (;;) {
                        if (f_readdir(&dp, &filinfo)  != FR_OK)
                            throw std::runtime_error("`f_readdir` reported error");
                        
                        if (filinfo.fname[0] == '\0')
                            break;
                        
                        if (!find_file_in_directory(&filinfo, directory))
                            return false;
                    }
                    
                    if (f_closedir(&dp) != FR_OK)
                        throw std::runtime_error("`f_opendir` reported error");
                    
                    return true;
                }
        );
        
        tests.emplace_back(
                "cd to directory",

                [&](FFat32* ffat, Scenario const&) {
                    strcpy(reinterpret_cast<char*>(ffat->buffer), "HELLO");
                    result = f_fat32(ffat, F_CD);
                    ffat->buffer[0] = F_START_OVER;
                    f_fat32(ffat, F_DIR);
                },

                [&](uint8_t const* buffer, Scenario const& scenario, FATFS*) {
                    if (scenario.disk_state == Scenario::DiskState::Complete) {
                        if (result != F_OK)
                            return false;
                        add_files_to_dir_structure(buffer, directory);  // read files in directory
                        if (std::find_if(directory.begin(), directory.end(),
                                         [](File const& file) { return file.name == "FORTUNA"; }) == directory.end())
                            return false;
                        if (std::find_if(directory.begin(), directory.end(),
                                         [](File const& file) { return file.name == "WORLD"; }) == directory.end())
                            return false;
                        
                        return true;
                        
                    } else {
                        return result == F_INEXISTENT_DIRECTORY;
                    }
                }
                
        );
    }
    
    // endregion
    
    return tests;
}


