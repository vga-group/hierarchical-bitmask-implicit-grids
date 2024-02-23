#ifndef RAYBASE_UNIQUE_INDEX_TABLE_HH
#define RAYBASE_UNIQUE_INDEX_TABLE_HH
#include "flat_set.hh"

namespace rb
{

// Like a set, but faster (by an order of magnitude usually). Assigns
// consecutive indices to given keys and does so without excessive memory
// allocations.
template<typename Key>
using unique_index_table = flat_set<Key>;

}

#endif
