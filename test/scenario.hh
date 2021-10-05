#ifndef FORTUNA_FAT32_SCENARIO_HH
#define FORTUNA_FAT32_SCENARIO_HH

#include <cstdint>
#include <memory>
#include <vector>

class Scenario {
public:
    enum class DiskState { Empty, FilesInRoot, Complete };
    
    Scenario(uint8_t number, std::string const& name, uint8_t partitions, DiskState disk_state, uint16_t disk_size, uint16_t sectors_per_cluster)
             : number(number), name(name), partitions(partitions), disk_state(disk_state), disk_size(disk_size),
               sectors_per_cluster(sectors_per_cluster) {}
               
    const uint8_t     number;
    const std::string name;
    const uint8_t     partitions;
    const DiskState   disk_state;
    const uint16_t    disk_size;
    const uint16_t    sectors_per_cluster;
    
    static std::vector<Scenario> all_scenarios();
    
    static void generate_disk_creators();
    
    static uint8_t* image() { return image_; }
    
    void decompress_image() const;
    
private:
    static constexpr uint8_t   v_partitions[] = { 0, 1, 2 };
    static constexpr DiskState v_disk_states[] = { DiskState::Empty, DiskState::FilesInRoot, DiskState::Complete };
    static constexpr uint16_t  v_disk_size[] = { 64, 256, 1024 };
    static constexpr uint8_t   v_sectors_per_cluster[] = { 1, 8, 32 };
    
    uint8_t const* link_to_compressed(size_t* file_size) const;
    
    static uint8_t image_[512 * 1024 * 1024];
};

#endif
