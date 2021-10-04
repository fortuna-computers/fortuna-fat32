#include "scenario.hh"

#include <cstdlib>
#include <string>

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "-g") {
        Scenario::generate_disk_creators();
        return EXIT_SUCCESS;
    }
    
    auto scenarios = Scenario::all_scenarios();
    
    Scenario::all_scenarios().at(0).decompress_image();
    
    
}