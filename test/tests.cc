#include "test.hh"

#include <cstring>
#include <iostream>

#include "helper.hh"

#define BYTES_PER_SECTOR 512

static std::vector<File> directory;
static FFatResult result;

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
                f_fat32(ffat, F_LABEL, 0);
            },
            
            [](uint8_t const* buffer, Scenario const&) {
                char label[50];
                if (f_getlabel("", label, nullptr) != FR_OK)
                    throw std::runtime_error("f_getlabel error");
                return strcmp((char const*) buffer, label) == 0;
            }
    );
    
    tests.emplace_back(
            "Check disk space (pre-existing)",
            
            [](FFat32* ffat, Scenario const&) {
                f_fat32(ffat, F_FREE, 0);
            },
            
            [](uint8_t const* buffer, Scenario const& scenario) {
                uint32_t free_ = *(uint32_t *) buffer;
                DWORD found = scenario.get_free_space();
                return free_ == found;
            }
    );
    
    tests.emplace_back(
            "Check disk space (calculate)",

            [](FFat32* ffat, Scenario const&) {
                f_fat32(ffat, F_FREE_R, 0);
            },

            [](uint8_t const* buffer, Scenario const& scenario) {
                uint32_t free_ = *(uint32_t *) buffer;
                DWORD found = scenario.get_free_space();
                return abs((int) free_ - (int) found) < 1024;
            }
    );

#if 0
    {
        uint32_t free_1st_check, free_2nd_check;
        tests.emplace_back(
                "Check disk space (calculate + read)",

                [&](FFat32* ffat, Scenario const&) {
                    f_fat32(ffat, F_FREE_R, 0);
                    free_1st_check = *(uint32_t *) ffat->buffer;
                    f_fat32(ffat, F_FREE, 0);
                    free_2nd_check = *(uint32_t *) ffat->buffer;
                },
                
                [&](uint8_t const*, Scenario const&) {
                    return free_1st_check == free_2nd_check;
                }
        );
    }
  #endif
    
    tests.emplace_back(
            "Load boot sector",

            [&](FFat32* ffat, Scenario const&) {
                result = f_fat32(ffat, F_BOOT, 0);
            },

            [&](uint8_t const* buffer, Scenario const&) {
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
                        r = f_fat32(ffat, F_DIR, 0);
                        if (r != F_OK && r != F_MORE_DATA)
                            throw std::runtime_error("F_DIR reported error " + std::to_string(r));
                        add_files_to_dir_structure(ffat->buffer, directory);
                        ffat->buffer[0] = F_CONTINUE;
                    } while (r == F_MORE_DATA);
                },
                
                [&](uint8_t const*, Scenario const&) {
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
                    result = f_fat32(ffat, F_CD, 0);
                    ffat->buffer[0] = F_START_OVER;
                    f_fat32(ffat, F_DIR, 0);
                },

                [&](uint8_t const* buffer, Scenario const& scenario) {
                    if (scenario.disk_state == Scenario::DiskState::Complete) {
                        if (result != F_OK)
                            return false;
                        
                        if (!check_for_files_in_directory(buffer, directory, { "FORTUNA", "WORLD" }))
                            return false;
                        
                        return true;
                        
                    } else {
                        return result == F_INEXISTENT_FILE_OR_DIR;
                    }
                }
        );
    
        tests.emplace_back(
                "Cd to directory (relative path with slash at the end)",
            
                [&](FFat32* ffat, Scenario const&) {
                    strcpy(reinterpret_cast<char*>(ffat->buffer), "HELLO/");
                    result = f_fat32(ffat, F_CD, 0);
                    ffat->buffer[0] = F_START_OVER;
                    f_fat32(ffat, F_DIR, 0);
                },
            
                [&](uint8_t const* buffer, Scenario const& scenario) {
                    if (scenario.disk_state == Scenario::DiskState::Complete) {
                        if (result != F_OK)
                            return false;
                    
                        if (!check_for_files_in_directory(buffer, directory, { "FORTUNA", "WORLD" }))
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
                    result = f_fat32(ffat, F_CD, 0);
                    if (scenario.disk_state == Scenario::DiskState::Complete && result != F_OK)
                        throw std::runtime_error("Unexpected result.");
                    strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/WORLD");
                    result = f_fat32(ffat, F_CD, 0);
                    ffat->buffer[0] = F_START_OVER;
                    f_fat32(ffat, F_DIR, 0);
                },
            
                [&](uint8_t const* buffer, Scenario const& scenario) {
                    if (scenario.disk_state == Scenario::DiskState::Complete) {
                        if (result != F_OK)
                            return false;
    
                        if (!check_for_files_in_directory(buffer, directory, { "HELLO.TXT" }))
                            return false;
                    
                        return true;
                    
                    } else {
                        return result == F_INEXISTENT_FILE_OR_DIR;
                    }
                }
        );
    
        tests.emplace_back(
                "Cd to root ('/') and dir",
            
                [&](FFat32* ffat, Scenario const&) {
                    strcpy(reinterpret_cast<char*>(ffat->buffer), "/");
                    result = f_fat32(ffat, F_CD, 0);
                    ffat->buffer[0] = F_START_OVER;
                    f_fat32(ffat, F_DIR, 0);
                },
            
                [&](uint8_t const* buffer, Scenario const& scenario) {
                    if (result != F_OK)
                        return false;
    
                    std::vector<std::string> files;
                    switch (scenario.disk_state) {
                        case Scenario::DiskState::Empty:
                            return true;
                        case Scenario::DiskState::FilesInRoot:
                            files = { "HELLO.TXT", "TAGS.TXT" };
                            break;
                        case Scenario::DiskState::Complete:
                            files = { "HELLO", "FORTUNA.DAT", "TAGS.TXT" };
                            break;
                        case Scenario::DiskState::Files300:
                        case Scenario::DiskState::Files512:
                            for (size_t i = 1; i < 10; ++i) {
                                char buf[16];
                                sprintf(buf, "FILE%03zu.BIN", i);
                                files.emplace_back(buf);
                            }
                            break;
                    }
    
                    if (!check_for_files_in_directory(buffer, directory, files))
                        return false;
    
                    return true;
                }
        );
    }
    
    // endregion
    
    tests.emplace_back(
            "Create dir at root",

            [&](FFat32* f, Scenario const&) {
                strcpy((char *) f->buffer, "TEST");
                result = f_fat32(f, F_MKDIR, 1981);
            },

            [&](uint8_t const*, Scenario const&) {
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                FRESULT fresult = f_stat("/TEST", &filinfo);
                if (fresult != FR_OK || filinfo.ftime != 1981 || !(filinfo.fattrib & AM_DIR))
                    return false;
                
                return true;
            }
    );
    
    tests.emplace_back(
            "Create dir at absolute location",
            
            [&](FFat32* f, Scenario const&) {
                strcpy((char *) f->buffer, "/HELLO/FORTUNA/TEST");
                result = f_fat32(f, F_MKDIR, 1981);
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return result != F_OK;
                    
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                FRESULT fresult = f_stat("/HELLO/FORTUNA/TEST", &filinfo);
                if (fresult != FR_OK || filinfo.ftime != 1981 || !(filinfo.fattrib & AM_DIR))
                    return false;
                
                return true;
            }
    );
    
    // TODO - create a dir in a dir with exactly 512 files
    
    // TODO - remove dir

    //
    // STAT
    //
    
    // region ...
    
    tests.emplace_back(
            "Test file stat (directory, relative)",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "HELLO");
                result = f_fat32(ffat, F_STAT, 0);
            },
            
            [&](uint8_t const* buffer, Scenario const& scenario) {
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
                result = f_fat32(ffat, F_STAT, 0);
            },
            
            [&](uint8_t const* buffer, Scenario const& scenario) {
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
                result = f_fat32(ffat, F_STAT, 0);
            },
            
            [&](uint8_t const* buffer, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_INEXISTENT_FILE_OR_DIR)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                return (buffer[11] & 0x10) != 0;   // attr is directory
            }
    );
    
    // endregion
    
    //
    // FILE OPERATIONS
    //
    
    // TODO - open file, read file, close file
    
    // TODO - create file, write file, remove file
    
    //
    // MOVE
    //
    
    // TODO - move file
    
    // TODO - move directory
    
    return tests;
}


