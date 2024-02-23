#ifndef RAYBASE_FLAT_SET_HH
#define RAYBASE_FLAT_SET_HH
#include "error.hh"
#include "stack_allocator.hh"
#include <memory>
#include <vector>

namespace rb
{

// Like a set, but faster (by an order of magnitude usually). Assigns
// consecutive indices to given keys and does so without excessive memory
// allocations. However, erase() will break this ordering, so if you intend to
// use this to generate unique unchanging indices, don't call that.
template<typename Key>
class flat_set
{
public:
    flat_set(int32_t size_hint = 8);
    flat_set(flat_set&& other) noexcept;
    flat_set(const flat_set& other);
    ~flat_set();

    flat_set& operator=(flat_set&& other) noexcept;
    flat_set& operator=(const flat_set& other);

    bool contains(const Key& key) const;
    int32_t get(const Key& key) const;
    int32_t insert(const Key& k);
    // Returns "iterator" to next entry after erase.
    Key* erase(const Key& k);
    void intersect(const flat_set<Key>& other);

    // Same as insert(), but returns negative index if key already existed.
    int32_t check_insert(const Key& k);

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
    void clear_keys();

    int32_t index_counter;
    std::size_t hashes_size;
    std::size_t alloc_size;
    int32_t* hashes;
    struct alignas(Key) key_mimicker { std::uint8_t pad[sizeof(Key)]; };
    key_mimicker* keys;
    int32_t* next;
    std::hash<Key> hasher;
};

}

#include "flat_set.tcc"
#endif

