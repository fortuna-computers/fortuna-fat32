#include "scenario.hh"
#include "test.hh"

#include "ff/ff.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#define GRN "\e[0;32m"
#define RED "\e[0;31m"
#define RST "\e[0m"

extern "C" {
    extern uint8_t* diskio_image;
}

extern std::vector<Test> prepare_tests();

static void print_test_descriptions(std::vector<Test> const& tests)
{
    char chr = 'A';
    for (Test const& test: tests)
        std::cout << chr++ << " - " << test.name << "\n";
}


static void print_headers(std::vector<Test> const& tests)
{
    char chr = 'A';
    std::cout << std::string(43, ' ');
    for ([[maybe_unused]] Test const& _: tests)
        std::cout << chr++ << ' ';
    std::cout << "\n";
    std::cout << std::string(80, '-') << "\n";
}


static void run_tests(Scenario const& scenario, std::vector<Test> const& tests, FFat32* ffat, uint8_t const* buffer)
{
    std::cout << std::left << std::setw(43) << scenario.name;
    
    scenario.decompress_image();
    Scenario::backup_image();
    
    size_t i = 0;
    for (Test const& test: tests) {
        if (i++ > 0)
            Scenario::restore_image_backup();
        diskio_image = Scenario::image();
        
        FFatResult r = f_fat32(ffat, F_INIT);
        if (r != F_OK) {
            std::cout << " Error initializing FAT32 volume: " << r << "\n";
            exit(EXIT_FAILURE);
        }
        
        FATFS fatfs;
        f_mount(&fatfs, "", 0);
        
        test.execute(ffat, scenario);
        if (test.verify(buffer, scenario, &fatfs)) {
            std::cout << GRN "\u2713 " RST;
        } else {
            std::cout << RED "X " RST;
            // exit(1);
        }
        std::cout.flush();
    
        f_mount(nullptr, "", 0);
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
    
    FFat32 ffat {
        .buffer = buffer,
        .data = Scenario::image(),
        .write = [](uint32_t block, uint8_t const* buffer, void* data) {
            memcpy(&((char*) data)[block * 512], buffer, 512);
            return true;
        },
        .read = [](uint32_t block, uint8_t* buffer, void* data) {
            memcpy(buffer, &((char const*) data)[block * 512], 512);
            return true;
        },
        .reg = {
                .last_operation_result = F_OK,
                .state_next_cluster = 0,
                .state_next_sector = 0,
        }
    };
    
    std::vector<Test> tests = prepare_tests();
    print_test_descriptions(tests);
    print_headers(tests);
    
    for (Scenario const& scenario: Scenario::all_scenarios())
        run_tests(scenario, tests, &ffat, buffer);
    // run_tests(Scenario::all_scenarios().at(7), tests, &ffat, buffer);
}

