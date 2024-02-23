#ifndef RAYBASE_LOG_HH
#define RAYBASE_LOG_HH
#include "math.hh"
#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <typeinfo>
#include <sstream>

#ifndef RAYBASE_ROOT_PATH_SIZE
#define RAYBASE_ROOT_PATH_SIZE 0
#endif

#define __FILENAME__ ((const char*)((uintptr_t)__FILE__ + RAYBASE_ROOT_PATH_SIZE))

#ifdef RAYBASE_DEBUG
#define RB_DBG(...) rb::log_message(__LINE__, __FILENAME__, __VA_ARGS__)
#else
#define RB_DBG(...)
#endif

#define RB_LOG(...) rb::log_message(__LINE__, __FILENAME__, __VA_ARGS__)

namespace rb
{

std::chrono::system_clock::time_point get_initial_time();

template<typename T>
std::string to_string(const T& t)
{
    if constexpr(std::is_pointer_v<T>)
    {
        std::stringstream stream;
        stream << typeid(std::decay_t<std::remove_pointer_t<T>>).name() << "*(" << std::hex << (uintptr_t)t << ")";
        std::string result(stream.str());
        return stream.str();
    }
    else return std::to_string(t);
}
inline std::string to_string(const char* t) { return t; }
inline std::string to_string(const std::string& t) { return t; }
inline std::string to_string(const std::string_view& t) { return std::string(t.begin(), t.end()); }

template<glm::length_t L, typename T, glm::qualifier Q>
inline std::string to_string(const glm::vec<L, T, Q>& t) { return glm::to_string(t); }

template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline std::string to_string(const glm::mat<C, R, T, Q>& t) { return glm::to_string(t); }

inline std::string to_string(const aabb& t) { return "aabb{"+glm::to_string(t.min) + ", " +glm::to_string(t.max)+"}"; }


template<typename T>
std::string make_string(const T& t) { return to_string(t); }

template<typename T, typename... Args>
std::string make_string(
    const T& t,
    const Args&... rest
){ return to_string(t) + make_string(rest...); }

template<typename... Args>
void log_message(
    int line,
    const char* file,
    const Args&... rest
){
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();
    std::chrono::system_clock::duration d = now - get_initial_time();
    std::cout << "[" << std::fixed << std::setprecision(3) <<
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(d).count()/1000.0
        << "](" << file << ":" << line << ") "
        << make_string(rest...) << std::endl;
}

}

#endif
