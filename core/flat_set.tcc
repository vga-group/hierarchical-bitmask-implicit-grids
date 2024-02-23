#ifndef RAYBASE_FLAT_SET_TCC
#define RAYBASE_FLAT_SET_TCC
#include "flat_set.hh"
#include "math.hh"

namespace rb
{

template<typename Key>
flat_set<Key>::flat_set(int32_t size_hint)
{
    index_counter = 0;
    alloc_size = next_power_of_two(size_hint);
    hashes_size = 4 * alloc_size;
    hashes = new int32_t[hashes_size];
    keys = new key_mimicker[alloc_size];
    next = new int32_t[alloc_size];
    clear();
}

template<typename Key>
flat_set<Key>::flat_set(flat_set<Key>&& other) noexcept
:   index_counter(other.index_counter), hashes_size(other.hashes_size),
    alloc_size(other.alloc_size), hashes(other.hashes), keys(other.keys),
    next(other.next)
{
    other.hashes = nullptr;
    other.keys = nullptr;
    other.next = nullptr;
}

template<typename Key>
flat_set<Key>::flat_set(const flat_set& other)
:   index_counter(0), hashes_size(0), alloc_size(0), hashes(nullptr),
    keys(nullptr), next(nullptr)
{
    operator=(other);
}

template<typename Key>
flat_set<Key>::~flat_set()
{
    if(hashes) delete [] hashes;
    clear_keys();
    if(keys) delete [] keys;
    if(next) delete [] next;
}

template<typename Key>
flat_set<Key>& flat_set<Key>::operator=(flat_set<Key>&& other) noexcept
{
    if(hashes) delete [] hashes;
    clear_keys();
    if(keys) delete [] keys;
    if(next) delete [] next;
    index_counter = other.index_counter;
    hashes_size = other.hashes_size;
    alloc_size = other.alloc_size;
    hashes = other.hashes;
    keys = other.keys;
    next = other.next;
    other.hashes = nullptr;
    other.keys = nullptr;
    other.next = nullptr;
    return *this;
}

template<typename Key>
flat_set<Key>& flat_set<Key>::operator=(const flat_set& other)
{
    clear_keys();
    if(alloc_size < other.alloc_size || hashes_size < other.hashes_size)
    {
        if(hashes) delete [] hashes;
        if(keys) delete [] keys;
        if(next) delete [] next;
        hashes = new int32_t[other.hashes_size];
        keys = new key_mimicker[other.alloc_size];
        next = new int32_t[other.alloc_size];
    }
    index_counter = other.index_counter;
    hashes_size = other.hashes_size;
    alloc_size = other.alloc_size;
    memcpy(hashes, other.hashes, hashes_size*sizeof(int32_t));
    memcpy(next, other.next, index_counter*sizeof(int32_t));
    if constexpr(std::is_trivial_v<Key>)
        memcpy(keys, other.keys, index_counter*sizeof(Key));
    else
    {
        for(std::size_t i = 0; i < index_counter; ++i)
            new (reinterpret_cast<Key*>(keys+i)) Key(*reinterpret_cast<Key*>(other.keys+i));
    }
    return *this;
}

template<typename Key>
bool flat_set<Key>::contains(const Key& key) const
{
    return get(key) >= 0;
}

template<typename Key>
int32_t flat_set<Key>::get(const Key& k) const
{
    std::size_t hash = hasher(k)&(hashes_size-1);
    int32_t index = hashes[hash];
    while(index >= 0)
    {
        if(*reinterpret_cast<Key*>(keys+index) == k) return index;
        else index = next[index];
    }
    return index;
}

template<typename Key>
int32_t flat_set<Key>::insert(const Key& k)
{
    // Rehash if there's no space for insert.
    std::size_t hash = hasher(k) & (hashes_size - 1);
    int32_t* index = hashes + hash;
    while(*index >= 0)
    {
        if(*reinterpret_cast<Key*>(keys + *index) == k) return *index;
        else index = next + *index;
    }
    *index = index_counter++;
    uint32_t saved_index = *index;
    new (reinterpret_cast<Key*>(keys + saved_index)) Key(k);
    next[saved_index] = -1;

    if((std::size_t)index_counter == alloc_size)
    {
        std::size_t new_alloc_size = alloc_size * 2;
        std::size_t new_hashes_size = hashes_size * 2;

        int32_t* new_hashes = new int32_t[new_hashes_size];
        for(std::size_t i = 0; i < new_hashes_size; ++i)
            new_hashes[i] = -1;
        for(std::size_t i = 0; i < alloc_size; ++i)
        {
            std::size_t h = hasher(*reinterpret_cast<Key*>(keys+i)) & (new_hashes_size-1);
            if(new_hashes[h] < 0)
                new_hashes[h] = i;
        }
        delete [] hashes;
        hashes = new_hashes;

        int32_t* new_next = new int32_t[new_alloc_size];
        memcpy(new_next, next, alloc_size*sizeof(int32_t));
        delete [] next;
        next = new_next;

        key_mimicker* new_keys = new key_mimicker[new_alloc_size];
        if constexpr(std::is_trivial_v<Key>)
            memcpy(new_keys, keys, alloc_size*sizeof(Key));
        else
        {
            for(std::size_t i = 0; i < alloc_size; ++i)
            {
                new (reinterpret_cast<Key*>(new_keys+i)) Key(std::move(*reinterpret_cast<Key*>(keys+i)));
                reinterpret_cast<Key*>(keys+i)->~Key();
            }
        }
        delete [] keys;
        keys = new_keys;

        alloc_size = new_alloc_size;
        hashes_size = new_hashes_size;
    }
    return saved_index;
}

template<typename Key>
Key* flat_set<Key>::erase(const Key& k)
{
    // Remove old value
    std::size_t hash = hasher(k)&(hashes_size-1);
    int32_t* search_index = hashes + hash;
    while(*search_index >= 0)
    {
        if(*reinterpret_cast<Key*>(keys+*search_index) == k) break;
        else search_index = next + *search_index;
    }
    if(*search_index < 0) return end();
    int32_t i = *search_index;
    *search_index = next[i];

    // "Swap" with last value
    if(i != index_counter-1)
    {
        *reinterpret_cast<Key*>(keys+i) = std::move(
            *reinterpret_cast<Key*>(keys+index_counter-1)
        );
        reinterpret_cast<Key*>(keys+index_counter-1)->~Key();
        hash = hasher(*reinterpret_cast<Key*>(keys+i))&(hashes_size-1);
        search_index = hashes + hash;
        while(*search_index != index_counter-1)
            search_index = next + *search_index;
        *search_index = i;
        next[i] = next[index_counter-1];
    }
    index_counter--;
    return reinterpret_cast<Key*>(keys + i);
}

template<typename Key>
void flat_set<Key>::intersect(const flat_set<Key>& other)
{
    for(int32_t i = 0; i < index_counter;)
    {
        if(other.contains(*reinterpret_cast<Key*>(keys+i)))
        {
            ++i;
        }
        else
        {
            // Remove old value
            std::size_t hash = hasher(
                *reinterpret_cast<Key*>(keys+i)
            )&(hashes_size-1);
            int32_t* search_index = hashes + hash;
            while(*search_index != i)
                search_index = next + *search_index;
            *search_index = next[i];

            // "Swap" with last value
            if(i != index_counter-1)
            {
                *reinterpret_cast<Key*>(keys+i) = std::move(
                    *reinterpret_cast<Key*>(keys+index_counter-1)
                );
                reinterpret_cast<Key*>(keys+index_counter-1)->~Key();
                hash = hasher(*reinterpret_cast<Key*>(keys+i))&(hashes_size-1);
                search_index = hashes + hash;
                while(*search_index != index_counter-1)
                    search_index = next + *search_index;
                *search_index = i;
                next[i] = next[index_counter-1];
            }
            index_counter--;
        }
    }
}

template<typename Key>
int32_t flat_set<Key>::check_insert(const Key& k)
{
    int32_t prev_counter = index_counter;
    int32_t i = insert(k);
    return index_counter == prev_counter ? -i-1 : i;
}

template<typename Key>
Key& flat_set<Key>::operator[](int32_t index)
{
    return reinterpret_cast<Key*>(keys)[index];
}

template<typename Key>
const Key& flat_set<Key>::operator[](int32_t index) const
{
    return reinterpret_cast<const Key*>(keys)[index];
}

template<typename Key>
Key* flat_set<Key>::begin()
{
    return reinterpret_cast<Key*>(keys);
}

template<typename Key>
Key* flat_set<Key>::end()
{
    return reinterpret_cast<Key*>(keys) + index_counter;
}

template<typename Key>
const Key* flat_set<Key>::cbegin() const
{
    return reinterpret_cast<const Key*>(keys);
}

template<typename Key>
const Key* flat_set<Key>::cend() const
{
    return reinterpret_cast<const Key*>(keys) + index_counter;
}

template<typename Key>
void flat_set<Key>::clear()
{
    for(std::size_t i = 0; i < hashes_size; ++i)
        hashes[i] = -1;
    clear_keys();
}

template<typename Key>
std::size_t flat_set<Key>::size() const
{
    return index_counter;
}

template<typename Key>
void flat_set<Key>::clear_keys()
{
    for(int32_t i = 0; i < index_counter; ++i)
        reinterpret_cast<Key*>(keys+i)->~Key();
    index_counter = 0;
}

}
#endif

