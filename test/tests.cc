#include <vector>
#include <cstring>
#include "test.hh"

std::vector<Test> prepare_tests()
{
    std::vector<Test> tests;
    
    tests.emplace_back(
            "Check label",
            
            [](FFat32Def* ffat, Scenario const&) {
                f_fat32(ffat, F_LABEL);
            },
            
            [](uint8_t const* buffer, Scenario const&, FATFS*) {
                char label[50];
                if (f_getlabel("", label, nullptr) != FR_OK)
                    abort();
                return strcmp((char const*) buffer, label) == 0;
            }
    );
    
    tests.emplace_back(
            "Check disk space (pre-existing)",
            
            [](FFat32Def* ffat, Scenario const&) {
                f_fat32(ffat, F_FREE);
            },
            
            [](uint8_t const* buffer, Scenario const&, FATFS* fatfs) {
                uint32_t free = *(uint32_t *) buffer;
                DWORD found;
                if (f_getfree("", &found, &fatfs) != FR_OK)
                    abort();
                return free == found;
            }
    );
    
    tests.emplace_back(
            "Check disk space (calculate)",

            [](FFat32Def* ffat, Scenario const&) {
                f_fat32(ffat, F_FREE_RECALCUATE);
            },

            [](uint8_t const* buffer, Scenario const& scenario, FATFS* fatfs) {
                uint32_t pos = 0x3e8;
                
                uint32_t free = *(uint32_t *) buffer;
                DWORD found;
                if (f_getfree("", &found, &fatfs) != FR_OK)
                    abort();
                return free == found;
            }
    );
    
    return tests;
}


