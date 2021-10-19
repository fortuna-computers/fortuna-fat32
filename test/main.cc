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

extern std::vector<Test> prepare_tests();
uint8_t buffer[512];

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
        std::cout << chr++;
    std::cout << "\n";
    std::cout << std::string(80, '-') << "\n";
}


static void run_tests(Scenario const& scenario, std::vector<Test> const& tests, FFat32* ffat, uint8_t const* buffer)
{
    std::cout << std::left << std::setw(43) << scenario.name;
    
    for (Test const& test: tests) {
    
        scenario.prepare_scenario();
        
        FFatResult r = f_fat32(ffat, F_INIT, 0);
        if (r != F_OK) {
            std::cout << " Error initializing FAT32 volume: " << r << "\n";
            exit(EXIT_FAILURE);
        }
        
        test.execute(ffat, scenario);
        scenario.remount();
        if (test.verify(buffer, scenario)) {
            std::cout << GRN "\u2713" RST;
        } else {
            std::cout << RED "X" RST;
            // exit(1);
        }
        std::cout.flush();
    
        scenario.end_scenario();
    }
    
    std::cout << "\n";
}


int main()
{
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
        .reg = {},
    };
    
    std::vector<Test> tests = prepare_tests();
    print_test_descriptions(tests);
    print_headers(tests);
    
    auto scenarios = Scenario::all_scenarios();
    for (Scenario const& scenario: scenarios)
        run_tests(scenario, tests, &ffat, buffer);
}

