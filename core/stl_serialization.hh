#ifndef RAYBASE_STL_SERIALIZATION_HH
#define RAYBASE_STL_SERIALIZATION_HH
#include "serialization.hh"
#include <array>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

namespace rb
{

template<typename A, typename T, std::size_t N>
void save(A& a, const std::array<T, N>& arr)
{
    for(std::size_t i = 0; i < N; ++i)
        a.serialize(arr[i]);
}

template<typename A, typename U>
void save_stl(A& a, const U& container)
{
    a.serialize(container.size());
    for(auto it = container.cbegin(); it != container.cend(); ++it)
        a.serialize(*it);
}

template<typename A, typename T, typename Traits>
void save(A& a, const std::basic_string_view<T, Traits>& container) { save_stl(a, container); }

template<typename A, typename T, typename Traits>
void save(A& a, const std::basic_string<T, Traits>& container) { save_stl(a, container); }

template<typename A, typename T>
void save(A& a, const std::vector<T>& container) { save_stl(a, container); }

template<typename A, typename K, typename T>
void save(A& a, const std::pair<K, T>& p) { a.serialize(p.first, p.second); }

template<typename A, typename K, typename T>
void save(A& a, const std::map<K, T>& container) { save_stl(a, container); }

template<typename A, typename K, typename T>
void save(A& a, const std::unordered_map<K, T>& container) { save_stl(a, container); }

template<typename A, typename T>
void save(A& a, const std::set<T>& container){ save_stl(a, container); }

template<typename A, typename T>
void save(A& a, const std::unordered_set<T>& container) { save_stl(a, container); }

template<typename A, typename T, std::size_t N>
bool load(A& a, std::array<T, N>& arr)
{
    for(std::size_t i = 0; i < N; ++i)
        if(!a.serialize(arr[i])) return false;
    return true;
}

template<typename A, typename U>
bool load_stl(A& a, U& container)
{
    container.clear();
    std::size_t size = 0;
    if(!a.serialize(size)) return false;

    auto ins = std::inserter(container, container.begin());
    for(std::size_t i = 0; i < size; ++i, ++ins)
    {
        std::remove_cv_t<std::remove_reference_t<decltype(*container.begin())>> entry;
        if(!a.serialize(entry))
            return false;
        *ins = entry;
    }
    return true;
}

template<typename A, typename U>
bool load_stl_map(A& a, U& container)
{
    container.clear();
    std::size_t size = 0;
    if(!a.serialize(size)) return false;

    auto ins = std::inserter(container, container.begin());
    for(std::size_t i = 0; i < size; ++i, ++ins)
    {
        typename U::key_type key;
        typename U::mapped_type value;
        if(!a.serialize(key, value))
            return false;
        *ins = {key, value};
    }
    return true;
}

template<typename A, typename T, typename Traits>
bool load(A& a, std::basic_string<T, Traits>& container)
{ return load_stl(a, container); }

template<typename A, typename T>
bool load(A& a, std::vector<T>& container)
{ return load_stl(a, container); }

template<typename A, typename K, typename T>
bool load(A& a, std::pair<K, T>& p) { return a.serialize(p.first, p.second); }

template<typename A, typename K, typename T>
bool load(A& a, std::map<K, T>& container)
{ return load_stl_map(a, container); }

template<typename A, typename K, typename T>
bool load(A& a, std::unordered_map<K, T>& container)
{ return load_stl_map(a, container); }

template<typename A, typename T>
bool load(A& a, std::set<T>& container)
{ return load_stl(a, container); }

template<typename A, typename T>
bool load(A& a, std::unordered_set<T>& container)
{ return load_stl(a, container); }

}

#endif

