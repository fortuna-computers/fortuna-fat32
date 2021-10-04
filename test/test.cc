#include "scenario.hh"

#include <cstdlib>
#include <string>

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "-g") {
        Scenario::generate_disk_creators();
        return EXIT_SUCCESS;
    }
    
    Scenario::print_legend();
    Scenario::print_scenarios(25);
}