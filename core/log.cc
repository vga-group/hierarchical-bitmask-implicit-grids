#include "log.hh"

namespace rb
{
    std::chrono::system_clock::time_point get_initial_time()
    {
        static std::chrono::system_clock::time_point initial =
            std::chrono::system_clock::now();
        return initial;
    }
}
