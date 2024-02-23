#include "stack_allocator.hh"

namespace rb
{

stack_allocator::stack_allocator(size_t size)
{
    bottom = new uint8_t[size];
    top = bottom+size;
}

stack_allocator::~stack_allocator()
{
    delete [] bottom;
}

thread_local stack_allocator global_stack_allocator(1<<16);

}
