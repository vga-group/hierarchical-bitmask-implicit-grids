#include "io.hh"
#include "error.hh"
#ifdef RAYBASE_HAS_SDL2
#include <SDL.h>
#endif
#include <cstdio>
#include <algorithm>
#include <set>

namespace rb
{

void write_binary_file(
    const std::string& path,
    const uint8_t* data,
    size_t size
){
    FILE* f = fopen(path.c_str(), "wb");

    if(!f) RB_PANIC("Unable to open ", path);

    if(fwrite(data, 1, size, f) != size)
    {
        fclose(f);
        RB_PANIC("Unable to write ", path);
    }
    fclose(f);
}

void write_text_file(const std::string& path, const std::string& content)
{
    write_binary_file(path, (uint8_t*)content.c_str(), content.size());
}


std::string read_text_file(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");

    if(!f) RB_PANIC("Unable to open ", path);

    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = new char[sz];
    if(fread(data, 1, sz, f) != sz)
    {
        fclose(f);
        delete [] data;
        RB_PANIC("Unable to read ", path);
    }
    fclose(f);
    std::string ret(data, sz);

    delete [] data;
    return ret;
}

#ifdef RAYBASE_HAS_SDL2
fs::path get_writable_path()
{
    static bool has_path = false;
    static fs::path path;
    if(!has_path)
    {
        char* path_str = SDL_GetPrefPath("jji.fi", "RayBoy");
        path = path_str;
        SDL_free(path_str);
        path = path.make_preferred();
        has_path = true;
    }

    return path;
}

std::vector<fs::path> get_readonly_paths()
{
    static bool has_path = false;
    static fs::path path;
    if(!has_path)
    {
        char* path_str = SDL_GetBasePath();
        path = path_str;
        SDL_free(path_str);
        path = path.make_preferred();
        has_path = true;
    }

    std::vector<fs::path> paths;
    paths.push_back(path);

    paths.push_back(fs::current_path());
#ifdef DATA_DIRECTORY
    paths.push_back(fs::path{DATA_DIRECTORY});
#endif

    return paths;
}

std::string get_readonly_path(const std::string& file)
{
    for(const fs::path& path: get_readonly_paths())
    {
        if(fs::exists(path/file))
            return (path/file).string();
    }
    return file;
}
#endif

uint32_t read_u32(const uint8_t* data, size_t size, size_t& offset)
{
    if(offset + 4 > size) RB_PANIC("u32 read out of range");

    uint32_t val =
        ((uint32_t)data[offset + 0]) |
        ((uint32_t)data[offset + 1]<<8) |
        ((uint32_t)data[offset + 2]<<16) |
        ((uint32_t)data[offset + 3]<<24);

    offset += 4;
    return val;
}

float read_float(const uint8_t* data, size_t size, size_t& offset)
{
    if(offset + 4 > size) RB_PANIC("float read out of range");

    // TODO: Make this safe on big-endian systems!
    float val = *(float*)(data+offset);

    offset += 4;
    return val;
}

std::string read_str(
    const uint8_t* data, size_t size, size_t& offset, size_t alignment
){
    std::string str;

    while(offset < size && data[offset] != 0)
    {
        str += (char)data[offset];
        offset++;
    }
    offset++;

    if(offset > size) RB_PANIC("str read out of range");

    // Rounds to next multiple of alignment
    while(offset%alignment) offset++;
    return str;
}

}
