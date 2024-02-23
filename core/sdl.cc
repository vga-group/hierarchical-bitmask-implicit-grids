#include "sdl.hh"
#include "error.hh"

namespace rb
{

size_t sdl_requirement::init_count = 0;

sdl_requirement::sdl_requirement(uint32_t flags)
: flags(flags)
{
    init(flags);
}

sdl_requirement::sdl_requirement(const sdl_requirement& other)
: flags(other.flags)
{
    init(other.flags);
}

sdl_requirement::~sdl_requirement()
{
    deinit(flags);
}

void sdl_requirement::init(uint32_t flags)
{
    if(SDL_InitSubSystem(flags))
        RB_PANIC("SDL_InitSubSystem: ", SDL_GetError());
    init_count++;
}

void sdl_requirement::deinit(uint32_t flags)
{
    if(init_count > 0)
    {
        SDL_QuitSubSystem(flags);
        init_count--;
        if(init_count == 0)
            SDL_Quit();
    }
}

}
