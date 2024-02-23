#ifndef RAYBASE_BITSET_HH
#define RAYBASE_BITSET_HH
#include "error.hh"
#include <memory>
#include <vector>

namespace rb
{

// Bitset, but acts like a regular set with limited, integer-only values that
// must be between 0 and 63.
class bitset
{
public:
    template<typename... Args>
    bitset(Args... indices): bits(0) { (insert(indices), ...); }

    bool contains(unsigned value) const;
    void erase(unsigned value);
    void insert(unsigned value);

    struct iterator
    {
        uint64_t value;

        iterator& operator++();
        unsigned operator*() const;
        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;
    };

    // Iterates keys in the order they were first added.
    iterator begin() const;
    iterator end() const;
    iterator cbegin() const;
    iterator cend() const;

    void clear();
    std::size_t size() const;

private:
    uint64_t bits;
};

}

#endif
