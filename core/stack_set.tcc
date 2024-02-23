#ifndef RAYBASE_STACK_SET_TCC
#define RAYBASE_STACK_SET_TCC
#include "stack_set.hh"
#include "math.hh"

namespace rb
{

template<typename Key>
stack_set<Key>::stack_set(int32_t size)
: hashes(global_stack_allocator.allocate<int32_t>(4 * next_power_of_two(size))),
  keys(global_stack_allocator.allocate<key_mimicker>(size)),
  next(global_stack_allocator.allocate<int32_t>(size))
{
    index_counter = 0;
    clear();
}

template<typename Key>
stack_set<Key>::~stack_set()
{
    for(std::size_t i = 0; i < index_counter; ++i)
        reinterpret_cast<Key*>(&keys[i])->~Key();
}

template<typename Key>
bool stack_set<Key>::contains(const Key& key) const
{
    return get(key) >= 0;
}

template<typename Key>
bool stack_set<Key>::insert(const Key& k)
{
    // Rehash if there's no space for insert.
    std::size_t hash = hasher(k) & (hashes.size() - 1);
    int32_t* index = &hashes[hash];
    while(*index >= 0)
    {
        if(*reinterpret_cast<Key*>(&keys[*index]) == k) return false;
        else index = &next[*index];
    }

    if(index_counter == keys.size())
        return false;

    *index = index_counter++;
    new (reinterpret_cast<Key*>(&keys[*index])) Key(k);
    next[*index] = -1;

    return true;
}

template<typename Key>
Key& stack_set<Key>::operator[](int32_t index)
{
    return reinterpret_cast<Key*>(keys.get())[index];
}

template<typename Key>
const Key& stack_set<Key>::operator[](int32_t index) const
{
    return reinterpret_cast<const Key*>(keys.get())[index];
}

template<typename Key>
Key* stack_set<Key>::begin()
{
    return reinterpret_cast<Key*>(keys.get());
}

template<typename Key>
Key* stack_set<Key>::end()
{
    return reinterpret_cast<Key*>(keys.get()) + index_counter;
}

template<typename Key>
const Key* stack_set<Key>::cbegin() const
{
    return reinterpret_cast<const Key*>(keys.get());
}

template<typename Key>
const Key* stack_set<Key>::cend() const
{
    return reinterpret_cast<const Key*>(keys.get()) + index_counter;
}

template<typename Key>
void stack_set<Key>::clear()
{
    for(std::size_t i = 0; i < hashes.size(); ++i)
        hashes[i] = -1;

    for(std::size_t i = 0; i < index_counter; ++i)
        reinterpret_cast<Key*>(&keys[i])->~Key();

    index_counter = 0;
}

template<typename Key>
std::size_t stack_set<Key>::size() const
{
    return index_counter;
}

}
#endif
