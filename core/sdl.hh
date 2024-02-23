#ifndef RAYBASE_CORE_SDL_HH
#define RAYBASE_CORE_SDL_HH
#include <string>
#include <SDL2/SDL.h>

namespace rb
{

class sdl_requirement
{
public:
    sdl_requirement(uint32_t flags);
    sdl_requirement(const sdl_requirement& other);
    ~sdl_requirement();

private:
    static void init(uint32_t flags);
    static void deinit(uint32_t flags);

    uint32_t flags = 0;
    static size_t init_count;
};

}

#endif


