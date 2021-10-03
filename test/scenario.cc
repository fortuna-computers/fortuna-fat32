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

void Scenario::generate_disk_creators()
{
    auto scenarios = all_scenarios();
    
    std::cout << "#!/bin/sh\n\n";
    
    size_t i = 0;
    for (auto const& scenario: scenarios) {
        const std::string filename = std::to_string(i) + ".img";
        
        // create image
        std::cout << "dd if=/dev/zero of=" + filename + " bs=1M count=" << std::to_string(scenario.disk_size) << " && \\\n";
        
        // create partitions
        std::string device_name = filename;
        
        if (scenario.partitions != 0) {
            switch (scenario.partitions) {
                case 1:
                    std::cout << "    parted -a optimal " + filename + " mktable msdos mkpart primary fat32 1 '100%' && \\\n";
                    break;
                case 2:
                    std::cout << "    parted -a optimal " + filename + " mktable msdos mkpart primary fat32 1 '50%' mkpart primary fat32 '50%' '100%' \\\n";
                    break;
            }
        
            device_name = "/dev/loop0p0";
    
            // create loopback devices
            std::cout << "    kpartx -av " + filename + " && \\\n";
        }
            
        // format partition
        std::cout << "    mkfs.fat -F 32 -s " + std::to_string(scenario.sectors_per_cluster) + " " + device_name + " && \\\n";
        
        // mount partition
        std::cout << "    mount " + device_name + " mnt && \\\n";
        
        // copy files
        
        // dismount partition
        std::cout << "    umount mnt && \\\n";
    
        if (scenario.partitions != 0) {
            // remove loopback devices
            std::cout << "    kpartx -d " + filename + " && \\\n";
        }
        
        // compress image file
        std::cout << "    bzip2 -i " + filename + " && \\\n";
        
        // C-ify image file
        std::cout << "    xxd -i " + filename + ".bz2 > " + std::to_string(i) + ".h && \\\n";
        
        // remove image file
        std::cout << "    rm " + filename + ".bz2";
        
        std::cout << "\n\n";
        break;
        ++i;
    }
}
