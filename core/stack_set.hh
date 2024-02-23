#ifndef RAYBASE_STACK_SET_HH
#define RAYBASE_STACK_SET_HH
#include "error.hh"
#include "stack_allocator.hh"
#include <memory>
#include <vector>

namespace rb
{

// Like a set, but faster and uses stack_allocator. Hence, it cannot grow after
// creation, so make sure there is enough room when you create it! Use it for
// deduplication in a function. You may be slightly better off with
// unique_index_table if you can keep the object around. This one is better for
// a temporary variable.
template<typename Key>
class stack_set
{
public:
    stack_set(int32_t size);
    ~stack_set();

    bool contains(const Key& key) const;
    // Returns false if the key already existed, or if the stack_set ran out
    // of space.
    bool insert(const Key& k);

    Key& operator[](int32_t index);
    const Key& operator[](int32_t index) const;

    // Iterates keys in the order they were first added.
    Key* begin();
    Key* end();
    const Key* cbegin() const;
    const Key* cend() const;

    void clear();
    std::size_t size() const;

private:
    int32_t index_counter;
    stack_allocator::stack_proxy<int32_t> hashes;
    struct alignas(Key) key_mimicker { std::uint8_t pad[sizeof(Key)]; };
    stack_allocator::stack_proxy<key_mimicker> keys;
    stack_allocator::stack_proxy<int32_t> next;
    std::hash<Key> hasher;
};

}

#include "stack_set.tcc"
#endif
