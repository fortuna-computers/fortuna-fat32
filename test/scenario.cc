#include "scenario.hh"

#include <cmath>
#include <iostream>
#include <iomanip>

std::vector<Scenario> Scenario::all_scenarios()
{
    std::vector<Scenario> scenarios;
    
    uint8_t number = 0;
    for (auto p: v_partitions) {
        for (auto st: v_disk_states) {
            for (auto sz: v_disk_size) {
                for (auto spc: v_sectors_per_cluster) {
                    uint32_t total_clusters = (sz * 1024 * 1024) / (spc * 512);
                    if (total_clusters < 64 * 1024)
                        continue;
                    scenarios.emplace_back(number++, p, st, sz, spc);
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
    std::cout << "     S - small (64 MB)\n";
    std::cout << "     M - medium (256 MB)\n";
    std::cout << "     L - large (1 GB)\n";
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
    for (auto const& scenario: scenarios) {
        switch (scenario.disk_size) {
            case 64:   std::cout << 'S'; break;
            case 256:  std::cout << 'M'; break;
            case 1024: std::cout << 'L'; break;
        }
    }
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
    
    std::cout << "#!/bin/sh -xe\n\n";
    std::cout << "mkdir -p mnt test/imghdr\n\n";
    
    size_t i = 0;
    for (auto const& scenario: scenarios) {
        const std::string filename = std::to_string(i) + ".img";
        
        std::cout << "echo 'Generating scenario " << std::to_string(i) << "...'\n";
        
        // create image
        size_t disk_size = scenario.disk_size;
        std::cout << "dd if=/dev/zero of=" << filename << " bs=1M count=" << disk_size << " status=none && \\\n";
        
        // create partitions
        std::string device_name = filename;
        
        if (scenario.partitions != 0) {
            switch (scenario.partitions) {
                case 1:
                    std::cout << "    parted -a optimal " << filename << " mktable msdos mkpart primary fat32 1 '100%' && \\\n";
                    break;
                case 2:
                    std::cout << "    parted -a optimal " + filename + " mktable msdos mkpart primary fat32 1 '50%' mkpart primary fat32 '50%' '100%' && \\\n";
                    break;
            }
        
            device_name = "/dev/mapper/loop0p1";
    
            // create loopback devices
            std::cout << "    kpartx -av " << filename << " && \\\n";
        }
            
        // format partition
        std::cout << "    mkfs.fat -F 32 -n FORTUNA -s " << scenario.sectors_per_cluster << " " << device_name << " > /dev/null && \\\n";
        
        if (scenario.disk_state != DiskState::Empty) {
            // mount partition
            std::cout << "    mount -o sync,mand,loud " << device_name << " mnt && \\\n";
            
            // copy files
            std::string origin = scenario.disk_state == DiskState::FilesInRoot ? "rootdir" : "multidir";
            std::cout << "    cp -R test/" << origin << "/* mnt/ && \\\n";
            std::cout << "    sync mnt && \\\n";
            
            // dismount partition
            std::cout << "    umount -f mnt && \\\n";
        }
    
        if (scenario.partitions != 0) {
            // remove loopback devices
            std::cout << "    sleep 1 && \\\n";
            std::cout << "    kpartx -d " << filename << " && \\\n";
            std::cout << "    losetup && \\\n";
        }
        
        // compress image file
        std::cout << "    bzip2 -1 " << filename << " && \\\n";
        
        // C-ify image file
        std::cout << "    xxd -i " << filename << ".bz2 > test/imghdr/" << i << ".h && \\\n";
        std::cout << "    sed -i '1s/^/static const /' test/imghdr/" << i << ".h && \\\n";
        
        // remove image file
        std::cout << "    rm " << filename << ".bz2 && \\\n";
        std::cout << "    sync .\n";
        
        std::cout << "\n\n";
        ++i;
    }
    
    std::cout << "rmdir mnt\n";
}

std::unique_ptr<uint8_t[]> Scenario::allocate_and_decompress_image() const
{
    return std::unique_ptr<uint8_t[]>();
}

#include "imghdr/0.h"
#include "imghdr/1.h"
#include "imghdr/2.h"
#include "imghdr/3.h"
#include "imghdr/4.h"
#include "imghdr/5.h"
#include "imghdr/6.h"
#include "imghdr/7.h"
#include "imghdr/8.h"
#include "imghdr/9.h"
#include "imghdr/10.h"
#include "imghdr/11.h"
#include "imghdr/12.h"
#include "imghdr/13.h"
#include "imghdr/14.h"
#include "imghdr/15.h"
#include "imghdr/16.h"
#include "imghdr/17.h"
#include "imghdr/18.h"
#include "imghdr/19.h"
#include "imghdr/20.h"
#include "imghdr/21.h"
#include "imghdr/22.h"
#include "imghdr/23.h"
#include "imghdr/24.h"
#include "imghdr/25.h"
#include "imghdr/26.h"
#include "imghdr/27.h"
#include "imghdr/28.h"
#include "imghdr/29.h"
#include "imghdr/30.h"
#include "imghdr/31.h"
#include "imghdr/32.h"
#include "imghdr/33.h"
#include "imghdr/34.h"
#include "imghdr/35.h"
#include "imghdr/36.h"
#include "imghdr/37.h"
#include "imghdr/38.h"
#include "imghdr/39.h"
#include "imghdr/40.h"
#include "imghdr/41.h"
#include "imghdr/42.h"
#include "imghdr/43.h"
#include "imghdr/44.h"
#include "imghdr/45.h"
#include "imghdr/46.h"
#include "imghdr/47.h"
#include "imghdr/48.h"
#include "imghdr/49.h"
#include "imghdr/50.h"
#include "imghdr/51.h"
#include "imghdr/52.h"
#include "imghdr/53.h"
uint8_t* Scenario::link_to_bzip2(uint32_t* size) const
{
    switch (number) {
        case 0: *size = __0_img_bz2_len; return __0_img_bz2;
        case 1: *size = __1_img_bz2_len; return __1_img_bz2;
        case 2: *size = __2_img_bz2_len; return __2_img_bz2;
        case 3: *size = __3_img_bz2_len; return __3_img_bz2;
        case 4: *size = __4_img_bz2_len; return __4_img_bz2;
        case 5: *size = __5_img_bz2_len; return __5_img_bz2;
        case 6: *size = __6_img_bz2_len; return __6_img_bz2;
        case 7: *size = __7_img_bz2_len; return __7_img_bz2;
        case 8: *size = __8_img_bz2_len; return __8_img_bz2;
        case 9: *size = __9_img_bz2_len; return __9_img_bz2;
        case 10: *size = __10_img_bz2_len; return __10_img_bz2;
        case 11: *size = __11_img_bz2_len; return __11_img_bz2;
        case 12: *size = __12_img_bz2_len; return __12_img_bz2;
        case 13: *size = __13_img_bz2_len; return __13_img_bz2;
        case 14: *size = __14_img_bz2_len; return __14_img_bz2;
        case 15: *size = __15_img_bz2_len; return __15_img_bz2;
        case 16: *size = __16_img_bz2_len; return __16_img_bz2;
        case 17: *size = __17_img_bz2_len; return __17_img_bz2;
        case 18: *size = __18_img_bz2_len; return __18_img_bz2;
        case 19: *size = __19_img_bz2_len; return __19_img_bz2;
        case 20: *size = __20_img_bz2_len; return __20_img_bz2;
        case 21: *size = __21_img_bz2_len; return __21_img_bz2;
        case 22: *size = __22_img_bz2_len; return __22_img_bz2;
        case 23: *size = __23_img_bz2_len; return __23_img_bz2;
        case 24: *size = __24_img_bz2_len; return __24_img_bz2;
        case 25: *size = __25_img_bz2_len; return __25_img_bz2;
        case 26: *size = __26_img_bz2_len; return __26_img_bz2;
        case 27: *size = __27_img_bz2_len; return __27_img_bz2;
        case 28: *size = __28_img_bz2_len; return __28_img_bz2;
        case 29: *size = __29_img_bz2_len; return __29_img_bz2;
        case 30: *size = __30_img_bz2_len; return __30_img_bz2;
        case 31: *size = __31_img_bz2_len; return __31_img_bz2;
        case 32: *size = __32_img_bz2_len; return __32_img_bz2;
        case 33: *size = __33_img_bz2_len; return __33_img_bz2;
        case 34: *size = __34_img_bz2_len; return __34_img_bz2;
        case 35: *size = __35_img_bz2_len; return __35_img_bz2;
        case 36: *size = __36_img_bz2_len; return __36_img_bz2;
        case 37: *size = __37_img_bz2_len; return __37_img_bz2;
        case 38: *size = __38_img_bz2_len; return __38_img_bz2;
        case 39: *size = __39_img_bz2_len; return __39_img_bz2;
        case 40: *size = __40_img_bz2_len; return __40_img_bz2;
        case 41: *size = __41_img_bz2_len; return __41_img_bz2;
        case 42: *size = __42_img_bz2_len; return __42_img_bz2;
        case 43: *size = __43_img_bz2_len; return __43_img_bz2;
        case 44: *size = __44_img_bz2_len; return __44_img_bz2;
        case 45: *size = __45_img_bz2_len; return __45_img_bz2;
        case 46: *size = __46_img_bz2_len; return __46_img_bz2;
        case 47: *size = __47_img_bz2_len; return __47_img_bz2;
        case 48: *size = __48_img_bz2_len; return __48_img_bz2;
        case 49: *size = __49_img_bz2_len; return __49_img_bz2;
        case 50: *size = __50_img_bz2_len; return __50_img_bz2;
        case 51: *size = __51_img_bz2_len; return __51_img_bz2;
        case 52: *size = __52_img_bz2_len; return __52_img_bz2;
        case 53: *size = __53_img_bz2_len; return __53_img_bz2;
        default: abort();
    }
}