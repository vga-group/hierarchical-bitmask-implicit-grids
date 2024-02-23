#ifndef RAYBASE_SORT_HH
#define RAYBASE_SORT_HH
#include <cstdint>
#include <cstddef>

namespace rb
{

template<typename T>
void radix_sort(
    uint32_t count,
    T* key_ptr,
    T* key_scratch = nullptr
);

template<typename T, typename U, bool use_value = true>
void radix_sort(
    uint32_t count,
    T* key_ptr,
    U* value_ptr,
    T* key_scratch = nullptr,
    U* value_scratch = nullptr
);

template<typename T, typename F>
void radix_sort(
    uint32_t count,
    const T* in_ptr,
    T* out_ptr,
    F&& key
);

template<typename T, typename F>
void radix_argsort(
    uint32_t count,
    const T* in_ptr,
    uint32_t* out_indices,
    F&& key
);

template<typename T, typename F>
void radix_inverse_argsort(
    uint32_t count,
    const T* in_ptr,
    uint32_t* out_indices,
    F&& key
);

}

#include "sort.tcc"

#endif

