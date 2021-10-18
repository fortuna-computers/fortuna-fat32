#ifndef FORTUNA_FAT32_SCENARIO_HH
#define FORTUNA_FAT32_SCENARIO_HH

#include <cstdint>
#include <memory>
#include <vector>
#include "ff/ff.h"

class Scenario {
public:
    enum class DiskState { Empty, FilesInRoot, Complete, Files300, Files512 };
    
    Scenario(std::string const& name, uint8_t partitions, DiskState disk_state, uint16_t disk_size, uint16_t sectors_per_cluster)
             : name(name), partitions(partitions), disk_state(disk_state), disk_size(disk_size),
               sectors_per_cluster(sectors_per_cluster) {}
               
    const std::string name;
    const uint8_t     partitions;
    const DiskState   disk_state;
    const uint16_t    disk_size;
    const uint16_t    sectors_per_cluster;
    
    static std::vector<Scenario> all_scenarios();
    
    static uint8_t* image() { return image_; }
    
    void prepare_scenario() const;
    void end_scenario() const;
    
    void remount() const;
    
    void store_image_in_disk(std::string const& filename) const;
    
    DWORD get_free_space() const;

private:
    static FATFS fatfs;
    
    static uint8_t image_[512 * 1024 * 1024];
    
    void clear_disk()     const;
    void partition_disk() const;
    void format_disk()    const;
    
    void add_files_in_root() const;
    void add_complete_files() const;
    void add_many_files(size_t count) const;
    
    void R(FRESULT fresult) const;
};

#endif
