#ifndef HELPER_HH_
#define HELPER_HH_

#include <string>
#include <vector>

#include "ff/ff.h"

struct File {
    std::string name;
    uint8_t     attr;
    uint32_t    size;
    
    File(std::string const& name, uint8_t attr, uint32_t size) : name(name), attr(attr), size(size) {}
};

std::string build_name(const char* name);
void        add_files_to_dir_structure(uint8_t const* buffer, std::vector<File>& directory);
bool        find_file_in_directory(FILINFO const* filinfo, std::vector<File>& directory);
bool        check_for_files_in_directory(uint8_t const* buffer, std::vector<File> const& directory, std::vector<std::string> const& filenames);

#endif
