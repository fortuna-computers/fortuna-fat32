#include "test.hh"

#include <cstring>
#include <iostream>

#include "helper.hh"

#define BYTES_PER_SECTOR 512

// region Utils

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
    
    tests.emplace_back(
            "Load boot sector",

            [&](FFat32* ffat, Scenario const&) {
                result = f_fat32(ffat, F_BOOT);
            },

            [&](uint8_t const* buffer, Scenario const&, FATFS*) {
                return result == F_OK
                    && buffer[0x0] == 0xeb
                    && *(uint16_t *) &buffer[510] == 0xaa55;
            }
    );
    
    // endregion
    
    //
    // DIRECTORY OPERATIONS
    //
    
    // region ...
    
    {
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
                "Cd to directory (relative path)",

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
                        return result == F_INEXISTENT_FILE_OR_DIR;
                    }
                }
        );
    
        tests.emplace_back(
                "Cd to directory (absolute path)",
            
                [&](FFat32* ffat, Scenario const& scenario) {
                    strcpy(reinterpret_cast<char*>(ffat->buffer), "HELLO");
                    result = f_fat32(ffat, F_CD);
                    if (scenario.disk_state == Scenario::DiskState::Complete && result != F_OK)
                        throw std::runtime_error("Unexpected result.");
                    strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/WORLD");
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
                                         [](File const& file) { return file.name == "HELLO.TXT"; }) == directory.end())
                            return false;
                    
                        return true;
                    
                    } else {
                        return result == F_INEXISTENT_FILE_OR_DIR;
                    }
                }
        );
    }
    
    // endregion
    
    //
    // FILE/DIRECTORY OPERATIONS
    //
    
    tests.emplace_back(
            "Test file stat (directory, relative)",

            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "HELLO");
                result = f_fat32(ffat, F_STAT);
            },

            [&](uint8_t const* buffer, Scenario const& scenario, FATFS*) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_INEXISTENT_FILE_OR_DIR)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                return (buffer[11] & 0x10) != 0;   // attr is directory
            }
    );
    
    tests.emplace_back(
            "Test file stat (file, absolute)",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/WORLD/HELLO.TXT");
                result = f_fat32(ffat, F_STAT);
            },
            
            [&](uint8_t const* buffer, Scenario const& scenario, FATFS*) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_INEXISTENT_FILE_OR_DIR)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                if (f_stat("/HELLO/WORLD/HELLO.TXT", &filinfo) != FR_OK)
                    throw std::runtime_error("`f_stat` reported error");
    
                uint32_t reported_size = *(uint32_t *) &buffer[28];
                return reported_size == filinfo.fsize;
            }
    );
    
    tests.emplace_back(
            "Test file stat (directory, absolute, end in slash)",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/");
                result = f_fat32(ffat, F_STAT);
            },
            
            [&](uint8_t const* buffer, Scenario const& scenario, FATFS*) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_INEXISTENT_FILE_OR_DIR)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                return (buffer[11] & 0x10) != 0;   // attr is directory
            }
    );
    
    return tests;
}


