#ifndef RAYBASE_ERROR_HH
#define RAYBASE_ERROR_HH
#include "log.hh"
#include <cstdlib>

#define RB_PANIC(...) { rb::log_message(__LINE__, __FILENAME__, __VA_ARGS__); abort(); }
#define RB_CHECK(condition, ...) [[unlikely]] if(condition) RB_PANIC(__VA_ARGS__);

#endif

