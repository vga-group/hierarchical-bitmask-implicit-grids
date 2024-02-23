#ifndef RAYBASE_INPUT_HH
#define RAYBASE_INPUT_HH
#include "math.hh"
#include "types.hh"
#include "sdl.hh"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <map>

namespace rb
{

// Returns true if the program should stop. The event callback can be used to 
// intercept SDL_Events before they're processed. It should return true to
// indicate that they should not be handled by input handlers.
template<typename... EventCallback>
bool poll_input(
    EventCallback&&... callback
){
    SDL_PumpEvents();

    bool quit = false;
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        if((callback(event) || ...))
            continue;

        if(event.type == SDL_QUIT)
        {
            quit = true;
            break;
        }
    }

    return quit;
}

}

#endif
