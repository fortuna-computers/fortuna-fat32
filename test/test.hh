#ifndef FORTUNA_FAT32_TEST_HH
#define FORTUNA_FAT32_TEST_HH

#include "../src/ffat32.h"
#include "scenario.hh"

#include <functional>
#include <string>

class Test {
public:
    Test(std::string const& name,
         std::function<void(FFat32Def*, Scenario const&)> execute,
         std::function<bool(uint8_t const*, Scenario const&)> verify)
            : name(name), execute(execute), verify(verify) {}
            
    const uint16_t number = counter++;
    const std::string name;
    const std::function<void(FFat32Def*, Scenario const&)> execute;
    const std::function<bool(uint8_t const*, Scenario const&)> verify;
    
    static uint16_t counter;
};

#endif
