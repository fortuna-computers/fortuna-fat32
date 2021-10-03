#include "scenario.hh"

#include <cmath>
#include <iostream>
#include <iomanip>

std::vector<Scenario> Scenario::all_scenarios()
{
    std::vector<Scenario> scenarios;
    
    for (auto p: v_partitions) {
        for (auto st: v_disk_states) {
            for (auto sz: v_disk_size) {
                for (auto spc: v_sectors_per_cluster) {
                    scenarios.emplace_back(p, st, sz, spc);
                }
            }
        }
    }
    
    return scenarios;
}

void Scenario::print_legend()
{
   std::cout << "Legend:\n";
   std::cout << "  * Number of partitions\n";
   std::cout << "  * Disk state:\n";
   std::cout << "     E - Empty disk\n";
   std::cout << "     R - Files in root\n";
   std::cout << "     C - Complete disk with subdirectories\n";
   std::cout << "  * Disk size (bits)\n";
   std::cout << "  * Sectors per cluster (bits)\n";
   std::cout << "\n";
}

void Scenario::print_scenarios(uint16_t spaces_at_start)
{
    auto scenarios = all_scenarios();
    
    std::cout << std::left << std::setw(spaces_at_start) << "Partitions";
    for (auto const& scenario: scenarios)
        std::cout << std::to_string(scenario.partitions);
    std::cout << '\n';
    
    std::cout << std::left << std::setw(spaces_at_start) << "Disk state";
    for (auto const& scenario: scenarios) {
        switch (scenario.disk_state) {
            case DiskState::Empty:       std::cout << 'E'; break;
            case DiskState::FilesInRoot: std::cout << 'R'; break;
            case DiskState::Complete:    std::cout << 'C'; break;
        }
    }
    std::cout << '\n';
    
    std::cout << std::left << std::setw(spaces_at_start) << "Disk size";
    for (auto const& scenario: scenarios)
        std::cout << std::to_string((int) std::log2(scenario.disk_size - 1) + 1);
    std::cout << '\n';
    
    std::cout << std::left << std::setw(spaces_at_start) << "Sectors per cluster";
    for (auto const& scenario: scenarios)
        if (scenario.sectors_per_cluster == 1)
            std::cout << '1';
        else
            std::cout << std::to_string((int) std::log2(scenario.sectors_per_cluster - 1) + 1);
    std::cout << '\n';
    
    std::cout << std::string(spaces_at_start + scenarios.size(), '-') << "\n";
}
