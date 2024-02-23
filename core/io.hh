#ifndef RAYBASE_IO_HH
#define RAYBASE_IO_HH
#include <filesystem>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace rb
{

void write_binary_file(const std::string& path, const uint8_t* data, size_t size);
void write_text_file(const std::string& path, const std::string& content);

#if RAYBASE_BIG_ENDIAN
template<typename I>
I adjust_endian(I v)
{
    uint8_t* d = (uint8_t*)&v;
    uint8_t o[sizeof(v)] = {0};
    for(unsigned i = 0; i < sizeof(v); ++i)
        o[sizeof(v)-1-i] = d[i];
    return *(I*)o;
}
#else
template<typename I>
I adjust_endian(I v) { return v; }
#endif

inline std::string to_string(const fs::path& p) { return p.string(); }

std::string read_text_file(const std::string& path);

#ifdef RAYBASE_HAS_SDL2
fs::path get_writable_path();
std::vector<fs::path> get_readonly_paths();
std::string get_readonly_path(const std::string& file);
#endif


uint32_t read_u32(const uint8_t* data, size_t size, size_t& offset);
float read_float(const uint8_t* data, size_t size, size_t& offset);
std::string read_str(
    const uint8_t* data, size_t size, size_t& offset, size_t alignment
);

}

#endif
