#include "test.hh"

#include <cstring>
#include <iostream>

#include "helper.hh"

#define BYTES_PER_SECTOR 512

static std::vector<File> directory;
static FFatResult result;
static uint32_t file_sector_length;
static std::string file_contents;

static FRESULT check_fs(FRESULT fresult) {
    if (fresult != FR_OK)
        throw std::runtime_error("Return != FR_OK (" + std::to_string(fresult) + ")");
    return fresult;
}

static FFatResult check_f(FFatResult r) {
    if (r != F_OK && r != F_MORE_DATA)
        throw std::runtime_error(f_error(r));
    return r;
}

static void assert(bool assertion) {
    if (!assertion)
        throw std::runtime_error("Assertion failed.");
}

std::vector<Test> prepare_tests()
{
    std::vector<Test> tests;

    //
    // DISK OPERATIONS
    //

    // region ...
    
    tests.emplace_back(
            "Load boot sector",
            
            [&](FFat32* ffat, Scenario const&) {
                check_f(f_fat32(ffat, F_BOOT, 0));
            },
            
            [&](uint8_t const* buffer, Scenario const&) {
                assert(buffer[0x0] == 0xeb);
                assert(*(uint16_t *) &buffer[510] == 0xaa55);
            }
    );

    tests.emplace_back(
            "Check disk space (pre-existing)",
            
            [](FFat32* ffat, Scenario const&) {
                check_f(f_fat32(ffat, F_FREE, 0));
            },
            
            [](uint8_t const* buffer, Scenario const& scenario) {
                uint32_t free_ = *(uint32_t *) buffer;
                DWORD found = scenario.get_free_space();
                assert(free_ == found);
            }
    );

    tests.emplace_back(
            "Check disk space (calculate)",

            [](FFat32* ffat, Scenario const&) {
                check_f(f_fat32(ffat, F_FSINFO_RECALC, 0));
                check_f(f_fat32(ffat, F_FREE, 0));
            },

            [](uint8_t const* buffer, Scenario const& scenario) {
                uint32_t free_ = *(uint32_t *) buffer;
                DWORD found = scenario.get_free_space();
                assert(abs((int) free_ - (int) found) < 4096);
            }
    );
    
    // endregion
    
    //
    // DIRECTORY OPERATIONS
    //
    
    // region ...

    tests.emplace_back(
            "Check directories",

            [&](FFat32* ffat, Scenario const&) {
                directory.clear();
                FFatResult r;
                ffat->buffer[0] = F_START_OVER;
                do {
                    r = check_f(f_fat32(ffat, F_DIR, 0));
                    add_files_to_dir_structure(ffat->buffer, directory);
                    ffat->buffer[0] = F_CONTINUE;
                } while (r == F_MORE_DATA);
            },

            [&](uint8_t const*, Scenario const&) {
                DIR dp;
                FILINFO filinfo;
                check_fs(f_opendir(&dp, "/"));
    
                for (;;) {
                    check_fs(f_readdir(&dp, &filinfo));
        
                    if (filinfo.fname[0] == '\0')
                        break;
        
                    assert(find_file_in_directory(&filinfo, directory));
                }
    
                check_fs(f_closedir(&dp));
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
                    check_f(result);
                    assert(check_for_files_in_directory(buffer, directory, { "FORTUNA", "WORLD" }));
                } else {
                    assert(result == F_PATH_NOT_FOUND);
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
                    check_f(result);
                    assert(check_for_files_in_directory(buffer, directory, { "FORTUNA", "WORLD" }));
                } else {
                    assert(result == F_PATH_NOT_FOUND);
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
                    check_f(result);
                    assert(check_for_files_in_directory(buffer, directory, { "HELLO.TXT" }));
                } else {
                    assert(result == F_PATH_NOT_FOUND);
                }
            }
    );

    tests.emplace_back(
            "Cd to root ('/') and dir",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/");
                result = f_fat32(ffat, F_CD, 0);
                ffat->buffer[0] = F_START_OVER;
                check_f(f_fat32(ffat, F_DIR, 0));
            },
            
            [&](uint8_t const* buffer, Scenario const& scenario) {
                std::vector<std::string> files;
                switch (scenario.disk_state) {
                    case Scenario::DiskState::Empty:
                        return;
                    case Scenario::DiskState::Complete:
                        files = { "HELLO", "FORTUNA.DAT", "TAGS.TXT" };
                        break;
                    case Scenario::DiskState::Files300:
                    case Scenario::DiskState::Files64:
                        for (size_t i = 1; i < 10; ++i) {
                            char buf[16];
                            sprintf(buf, "FILE%03zu.BIN", i);
                            files.emplace_back(buf);
                        }
                        break;
                }
                
                assert(check_for_files_in_directory(buffer, directory, files));
            }
    );

#if 0
    tests.emplace_back(
            "Create dir at root",

            [&](FFat32* f, Scenario const&) {
                strcpy((char *) f->buffer, "TEST");
                result = f_fat32(f, F_MKDIR, 1981);
            },

            [&](uint8_t const*, Scenario const&) {
                if (result != F_OK)
                    return false;
                
                DIR dp;
                FILINFO filinfo;
                FRESULT fresult = f_findfirst(&dp, &filinfo, "/", "TEST");
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
    
    tests.emplace_back(
            "Remove a existing directory (absolute)",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/FORTUNA");
                result = f_fat32(ffat, F_RMDIR, 0);
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                return f_stat("/HELLO/FORTUNA", &filinfo) == FR_NO_FILE;
            }
    );
    
    tests.emplace_back(
            "Remove a existing directory (relative)",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "HELLO");
                result = f_fat32(ffat, F_CD, 0);
                strcpy(reinterpret_cast<char*>(ffat->buffer), "FORTUNA");
                result = f_fat32(ffat, F_RMDIR, 0);
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                return f_stat("/HELLO/FORTUNA", &filinfo) == FR_NO_FILE;
            }
    );
    
    tests.emplace_back(
            "Forbid removal of a non-empty directory",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/WORLD");
                result = f_fat32(ffat, F_RMDIR, 0);
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
                    return true;
                
                return result == F_DIR_NOT_EMPTY;
            }
    );
    
    tests.emplace_back(
            "Create and remove a directory",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/TEMP");
                result = f_fat32(ffat, F_MKDIR, 0);
                if (result != F_OK)
                    throw std::runtime_error("Could not create directory.");
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/TEMP");
                result = f_fat32(ffat, F_RMDIR, 0);
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                return f_stat("/TEMP", &filinfo) == FR_NO_FILE;
            }
    );
    
    tests.emplace_back(
            "Create, remove and recreate directory",
            
            [&](FFat32* ffat, Scenario const&) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/TEMP");
                f_fat32(ffat, F_MKDIR, 0);
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/TEMP");
                result = f_fat32(ffat, F_RMDIR, 0);
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/TEMP");
                f_fat32(ffat, F_MKDIR, 0);
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                FILINFO filinfo;
                return f_stat("/TEMP", &filinfo) == FR_OK;
            }
    );
    
    // endregion
    
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
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
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
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
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
                if (scenario.disk_state != Scenario::DiskState::Complete && result == F_PATH_NOT_FOUND)
                    return true;
                
                if (result != F_OK)
                    return false;
                
                return (buffer[11] & 0x10) != 0;   // attr is directory
            }
    );
    
    // endregion
#endif
    
    //
    // FILE OPERATIONS
    //
    
    // region ...
    
    tests.emplace_back(
            "Open small existing file, read it and close it.",
            
            [&](FFat32* ffat, Scenario const& scenario) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "FORTUNA.DAT");
                
                FFatResult r = f_fat32(ffat, F_OPEN, 0);
                if (scenario.disk_state == Scenario::DiskState::Complete) {
                    check_f(r);
                } else {
                    assert(r == F_PATH_NOT_FOUND);
                    return;
                }
    
                uint8_t file_idx = ffat->buffer[0];
                
                FFatResult rr = f_fat32(ffat, F_READ, 0);  // file idx is already set on buffer
                assert(rr == F_OK);
                file_sector_length = ffat->reg.file_sector_length;
                file_contents = (const char *) ffat->buffer;
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_CLOSE, 0));
            },

            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return;
                
                assert(file_contents == "Hello world!");
                assert(file_sector_length == file_contents.size());
            }
    );
    
    tests.emplace_back(
            "Open large existing file, read it and close it.",
            
            [&](FFat32* ffat, Scenario const& scenario) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "TAGS.TXT");
                
                FFatResult r = f_fat32(ffat, F_OPEN, 0);
                if (scenario.disk_state == Scenario::DiskState::Complete) {
                    check_f(r);
                } else {
                    assert(r == F_PATH_NOT_FOUND);
                    return;
                }
                
                uint8_t file_idx = ffat->buffer[0];
                
                FFatResult rr;
                file_contents = "";
                do {
                    ffat->buffer[0] = file_idx;
                    rr = check_f(f_fat32(ffat, F_READ, 0));  // file idx is already set on buffer
                    file_sector_length = ffat->reg.file_sector_length;
                    file_contents.append((const char *) ffat->buffer, file_sector_length);
                } while (rr == F_MORE_DATA);
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_CLOSE, 0));
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                extern uint8_t _binary_test_TAGS_TXT_start[];
                extern uint8_t _binary_test_TAGS_TXT_end[];
                
                static std::string tags_txt((const char *) _binary_test_TAGS_TXT_start, _binary_test_TAGS_TXT_end - _binary_test_TAGS_TXT_start);
                
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return;
                
                assert(file_contents == tags_txt);
                assert(file_sector_length == tags_txt.length() % BYTES_PER_SECTOR);
            }
    );
    
    
    tests.emplace_back(
            "Open file with full path, read it and close it.",
            
            [&](FFat32* ffat, Scenario const& scenario) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "/HELLO/WORLD/HELLO.TXT");
                
                FFatResult r = f_fat32(ffat, F_OPEN, 0);
                if (scenario.disk_state == Scenario::DiskState::Complete) {
                    check_f(r);
                } else {
                    assert(r == F_PATH_NOT_FOUND);
                    return;
                }
                
                uint8_t file_idx = ffat->buffer[0];
                
                FFatResult rr = f_fat32(ffat, F_READ, 0);  // file idx is already set on buffer
                assert(rr == F_OK);
                file_sector_length = ffat->reg.file_sector_length;
                file_contents = (const char *) ffat->buffer;
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_CLOSE, 0));
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return;
                
                assert(file_contents == "Hello world!");
                assert(file_sector_length == file_contents.size());
            }
    );

    tests.emplace_back(
            "Seek while reading file",
            
            [&](FFat32* ffat, Scenario const& scenario) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "TAGS.TXT");
                
                FFatResult r = f_fat32(ffat, F_OPEN, 0);
                if (scenario.disk_state == Scenario::DiskState::Complete) {
                    check_f(r);
                } else {
                    assert(r == F_PATH_NOT_FOUND);
                    return;
                }
                
                uint8_t file_idx = ffat->buffer[0];
    
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_READ, 0)); // skip first sector
                
                ffat->buffer[0] = file_idx;
                *((uint32_t *) &ffat->buffer[1]) = 12;
                result = check_f(f_fat32(ffat, F_SEEK, 0));  // move forward 12 sectors
                file_sector_length = ffat->reg.file_sector_length;
    
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_READ, 0)); // read the 13th sector
                file_contents = (const char *) ffat->buffer;
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_CLOSE, 0));
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                extern uint8_t _binary_test_TAGS_TXT_start[];
                
                static std::string tags_txt(((const char *) _binary_test_TAGS_TXT_start) + (13 * BYTES_PER_SECTOR), BYTES_PER_SECTOR);
                
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return;
    
                assert(result == F_MORE_DATA);
                assert(file_contents == tags_txt);
                assert(file_sector_length == BYTES_PER_SECTOR);
            }
    );
    
    tests.emplace_back(
            "Seek to end",
            
            [&](FFat32* ffat, Scenario const& scenario) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "TAGS.TXT");
                
                FFatResult r = f_fat32(ffat, F_OPEN, 0);
                if (scenario.disk_state == Scenario::DiskState::Complete) {
                    check_f(r);
                } else {
                    assert(r == F_PATH_NOT_FOUND);
                    return;
                }
                
                uint8_t file_idx = ffat->buffer[0];
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_READ, 0)); // skip first sector
                
                ffat->buffer[0] = file_idx;
                *((uint32_t *) &ffat->buffer[1]) = (uint32_t) -1;
                result = check_f(f_fat32(ffat, F_SEEK, 0));  // move to end
                file_sector_length = ffat->reg.file_sector_length;
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_READ, 0)); // read last sector
                file_contents = (const char *) ffat->buffer;
                
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_CLOSE, 0));
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                extern uint8_t _binary_test_TAGS_TXT_start[];
                extern uint8_t _binary_test_TAGS_TXT_end[];
    
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return;
    
                size_t last_sector = (const char *) _binary_test_TAGS_TXT_end - (const char *) _binary_test_TAGS_TXT_start;
                last_sector /= BYTES_PER_SECTOR;
                static std::string tags_txt(((const char *) _binary_test_TAGS_TXT_start) + (last_sector * BYTES_PER_SECTOR));
                
                assert(result == F_OK);
                assert(file_contents == tags_txt);
                assert(file_sector_length == tags_txt.length());
            }
    );
    
    
    tests.emplace_back(
            "Seek past EOF",
            
            [&](FFat32* ffat, Scenario const& scenario) {
                strcpy(reinterpret_cast<char*>(ffat->buffer), "FORTUNA.DAT");
                
                FFatResult r = f_fat32(ffat, F_OPEN, 0);
                if (scenario.disk_state == Scenario::DiskState::Complete) {
                    check_f(r);
                } else {
                    assert(r == F_PATH_NOT_FOUND);
                    return;
                }
                
                uint8_t file_idx = ffat->buffer[0];
    
                ffat->buffer[0] = file_idx;
                *((uint32_t *) &ffat->buffer[1]) = 1;
                result = f_fat32(ffat, F_SEEK, 0);  // move forward 1 sector
                file_sector_length = ffat->reg.file_sector_length;
    
                ffat->buffer[0] = file_idx;
                check_f(f_fat32(ffat, F_CLOSE, 0));
            },
            
            [&](uint8_t const*, Scenario const& scenario) {
                if (scenario.disk_state != Scenario::DiskState::Complete)
                    return;
    
                assert(result == F_SEEK_PAST_EOF);
            }
    );
    
    // TODO - create file, write file
    
    // TODO - seek to end
    
    // endregion
    
    //
    // MOVE/REMOVE
    //
    
    // TODO - remove file
    
    // TODO - move file
    
    // TODO - move directory
    
    //
    // SPECIAL SITUATIONS
    //

#if 0
    tests.emplace_back(
            "Device is returning I/O errors",
            
            [&](FFat32* f, Scenario const&) {
                extern bool disk_ok;
                disk_ok = false;
                strcpy((char *) f->buffer, "TEST");
                result = f_fat32(f, F_MKDIR, 1981);
                disk_ok = true;
            },
            
            [&](uint8_t const*, Scenario const&) {
                return result == F_IO_ERROR;
            }
    );
#endif
    
    return tests;
}


