#ifndef FORTUNA_FAT32_SCENARIO_HH
#define FORTUNA_FAT32_SCENARIO_HH

#include <cstdint>
#include <vector>

class Scenario {
public:
    enum class DiskState { Empty, FilesInRoot, Complete };
    
    Scenario(uint8_t partitions, DiskState disk_state, uint16_t disk_size, uint16_t sectors_per_cluster)
             : partitions(partitions), disk_state(disk_state), disk_size(disk_size),
               sectors_per_cluster(sectors_per_cluster) {}
    
    const uint8_t   partitions;
    const DiskState disk_state;
    const uint16_t  disk_size;
    const uint16_t  sectors_per_cluster;
    
    static std::vector<Scenario> all_scenarios();
    
    static void print_legend();
    static void print_scenarios(uint16_t spaces_at_start);
    static void generate_disk_creators();

private:
    static constexpr uint8_t   v_partitions[] = { 0, 1, 2 };
    static constexpr DiskState v_disk_states[] = { DiskState::Empty, DiskState::FilesInRoot, DiskState::Complete };
    static constexpr uint16_t  v_disk_size[] = { 64, 256, 1024 };
    static constexpr uint8_t   v_sectors_per_cluster[] = { 1, 8, 64 };
};

#endif
