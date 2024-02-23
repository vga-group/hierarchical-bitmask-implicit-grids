#ifndef RAYBASE_SORT_TCC
#define RAYBASE_SORT_TCC
#include "sort.hh"
#include <memory>
#include <numeric>
#include <cstring>

namespace rb
{

namespace radix_internal
{
inline constexpr size_t RADIX_PASS_BITS = 8;
inline constexpr size_t RADIX_PASS_MASK = (1<<RADIX_PASS_BITS)-1;
template<typename T>
struct placeholder_type_helper {};
#define placeholder_type(from, to) \
template<> struct placeholder_type_helper<from> { \
    using type = to; \
};

placeholder_type(uint8_t, uint8_t);
placeholder_type(int8_t, uint8_t);
placeholder_type(char, uint8_t);
placeholder_type(uint16_t, uint16_t);
placeholder_type(int16_t, uint16_t);
placeholder_type(uint32_t, uint32_t);
placeholder_type(int32_t, uint32_t);
placeholder_type(float, uint32_t);
placeholder_type(uint64_t, uint64_t);
placeholder_type(int64_t, uint64_t);
placeholder_type(double, uint64_t);
template<typename T> struct placeholder_type_helper<T*> { using type = std::uintptr_t; };
#undef placeholder_type

template<typename T>
using placeholder_type = typename placeholder_type_helper<T>::type;

template<typename T>
void to_sortable(placeholder_type<T>& placeholder)
{
    constexpr auto msb = placeholder_type<T>(1)<<(sizeof(placeholder)*8-1);
    if constexpr(std::is_floating_point_v<T>)
    {
        // Assumes IEEE.754 format!
        if(placeholder & msb)
            placeholder = ~placeholder;
        else placeholder ^= msb;
    }
    else if constexpr(std::is_signed_v<T>)
    {
        placeholder += msb;
    }
}

template<typename T>
void from_sortable(placeholder_type<T>& placeholder)
{
    constexpr auto msb = placeholder_type<T>(1)<<(sizeof(placeholder)*8-1);
    if constexpr(std::is_floating_point_v<T>)
    {
        // Assumes IEEE.754 format!
        if(placeholder & msb)
            placeholder ^= msb;
        else placeholder = ~placeholder;
    }
    else if constexpr(std::is_signed_v<T>)
    {
        placeholder -= msb;
    }
}
}

template<typename T>
void radix_sort(
    uint32_t count,
    T* key_ptr,
    T* key_scratch
){
    radix_sort<T, int, false>(count, key_ptr, nullptr, key_scratch, nullptr);
}

template<typename T, typename U, bool use_value>
void radix_sort(
    uint32_t count,
    T* key_ptr,
    U* value_ptr,
    T* key_scratch,
    U* value_scratch
){
    using namespace radix_internal;
    constexpr size_t pass_count = (sizeof(T) * 8 + RADIX_PASS_BITS - 1) / RADIX_PASS_BITS;
    uint32_t histogram[1<<RADIX_PASS_BITS];
    uint32_t cumulative[1<<RADIX_PASS_BITS];

    std::unique_ptr<T[]> local_key_scratch;
    if(!key_scratch)
    {
        local_key_scratch.reset(new T[count]);
        key_scratch = local_key_scratch.get();
    }
    std::unique_ptr<U[]> local_value_scratch;
    if(use_value && !value_scratch)
    {
        local_value_scratch.reset(new U[count]);
        value_scratch = local_value_scratch.get();
    }

    placeholder_type<T>* in_key =
        reinterpret_cast<placeholder_type<T>*>(key_ptr);
    placeholder_type<T>* out_key =
        reinterpret_cast<placeholder_type<T>*>(key_scratch);

    U* in_value = value_ptr;
    U* out_value = value_scratch;

    std::memset(histogram, 0, sizeof(histogram));
    for(size_t i = 0; i < count; ++i)
    {
        placeholder_type<T> k;
        memcpy(&k, &key_ptr[i], sizeof(T));
        to_sortable<T>(k);
        in_key[i] = k;
        histogram[in_key[i] & RADIX_PASS_MASK]++;
    }

    std::exclusive_scan(std::begin(histogram), std::end(histogram), cumulative, 0);

    for(size_t pass = 1; pass < pass_count; ++pass)
    {
        std::memset(histogram, 0, sizeof(histogram));
        for(size_t i = 0; i < count; ++i)
        {
            placeholder_type<T> k = in_key[i];
            uint32_t category = (k >> ((pass-1)*RADIX_PASS_BITS)) & RADIX_PASS_MASK;
            uint32_t index = cumulative[category]++;
            out_key[index] = k;
            if constexpr(use_value)
                out_value[index] = std::move(in_value[i]);
            histogram[(k >> (pass*RADIX_PASS_BITS)) & RADIX_PASS_MASK]++;
        }
        std::swap(in_key, out_key);
        if constexpr(use_value)
            std::swap(in_value, out_value);

        std::exclusive_scan(std::begin(histogram), std::end(histogram), cumulative, 0);
    }

    if constexpr(pass_count & 1)
    { // Odd (out_key != key_ptr)
        for(size_t i = 0; i < count; ++i)
        {
            placeholder_type<T> k = in_key[i];
            uint32_t category = (in_key[i] >> ((pass_count-1)*RADIX_PASS_BITS)) & RADIX_PASS_MASK;
            uint32_t index = cumulative[category]++;
            out_key[index] = in_key[i];
            if constexpr(use_value)
                out_value[index] = std::move(in_value[i]);
        }

        for(size_t i = 0; i < count; ++i)
        {
            placeholder_type<T> t = out_key[i];
            from_sortable<T>(t);
            memcpy(&key_ptr[i], &t, sizeof(T));
        }
        if constexpr(use_value)
        {
            if(out_value != value_ptr)
                memcpy(value_ptr, out_value, count*sizeof(U));
        }
    }
    else
    { // Even (out_key == key_ptr)
        for(size_t i = 0; i < count; ++i)
        {
            placeholder_type<T> k = in_key[i];
            uint32_t category = (k >> ((pass_count-1)*RADIX_PASS_BITS)) & RADIX_PASS_MASK;
            uint32_t index = cumulative[category]++;
            from_sortable<T>(k);
            memcpy(&key_ptr[index], &k, sizeof(T));
            if constexpr(use_value)
                value_ptr[index] = std::move(in_value[i]);
        }
    }
}

template<typename T, typename F>
void radix_sort(
    uint32_t count,
    const T* in_ptr,
    T* out_ptr,
    F&& key
){
    using key_type = decltype(key(std::declval<T>()));
    std::unique_ptr<key_type[]> keys(new key_type[2lu*count]);
    std::unique_ptr<uint32_t[]> indices(new uint32_t[2lu*count]);
    std::iota(indices.get(), indices.get()+count, 0);

    for(uint32_t i = 0; i < count; ++i)
        keys[i] = key(in_ptr[i]);

    radix_sort(
        count,
        keys.get(),
        indices.get(),
        keys.get()+count,
        indices.get()+count
    );

    for(uint32_t i = 0; i < count; ++i)
        out_ptr[i] = in_ptr[indices[i]];
}

template<typename T, typename F>
void radix_argsort(
    uint32_t count,
    const T* in_ptr,
    uint32_t* out_indices,
    F&& key
){
    using key_type = decltype(key(std::declval<T>()));
    std::unique_ptr<key_type[]> keys(new key_type[2lu*count]);
    std::iota(out_indices, out_indices+count, 0);
    for(uint32_t i = 0; i < count; ++i)
        keys[i] = key(in_ptr[i]);

    radix_sort(count, keys.get(), out_indices, keys.get()+count, (uint32_t*)nullptr);
}

template<typename T, typename F>
void radix_inverse_argsort(
    uint32_t count,
    const T* in_ptr,
    uint32_t* out_indices,
    F&& key
){
    using key_type = decltype(key(std::declval<T>()));
    std::unique_ptr<key_type[]> keys(new key_type[2lu*count]);
    std::unique_ptr<uint32_t[]> indices(new uint32_t[2lu*count]);
    std::iota(indices.get(), indices.get()+count, 0);
    for(uint32_t i = 0; i < count; ++i)
        keys[i] = key(in_ptr[i]);

    radix_sort(count, keys.get(), indices.get(), keys.get()+count, indices.get()+count);

    for(uint32_t i = 0; i < count; ++i)
        out_indices[indices[i]] = i;
}

}

#endif

