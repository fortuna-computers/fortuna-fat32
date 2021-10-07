#include <vector>
#include <cstring>
#include <iostream>
#include <string>
#include "test.hh"
#include "../src/internal.h"

extern FFat32Variables var;

#define BYTES_PER_SECTOR 512

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
        struct File {
            std::string name;
            bool        is_dir;
            uint32_t    size;
    
            File(std::string const& name, bool is_dir, uint32_t size) : name(name), is_dir(is_dir), size(size) {}
        };
        
        auto add_files_to_dir_structure = [](uint8_t const* buffer, std::vector<File>& directory)
        {
            for (size_t i = 0; i < 512 / 32; ++i) {
                if (buffer[i * 32] == 0)
                    break;
                char buf[12] = { 0 };
                strncpy(buf, (char*) &buffer[i * 32], 11);
                std::string name = std::string(buf);
                bool is_dir = buffer[i * 32 + 0xb];
                uint32_t size = *(uint32_t *) &buffer[i * 32 + 0x1c];
                directory.emplace_back(name, is_dir, size);
            }
        };
        
        std::vector<File> directory;
        
        tests.emplace_back(
                "Check directories",

                [&](FFat32* ffat, Scenario const&) {
                    directory.clear();
                    FFatResult r;
                    do {
                        r = f_fat32(ffat, F_DIR);
                        if (r != F_OK && r != F_MORE_DATA)
                            throw std::runtime_error("F_DIR reported error " + std::to_string(r));
                        add_files_to_dir_structure(ffat->buffer, directory);
                    } while (r == F_MORE_DATA);
                },
                
                [&](uint8_t const*, Scenario const&, FATFS*) {
                    return false;
                }
        );
    }
    
    // endregion
    
    return tests;
}


