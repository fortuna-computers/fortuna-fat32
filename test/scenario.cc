#include "scenario.hh"

#include <cmath>
#include <iostream>
#include <iomanip>

#include "brotli/decode.h"

uint8_t Scenario::image_[512 * 1024 * 1024] = { 0 };

std::vector<Scenario> Scenario::all_scenarios()
{
    std::vector<Scenario> scenarios;
    
    size_t i = 0;
    
    scenarios.emplace_back(i++, 1, DiskState::Empty, 256, 4);
    scenarios.emplace_back(i++, 1, DiskState::FilesInRoot, 256, 4);
    scenarios.emplace_back(i++, 1, DiskState::Complete, 256, 4);
    
    scenarios.emplace_back(i++, 0, DiskState::Complete, 256, 4);
    scenarios.emplace_back(i++, 2, DiskState::Complete, 256, 4);
    
    scenarios.emplace_back(i++, 1, DiskState::Complete, 64, 1);
    scenarios.emplace_back(i++, 1, DiskState::Complete, 512, 8);
    
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
    std::cout << "     L - large (512 MB)\n";
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
            case 512: std::cout << 'L'; break;
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
            std::cout << "    sleep 0.1 && \\\n";
            std::cout << "    kpartx -d " << filename << " && \\\n";
            std::cout << "    losetup && \\\n";
        }
        
        // compress image file
        std::cout << "    brotli -j1 " << filename << " && \\\n";
        
        // C-ify image file
        // std::cout << "    xxd -i " << filename << ".br > test/imghdr/" << i << ".h && \\\n";
        // std::cout << "    sed -i '1s/^/static const /' test/imghdr/" << i << ".h && \\\n";
        // std::cout << "    echo '\nstatic const size_t __" << i << "_original = " << disk_size * 1024 * 1024 << ";' >> test/imghdr/" << i << ".h && \\\n";
        
        // remove image file
        // std::cout << "    rm " << filename << " " << filename << ".br && \\\n";
        std::cout << "    mv " << filename << ".br test/imghdr/ && \\\n";
        std::cout << "    sync .\n";
        
        std::cout << "\n\n";
        ++i;
    }
    
    std::cout << "rmdir mnt\n";
}

void Scenario::decompress_image() const
{
    size_t file_size;
    uint8_t const* compressed_data = link_to_compressed(&file_size);
    
    size_t decoded_size = sizeof image_;
    
    BrotliDecoderState* state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (BrotliDecoderDecompress(file_size, compressed_data, &decoded_size, image_) != BROTLI_DECODER_RESULT_SUCCESS)
        abort();
    BrotliDecoderDestroyInstance(state);
}

uint8_t const* Scenario::link_to_compressed(size_t* file_size) const
{
    extern uint8_t _binary_test_imghdr_0_img_br_start[];
    extern uint8_t _binary_test_imghdr_0_img_br_size;
    extern uint8_t _binary_test_imghdr_1_img_br_start[];
    extern uint8_t _binary_test_imghdr_1_img_br_size;
    extern uint8_t _binary_test_imghdr_2_img_br_start[];
    extern uint8_t _binary_test_imghdr_2_img_br_size;
    extern uint8_t _binary_test_imghdr_3_img_br_start[];
    extern uint8_t _binary_test_imghdr_3_img_br_size;
    extern uint8_t _binary_test_imghdr_4_img_br_start[];
    extern uint8_t _binary_test_imghdr_4_img_br_size;
    extern uint8_t _binary_test_imghdr_5_img_br_start[];
    extern uint8_t _binary_test_imghdr_5_img_br_size;
    extern uint8_t _binary_test_imghdr_6_img_br_start[];
    extern uint8_t _binary_test_imghdr_6_img_br_size;
    
    switch (number) {
        case 0: *file_size = (size_t) &_binary_test_imghdr_0_img_br_size; return _binary_test_imghdr_0_img_br_start;
        case 1: *file_size = (size_t) &_binary_test_imghdr_1_img_br_size; return _binary_test_imghdr_1_img_br_start;
        case 2: *file_size = (size_t) &_binary_test_imghdr_2_img_br_size; return _binary_test_imghdr_2_img_br_start;
        case 3: *file_size = (size_t) &_binary_test_imghdr_3_img_br_size; return _binary_test_imghdr_3_img_br_start;
        case 4: *file_size = (size_t) &_binary_test_imghdr_4_img_br_size; return _binary_test_imghdr_4_img_br_start;
        case 5: *file_size = (size_t) &_binary_test_imghdr_5_img_br_size; return _binary_test_imghdr_5_img_br_start;
        case 6: *file_size = (size_t) &_binary_test_imghdr_6_img_br_size; return _binary_test_imghdr_6_img_br_start;
        default: abort();
    }
}

