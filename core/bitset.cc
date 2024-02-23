#include "bitset.hh"
#include "math.hh"

namespace rb
{

bool bitset::contains(unsigned value) const
{
    return (bits >> value)&1;
}

void bitset::erase(unsigned value)
{
    bits &= ~(1llu << (uint64_t)value);
}

void bitset::insert(unsigned value)
{
    bits |= 1llu << (uint64_t)value;
}

bitset::iterator& bitset::iterator::operator++()
{
    value ^= (1llu << findLSB(value));
    return *this;
}

unsigned bitset::iterator::operator*() const
{
    return findLSB(value);
}

bool bitset::iterator::operator==(const iterator& other) const
{
    return value == other.value;
}

bool bitset::iterator::operator!=(const iterator& other) const
{
    return value != other.value;
}

bitset::iterator bitset::begin() const
{
    return {bits};
}

bitset::iterator bitset::end() const
{
    return {0};
}

bitset::iterator bitset::cbegin() const
{
    return {bits};
}

bitset::iterator bitset::cend() const
{
    return {0};
}

void bitset::clear()
{
    bits = 0;
}

std::size_t bitset::size() const
{
    return bitCount(bits);
}

}
