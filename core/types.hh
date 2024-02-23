#ifndef RAYBASE_TYPES_HH
#define RAYBASE_TYPES_HH
#include <cstdint>
#include <type_traits>
#include <iterator>

// This whole thing could be removed with C++20 concepts, it's essentially doing
// the same thing.
#define RB_TRAIT(name, mock_call) \
    struct name \
    { \
        template<typename T, typename=void> \
        struct has: std::false_type { }; \
        template<typename T> \
        struct has<T, decltype((void)mock_call, void())> : std::true_type { }; \
        template<typename T> \
        static inline constexpr bool fulfill = has<T>::value; \
    }

namespace rb
{

// Defined in microseconds. Used when exact timing is needed in Raybase.
using time_ticks = int64_t;

RB_TRAIT(is_iterable, (std::begin(std::declval<T>()), std::end(std::declval<T>()), std::size(std::declval<T>())));

}

#endif
