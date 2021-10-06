#include "scenario.hh"
#include "test.hh"

#include "ff/ff.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

std::vector<Test> prepare_tests()
{
    std::vector<Test> tests;
    
    tests.emplace_back(
            "Check label",
            
            [](FFat32Def* ffat, Scenario const&) {
                f_fat32(ffat, F_LABEL);
            },
            
            [](uint8_t const* buffer, Scenario const&) {
                char label[50];
                f_getlabel(nullptr, label, nullptr);
                return strcmp((char const*) buffer, label) == 0;
            }
    );
    
    return tests;
}

static void run_tests(Scenario const& scenario, std::vector<Test> const& tests, FFat32Def* ffat, uint8_t const* buffer)
{
    std::cout << std::left << std::setw(43) << scenario.name;
    
    scenario.decompress_image();
    Scenario::backup_image();
    
    size_t i = 0;
    for (Test const& test: tests) {
        if (i++ == 0)
            Scenario::restore_image_backup();
    
        f_fat32(ffat, F_INIT);
        
        test.execute(ffat, scenario);
        if (test.verify(buffer, scenario)) {
            std::cout << " . ";
        } else {
            std::cout << " X ";
            // exit(1);
        }
    }
    
    std::cout << "\n";
}


int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "-g") {
        Scenario::generate_disk_creators();
        return EXIT_SUCCESS;
    }
    
    uint8_t buffer[512];
    
    FFat32Def ffat {
        buffer,
        Scenario::image(),
        [](uint32_t block, uint8_t const* buffer, void* data) {
            memcpy(&((char*) data)[block * 512], buffer, 512);
            return true;
        },
        [](uint32_t block, uint8_t* buffer, void* data) {
            memcpy(buffer, &((char const*) data)[block * 512], 512);
            return true;
        }
    };
    
    std::vector<Test> tests = prepare_tests();
    
    for (Test const& test: tests)
        std::cout << test.number << " - " << test.name << "\n";
    
    std::cout << std::string(43, ' ');
    for (Test const& test: tests)
        std::cout << std::setw(2) << test.number << ' ';
    std::cout << "\n";
    std::cout << std::string(80, '-') << "\n";
    
    for (Scenario const& scenario: Scenario::all_scenarios())
        run_tests(scenario, tests, &ffat, buffer);
}

