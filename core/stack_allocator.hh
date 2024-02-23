#ifndef RAYBASE_STACK_ALLOCATOR_HH
#define RAYBASE_STACK_ALLOCATOR_HH
#include "error.hh"
#include <memory>
#include <vector>

namespace rb
{

class stack_allocator
{
public:
    stack_allocator(size_t size);
    ~stack_allocator();

    template<typename T>
    struct stack_deleter
    {
        stack_allocator* allocator;
        size_t count, bytes;

        void operator()(T* ptr);
    };

    // DON'T SAVE THIS IN A STRUCT, ONLY USE AS A LOCAL VARIABLE!
    template<typename T>
    struct stack_proxy: std::unique_ptr<T[], stack_deleter<T>>
    {
        using std::unique_ptr<T[], stack_deleter<T>>::get;
        using std::unique_ptr<T[], stack_deleter<T>>::get_deleter;

        // Prevent abuse of the stack proxy: you can't move it out-of-stack,
        // hopefully.
        stack_proxy() = delete;
        stack_proxy(const stack_proxy& other) = delete;
        stack_proxy(stack_proxy&& other) = delete;

        stack_proxy& operator=(const stack_proxy& other) = delete;
        stack_proxy& operator=(stack_proxy&& other) = delete;

        size_t size() const;
        T* begin() const;
        T* end() const;
        T* data() const;

    private:
        friend class stack_allocator;
        using std::unique_ptr<T[], stack_deleter<T>>::unique_ptr;
    };

    // DON'T SAVE THE RESULT IN A STRUCT, ONLY USE AS A LOCAL VARIABLE!
    template<typename T>
    inline stack_proxy<T> allocate(size_t count);

private:
    uint8_t* top;
    uint8_t* bottom;
};

extern thread_local stack_allocator global_stack_allocator;

// DON'T SAVE THE RESULT IN A STRUCT, ONLY USE AS A LOCAL VARIABLE!
template<typename T>
stack_allocator::stack_proxy<T> stack_allocate(size_t count);

}

#include "stack_allocator.tcc"
#endif
