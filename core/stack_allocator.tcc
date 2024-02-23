#ifndef RAYBASE_STACK_ALLOCATOR_TCC
#define RAYBASE_STACK_ALLOCATOR_TCC
#include "stack_allocator.hh"

namespace rb
{

template<typename T>
void stack_allocator::stack_deleter<T>::operator()(T* ptr)
{
    if(allocator)
    {
        for(size_t i = 0; i < count; ++i)
            ptr[i].~T();
        allocator->top += bytes;
    }
    else delete [] ptr;
}


template<typename T>
size_t stack_allocator::stack_proxy<T>::size() const
{
    return get_deleter().count;
}

template<typename T>
T* stack_allocator::stack_proxy<T>::begin() const
{
    return get();
}

template<typename T>
T* stack_allocator::stack_proxy<T>::end() const
{
    return get() + get_deleter().count;
}

template<typename T>
T* stack_allocator::stack_proxy<T>::data() const
{
    return get();
}

template<typename T>
stack_allocator::stack_proxy<T> stack_allocator::allocate(size_t count)
{
    constexpr size_t alignment = alignof(T);

    uint8_t* old_top = top;
    top -= count * sizeof(T);
    top = (uint8_t*)(((uintptr_t)top)&~(alignment-1));
    size_t bytes = old_top - top;

    if(top < bottom || old_top < top)
    {
        top = old_top;
        return stack_proxy<T>(
            new T[count](), stack_deleter<T>{nullptr, count, 0}
        );
    }

    T* ptr = reinterpret_cast<T*>(top);
    if constexpr(!std::is_trivial_v<T>)
    {
        for(size_t i = 0; i < count; ++i)
            new (ptr+i) T();
    }
    return stack_proxy<T>(
        ptr, stack_deleter<T>{this, count, bytes}
    );
}

template<typename T>
stack_allocator::stack_proxy<T> stack_allocate(size_t count)
{
    return global_stack_allocator.allocate<T>(count);
}

}

#endif
