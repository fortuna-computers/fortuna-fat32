#include "helper.hh"

#include <cstring>
#include <algorithm>

std::string build_name(const char* name)
{
    std::string filename = std::string(name, 8);
    while (filename.back() == ' ')
        filename.pop_back();
    
    std::string extension = std::string(&name[8]);
    while (extension.back() == ' ')
        extension.pop_back();
    
    if (extension.empty())
        return filename;
    else
        return filename + "." + extension;
};

void add_files_to_dir_structure(uint8_t const* buffer, std::vector<File>& directory)
{
    for (size_t i = 0; i < 512 / 32; ++i) {
        if (buffer[i * 32] == 0)
            break;
        char buf[12] = { 0 };
        strncpy(buf, (char*) &buffer[i * 32], 11);
        std::string name = build_name(buf);
        bool is_dir = buffer[i * 32 + 0xb];
        uint32_t size = *(uint32_t *) &buffer[i * 32 + 0x1c];
        directory.emplace_back(name, is_dir, size);
    }
};

bool find_file_in_directory(FILINFO const* filinfo, std::vector<File>& directory)
{
    auto it = std::find_if(directory.begin(), directory.end(),
                           [&](File const& file) { return file.name == std::string(filinfo->fname); });
    if (it == directory.end())
        return false;
    
    return it->size == filinfo->fsize;
};

bool check_for_files_in_directory(uint8_t const* buffer, std::vector<File>& directory, std::vector<std::string> const& filenames)
{
    add_files_to_dir_structure(buffer, directory);
    for (auto const& filename: filenames)
        if (std::find_if(directory.begin(), directory.end(),
                         [&filename](File const& file) { return file.name == filename; }) == directory.end())
            return false;
    return true;
};
